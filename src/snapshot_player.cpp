#include "snapshot_player.h"
#include "config.h"
#include "network_manager.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_heap_caps.h>

// STB 图像库配置：所有解码堆内存全部在 8MB PSRAM 中分配，零 SRAM 消耗
#define STBI_MALLOC(sz)        heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define STBI_REALLOC(p, sz)    heap_caps_realloc(p, sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define STBI_FREE(p)           free(p)
#define STBI_ONLY_JPEG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

SnapshotPlayer snapshot_player;

SnapshotPlayer::SnapshotPlayer()
    : current_state(SNAP_STATE_IDLE),
      new_frame_flag(false),
      has_decoded_frame(false),
      frame_buffer(nullptr),
      buffer_size(0),
      buffer_capacity(SNAPSHOT_MAX_BUF_SIZE),
      canvas_preview(&M5.Display),
      canvas_fullscreen(&M5.Display),
      active_place(nullptr),
      last_fetch_ms(0),
      fetch_start_ms(0),
      task_running(false),
      buffer_mutex(nullptr)
{
    current_url[0] = '\0';
}

SnapshotPlayer::~SnapshotPlayer() {
    stop();
    if (frame_buffer) {
        free(frame_buffer);
        frame_buffer = nullptr;
    }
    if (buffer_mutex) {
        vSemaphoreDelete(buffer_mutex);
        buffer_mutex = nullptr;
    }
}

void SnapshotPlayer::init() {
    if (!buffer_mutex) {
        buffer_mutex = xSemaphoreCreateMutex();
    }

    // 在 PSRAM 中分配高速图片接收缓冲
    if (!frame_buffer) {
        frame_buffer = (uint8_t*)heap_caps_malloc(buffer_capacity, MALLOC_CAP_SPIRAM);
        if (!frame_buffer) {
            Serial.println("[ERROR] SnapshotPlayer: Failed to allocate PSRAM buffer! Fallback to internal heap.");
            frame_buffer = (uint8_t*)malloc(32 * 1024);
            buffer_capacity = 32 * 1024;
        } else {
            Serial.printf("[SnapshotPlayer] Allocated %u KB PSRAM stream buffer.\n", (uint32_t)(buffer_capacity / 1024));
        }
    }

    // 在 PSRAM 中预先创建好预览视窗 (240x135) 和全屏视窗 (406x228) 两个 Canvas 帧缓存
    // 两个 Canvas 合计仅占约 250 KB PSRAM，换取 60FPS 零卡顿的瞬时渲染与 100% 正确的色彩字节序
    if (!canvas_preview.getBuffer()) {
        canvas_preview.setColorDepth(16);
        canvas_preview.setPsram(true);
        canvas_preview.createSprite(240, 135);
        canvas_preview.fillScreen(0x0842);
    }

    if (!canvas_fullscreen.getBuffer()) {
        canvas_fullscreen.setColorDepth(16);
        canvas_fullscreen.setPsram(true);
        canvas_fullscreen.createSprite(406, 228);
        canvas_fullscreen.fillScreen(0x0821);
    }
}

void SnapshotPlayer::loadPlace(const PlaceLocation* place, bool force_refresh) {
    if (!place) return;

    bool is_new_place = (active_place != place);
    active_place = place;

    if (is_new_place) {
        has_decoded_frame = false;
        if (canvas_preview.getBuffer()) canvas_preview.fillScreen(0x0842);
        if (canvas_fullscreen.getBuffer()) canvas_fullscreen.fillScreen(0x0821);
    }

    if (!place->snapshot_url || strlen(place->snapshot_url) == 0) {
        current_state = SNAP_STATE_IDLE;
        return;
    }

    uint32_t now = millis();
    if (is_new_place || force_refresh || (now - last_fetch_ms > SNAPSHOT_REFRESH_INTERVAL_MS)) {
        if (!task_running) {
            strncpy(current_url, place->snapshot_url, sizeof(current_url) - 1);
            current_url[sizeof(current_url) - 1] = '\0';
            startAsyncFetch(current_url);
        }
    }
}

void SnapshotPlayer::startAsyncFetch(const char* url) {
    if (!network_mgr.isConnected()) {
        current_state = SNAP_STATE_ERROR;
        return;
    }

    if (task_running) return;

    task_running = true;
    current_state = SNAP_STATE_FETCHING;
    fetch_start_ms = millis();

    // 启动 FreeRTOS 异步拉取任务在 Core 0 执行，完全不占用主线程 UI 渲染
    xTaskCreatePinnedToCore(
        fetchTaskStatic,
        "snap_fetch",
        8192,
        this,
        1,
        nullptr,
        0
    );
}

void SnapshotPlayer::fetchTaskStatic(void* param) {
    SnapshotPlayer* self = static_cast<SnapshotPlayer*>(param);
    if (self) {
        self->fetchTaskWorker();
    }
    vTaskDelete(nullptr);
}

void SnapshotPlayer::fetchTaskWorker() {
    Serial.printf("[SnapshotPlayer] Fetching frame from: %s\n", current_url);

    HTTPClient http;
    WiFiClientSecure secure_client;
    WiFiClient plain_client;

    bool is_https = (strncmp(current_url, "https://", 8) == 0);
    bool success = false;
    size_t downloaded_bytes = 0;

    if (is_https) {
        secure_client.setInsecure(); // 嵌入式端忽略证书链校验以节省昂贵的 RAM
        secure_client.setTimeout(SNAPSHOT_HTTP_TIMEOUT_MS / 1000);
        http.begin(secure_client, current_url);
    } else {
        plain_client.setTimeout(SNAPSHOT_HTTP_TIMEOUT_MS / 1000);
        http.begin(plain_client, current_url);
    }

    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setReuse(false);
    http.setTimeout(SNAPSHOT_HTTP_TIMEOUT_MS);
    http.addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    http.addHeader("Accept", "image/jpeg,image/*,*/*");

    int http_code = http.GET();
    if (http_code == HTTP_CODE_OK) {
        int content_len = http.getSize();
        WiFiClient* stream = http.getStreamPtr();

        if (stream) {
            if (xSemaphoreTake(buffer_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                buffer_size = 0;
                uint8_t temp_chunk[1024];
                uint32_t read_start = millis();

                while (http.connected() && (content_len > 0 || content_len == -1)) {
                    size_t avail = stream->available();
                    if (avail > 0) {
                        size_t to_read = (avail > sizeof(temp_chunk)) ? sizeof(temp_chunk) : avail;
                        int bytes_read = stream->read(temp_chunk, to_read);
                        if (bytes_read > 0) {
                            if (buffer_size + bytes_read <= buffer_capacity) {
                                memcpy(frame_buffer + buffer_size, temp_chunk, bytes_read);
                                buffer_size += bytes_read;
                            } else {
                                Serial.println("[SnapshotPlayer] [WARN] Image exceeds buffer capacity!");
                                break;
                            }
                        }
                        read_start = millis();
                    } else {
                        if (millis() - read_start > 3000) break; // 超时退出
                        vTaskDelay(pdMS_TO_TICKS(10));
                    }

                    if (content_len > 0 && (int)buffer_size >= content_len) break;
                }

                downloaded_bytes = buffer_size;
                xSemaphoreGive(buffer_mutex);

                // 验证 JPEG 幻数头 (SOI: 0xFF 0xD8)
                if (downloaded_bytes > 4 && frame_buffer[0] == 0xFF && frame_buffer[1] == 0xD8) {
                    success = true;
                    Serial.printf("[SnapshotPlayer] [OK] Downloaded %u KB JPEG in %u ms\n",
                                  (uint32_t)(downloaded_bytes / 1024),
                                  (uint32_t)(millis() - fetch_start_ms));
                } else {
                    Serial.printf("[SnapshotPlayer] [WARN] Invalid JPEG header (size=%u, byte0=0x%02X)\n",
                                  (uint32_t)downloaded_bytes, frame_buffer ? frame_buffer[0] : 0);
                }
            }
        }
    } else {
        Serial.printf("[SnapshotPlayer] [FAIL] HTTP GET failed, code: %d\n", http_code);
    }

    http.end();

    if (success) {
        current_state = SNAP_STATE_READY;
        last_fetch_ms = millis();
        new_frame_flag = true;
    } else {
        current_state = SNAP_STATE_ERROR;
    }

    task_running = false;
}

// ----------------------------------------------------------------------
// 异步预解码管线：当下载完新 JPEG 后仅解码一次，将像素高速转存至 PSRAM Canvas
// 彻底解决主线程每帧重复 STB 解码导致的卡顿、掉帧与崩溃问题
// ----------------------------------------------------------------------
void SnapshotPlayer::decodeAndCacheFrame(M5GFX& display) {
    if (!frame_buffer || buffer_size == 0) return;

    if (xSemaphoreTake(buffer_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // 1. 尝试硬件标准基线 Baseline JPEG 解码
        bool ok = canvas_fullscreen.drawJpg(frame_buffer, buffer_size, 0, 0, 406, 228);
        if (ok) {
            canvas_preview.drawJpg(frame_buffer, buffer_size, 0, 0, 240, 135);
            has_decoded_frame = true;
            new_frame_flag = false;
            Serial.println("[SnapshotPlayer] Hardware Baseline JPEG cached to Canvas.");
        } else {
            // 2. 硬件不支持（如 Progressive JPEG），使用 PSRAM STB 全功能解码器
            int img_w = 0, img_h = 0, channels = 0;
            uint8_t* rgb = stbi_load_from_memory(frame_buffer, (int)buffer_size, &img_w, &img_h, &channels, 3);
            if (rgb && img_w > 0 && img_h > 0) {
                // 写入全屏视窗 Canvas (406x228)
                canvas_fullscreen.startWrite();
                for (int dy = 0; dy < 228; ++dy) {
                    int sy = (dy * img_h) / 228;
                    const uint8_t* row = rgb + (sy * img_w * 3);
                    for (int dx = 0; dx < 406; ++dx) {
                        int sx = (dx * img_w) / 406;
                        const uint8_t* p = row + (sx * 3);
                        canvas_fullscreen.writePixel(dx, dy, canvas_fullscreen.color565(p[0], p[1], p[2]));
                    }
                }
                canvas_fullscreen.endWrite();

                // 写入预览视窗 Canvas (240x135)
                canvas_preview.startWrite();
                for (int dy = 0; dy < 135; ++dy) {
                    int sy = (dy * img_h) / 135;
                    const uint8_t* row = rgb + (sy * img_w * 3);
                    for (int dx = 0; dx < 240; ++dx) {
                        int sx = (dx * img_w) / 240;
                        const uint8_t* p = row + (sx * 3);
                        canvas_preview.writePixel(dx, dy, canvas_preview.color565(p[0], p[1], p[2]));
                    }
                }
                canvas_preview.endWrite();

                stbi_image_free(rgb);
                has_decoded_frame = true;
                new_frame_flag = false;
                Serial.printf("[SnapshotPlayer] Progressive JPEG pre-decoded (%dx%d) into PSRAM Canvas!\n", img_w, img_h);
            } else {
                Serial.println("[SnapshotPlayer] [FAIL] STB decode failed!");
            }
        }
        xSemaphoreGive(buffer_mutex);
    }
}

void SnapshotPlayer::renderPreview(M5GFX& display, int16_t x, int16_t y, int16_t w, int16_t h) {
    if (new_frame_flag || !has_decoded_frame) {
        if (hasValidFrame()) {
            decodeAndCacheFrame(display);
        }
    }

    if (has_decoded_frame && canvas_preview.getBuffer()) {
        canvas_preview.pushSprite(&display, x, y);
    } else {
        display.fillRect(x, y, w, h, 0x0842);
    }
}

void SnapshotPlayer::renderFullscreen(M5GFX& display, int16_t x, int16_t y, int16_t w, int16_t h) {
    if (new_frame_flag || !has_decoded_frame) {
        if (hasValidFrame()) {
            decodeAndCacheFrame(display);
        }
    }

    if (has_decoded_frame && canvas_fullscreen.getBuffer()) {
        canvas_fullscreen.pushSprite(&display, x, y);
    } else {
        display.fillRect(x, y, w, h, 0x0821);
    }
}

void SnapshotPlayer::update(uint32_t now_ms) {
    // 定时轮询与自动重刷新逻辑
    if (active_place && active_place->snapshot_url && strlen(active_place->snapshot_url) > 0) {
        if (current_state == SNAP_STATE_READY && (now_ms - last_fetch_ms > SNAPSHOT_REFRESH_INTERVAL_MS)) {
            loadPlace(active_place, true);
        }
    }
}

void SnapshotPlayer::stop() {
    current_state = SNAP_STATE_IDLE;
    active_place = nullptr;
    has_decoded_frame = false;
}
