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

// 高性能轻量重采样器：将解码出的 RGB888 图像缩放并转换推送到屏幕目标视口
static void renderDecodedRGB888(M5GFX& display, const uint8_t* rgb, int src_w, int src_h, int dst_x, int dst_y, int dst_w, int dst_h) {
    if (!rgb || src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) return;

    // 仅分配一行目标宽度的行像素缓冲区 (dst_w 个 uint16_t RGB565，约几百字节)
    uint16_t* line_buf = (uint16_t*)heap_caps_malloc(dst_w * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_INTERNAL);
    if (!line_buf) return;

    display.startWrite();
    for (int dy = 0; dy < dst_h; ++dy) {
        int sy = (dy * src_h) / dst_h;
        const uint8_t* src_row = rgb + (sy * src_w * 3);

        for (int dx = 0; dx < dst_w; ++dx) {
            int sx = (dx * src_w) / dst_w;
            const uint8_t* px = src_row + (sx * 3);
            uint16_t r = px[0] >> 3;
            uint16_t g = px[1] >> 2;
            uint16_t b = px[2] >> 3;
            line_buf[dx] = (r << 11) | (g << 5) | b;
        }
        display.pushImage(dst_x, dst_y + dy, dst_w, 1, line_buf);
    }
    display.endWrite();

    free(line_buf);
}

SnapshotPlayer snapshot_player;

SnapshotPlayer::SnapshotPlayer()
    : current_state(SNAP_STATE_IDLE),
      new_frame_flag(false),
      frame_buffer(nullptr),
      buffer_size(0),
      buffer_capacity(SNAPSHOT_MAX_BUF_SIZE),
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
}

void SnapshotPlayer::loadPlace(const PlaceLocation* place, bool force_refresh) {
    if (!place) return;

    bool is_new_place = (active_place != place);
    active_place = place;

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

void SnapshotPlayer::renderPreview(M5GFX& display, int16_t x, int16_t y, int16_t w, int16_t h) {
    if (hasValidFrame()) {
        if (xSemaphoreTake(buffer_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            // 1. 优先尝试 M5GFX 硬件高速解码 (Baseline JPEG)
            bool ok = display.drawJpg(frame_buffer, buffer_size, x, y, w, h);
            if (!ok) {
                // 2. 若硬件解码不支持 Progressive JPEG，自动启用 PSRAM STB 全能解码器
                int img_w = 0, img_h = 0, channels = 0;
                uint8_t* rgb = stbi_load_from_memory(frame_buffer, (int)buffer_size, &img_w, &img_h, &channels, 3);
                if (rgb) {
                    renderDecodedRGB888(display, rgb, img_w, img_h, x, y, w, h);
                    stbi_image_free(rgb);
                    ok = true;
                    Serial.printf("[SnapshotPlayer] Progressive JPEG decoded with STB: %dx%d -> %dx%d\n", img_w, img_h, w, h);
                }
            }
            if (!ok) {
                display.fillRect(x, y, w, h, 0x0842);
            }
            xSemaphoreGive(buffer_mutex);
        }
    }
}

void SnapshotPlayer::renderFullscreen(M5GFX& display, int16_t x, int16_t y, int16_t w, int16_t h) {
    if (hasValidFrame()) {
        if (xSemaphoreTake(buffer_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            // 1. 优先尝试 M5GFX 硬件高速解码
            bool ok = display.drawJpg(frame_buffer, buffer_size, x, y, w, h);
            if (!ok) {
                // 2. 若硬件解码不支持 Progressive JPEG，自动启用 PSRAM STB 全能解码器
                int img_w = 0, img_h = 0, channels = 0;
                uint8_t* rgb = stbi_load_from_memory(frame_buffer, (int)buffer_size, &img_w, &img_h, &channels, 3);
                if (rgb) {
                    renderDecodedRGB888(display, rgb, img_w, img_h, x, y, w, h);
                    stbi_image_free(rgb);
                    ok = true;
                    Serial.printf("[SnapshotPlayer] Fullscreen Progressive JPEG decoded with STB: %dx%d -> %dx%d\n", img_w, img_h, w, h);
                }
            }
            if (!ok) {
                display.fillRect(x, y, w, h, 0x0821);
            }
            xSemaphoreGive(buffer_mutex);
        }
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
}
