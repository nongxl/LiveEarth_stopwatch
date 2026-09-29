#include "snapshot_player.h"
#include "config.h"
#include "network_manager.h"
#include "camera_drawer_view.h"
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
      need_decode(false),
      frame_ready(false),
      has_decoded_frame(false),
      is_using_cache(false),
      fail_count(0),
      next_allowed_fetch_ms(0),
      frame_seq(0),
      frame_buffer(nullptr),
      buffer_size(0),
      buffer_capacity(SNAPSHOT_MAX_BUF_SIZE),
      canvas_preview(&M5.Display),
      canvas_fullscreen(&M5.Display),
      active_place(nullptr),
      pending_place(nullptr),
      fetching_place(nullptr),
      last_fetch_ms(0),
      fetch_start_ms(0),
      last_decode_done_ms(0),
      last_frame_interval_ms(0),
      task_running(false),
      stop_requested(false),
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
        // 若切了新地点且前一机位的下载任务还在进行，立即发出中断信号并登记挂起目标！
        if (task_running) {
            stop_requested = true;
            pending_place = place;
        } else {
            pending_place = nullptr;
        }

        need_decode = false;
        frame_ready = false;
        frame_seq = 0;
        fail_count = 0;
        next_allowed_fetch_ms = 0;

        // 尝试从抽屉预加载缓存池中提取该地点的缩略图作为即时底图（双画布秒开，0秒黑屏等待！）
        int p_idx = -1;
        for (size_t i = 0; i < GLOBAL_PLACES_COUNT; i++) {
            if (&GLOBAL_PLACES[i] == place) {
                p_idx = (int)i;
                break;
            }
        }
        uint16_t* thumb = (p_idx >= 0) ? camera_drawer.getThumbnail(p_idx) : nullptr;
        if (thumb) {
            // 1. 即时填入预览视窗 Canvas (240x135)
            if (canvas_preview.getBuffer()) {
                canvas_preview.startWrite();
                for (int dy = 0; dy < 135; dy++) {
                    int sy = dy * CameraDrawerView::THUMB_H / 135;
                    const uint16_t* s_row = thumb + sy * CameraDrawerView::THUMB_W;
                    for (int dx = 0; dx < 240; dx++) {
                        int sx = dx * CameraDrawerView::THUMB_W / 240;
                        canvas_preview.writePixel(dx, dy, s_row[sx]);
                    }
                }
                canvas_preview.endWrite();
            }

            // 2. 即时填入全屏视窗 Canvas (406x228)
            if (canvas_fullscreen.getBuffer()) {
                canvas_fullscreen.startWrite();
                for (int dy = 0; dy < 228; dy++) {
                    int sy = dy * CameraDrawerView::THUMB_H / 228;
                    const uint16_t* s_row = thumb + sy * CameraDrawerView::THUMB_W;
                    for (int dx = 0; dx < 406; dx++) {
                        int sx = dx * CameraDrawerView::THUMB_W / 406;
                        canvas_fullscreen.writePixel(dx, dy, s_row[sx]);
                    }
                }
                canvas_fullscreen.endWrite();
            }

            has_decoded_frame = true;
            is_using_cache = true; // 标记正在使用缓存帧
            Serial.printf("[SnapshotPlayer] Instant zero-wait cache frame activated for %s\n", place->name);
        } else {
            // 该地点尚未生成缩略图缓存：坚决清空旧画面，彻底消灭“卡死显示上一地点画面”的严重缺陷！
            has_decoded_frame = false;
            is_using_cache = false;
            if (canvas_preview.getBuffer()) canvas_preview.fillScreen(0x0842);
            if (canvas_fullscreen.getBuffer()) canvas_fullscreen.fillScreen(0x0821);
        }
    }

    if (!place->is_online || !place->snapshot_url || strlen(place->snapshot_url) == 0) {
        current_state = SNAP_STATE_IDLE;
        return;
    }

    uint32_t now = millis();
    // 智能退避与控频保护：如果正处于失败退避期，绝不立刻重发网络请求，继续平滑展示本地缓存
    if (now < next_allowed_fetch_ms && !force_refresh && !is_new_place) {
        return;
    }

    if (is_new_place || force_refresh || (now - last_fetch_ms > SNAPSHOT_REFRESH_INTERVAL_MS)) {
        strncpy(current_url, place->snapshot_url, sizeof(current_url) - 1);
        current_url[sizeof(current_url) - 1] = '\0';
        if (!task_running) {
            frame_seq++;
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
    stop_requested = false;
    fetching_place = active_place;
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
    Serial.printf("[SnapshotPlayer] Fetching frame #%u from: %s\n", frame_seq, current_url);

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
    http.addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    http.addHeader("Accept", "image/jpeg,image/*,*/*");
    http.addHeader("Connection", "close"); // 显式请求短连接，避免服务端主动超时RESET导致客户端抛出SSL EOF (-29312)

    uint32_t t_connect_start = millis();
    int http_code = http.GET();
    uint32_t t_connected = millis();

    if (http_code == HTTP_CODE_OK) {
        int content_len = http.getSize();
        WiFiClient* stream = http.getStreamPtr();

        if (stream) {
            if (xSemaphoreTake(buffer_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                buffer_size = 0;
                uint32_t read_start = millis();

                while (!stop_requested && (content_len > 0 ? (int)buffer_size < content_len : http.connected())) {
                    size_t avail = stream->available();
                    if (avail > 0) {
                        size_t max_read = buffer_capacity - buffer_size;
                        if (max_read == 0) {
                            Serial.println("[SnapshotPlayer] [WARN] Image exceeds buffer capacity!");
                            break;
                        }
                        size_t to_read = (avail > 4096) ? 4096 : avail;
                        if (to_read > max_read) to_read = max_read;

                        int bytes_read = stream->readBytes((char*)frame_buffer + buffer_size, to_read);
                        if (bytes_read > 0) {
                            buffer_size += bytes_read;
                            read_start = millis();
                        }
                    } else {
                        // 跨境网络等待超时放宽至 6 秒
                        if (millis() - read_start > 6000) {
                            Serial.println("[SnapshotPlayer] [WARN] Stream read stalled timeout!");
                            break;
                        }
                        vTaskDelay(pdMS_TO_TICKS(2));
                    }
                }

                downloaded_bytes = buffer_size;
                xSemaphoreGive(buffer_mutex);

                // --- 严格完整性校验：防止残破帧送去解码 ---
                bool is_complete = false;
                if (content_len > 0) {
                    // 如果已知 Content-Length，必须 100% 完整接收
                    is_complete = ((int)downloaded_bytes == content_len);
                } else if (downloaded_bytes > 4) {
                    // 若无 Content-Length，在末尾 32 字节内探测 JPEG EOI 标记 (0xFF 0xD9)
                    for (int i = (int)downloaded_bytes - 1; i >= (int)downloaded_bytes - 32 && i >= 1; --i) {
                        if (frame_buffer[i - 1] == 0xFF && frame_buffer[i] == 0xD9) {
                            is_complete = true;
                            break;
                        }
                    }
                }

                // 校验 SOI (0xFF 0xD8) + 完整性
                if (is_complete && downloaded_bytes > 4 && frame_buffer[0] == 0xFF && frame_buffer[1] == 0xD8) {
                    success = true;
                    uint32_t t_now = millis();
                    Serial.printf("[SnapshotPlayer] [OK] Downloaded %u KB frame #%u (Connect: %u ms, Transfer: %u ms, Total: %u ms)\n",
                                  (uint32_t)(downloaded_bytes / 1024),
                                  frame_seq,
                                  (uint32_t)(t_connected - t_connect_start),
                                  (uint32_t)(t_now - t_connected),
                                  (uint32_t)(t_now - fetch_start_ms));
                } else {
                    Serial.printf("[SnapshotPlayer] [WARN] Incomplete frame #%u dropped: got %u bytes, expected %d bytes (Total: %u ms)\n",
                                  frame_seq, (uint32_t)downloaded_bytes, content_len, (uint32_t)(millis() - fetch_start_ms));
                }
            }
        }
    } else {
        Serial.printf("[SnapshotPlayer] [FAIL] HTTP GET failed, code: %d\n", http_code);
    }

    http.end();

    // 严谨校验：如果任务期间用户已经切换到了其它地点，或者发出了中断请求，坚决丢弃本次下载的数据！
    if (stop_requested || active_place != fetching_place) {
        Serial.println("[SnapshotPlayer] Aborted/obsolete frame discarded (user switched location).");
        task_running = false;
        return;
    }

    if (success) {
        current_state = SNAP_STATE_READY;
        last_fetch_ms = millis();
        fail_count = 0; // 成功恢复，重置失败计数
        next_allowed_fetch_ms = millis() + SNAPSHOT_REFRESH_INTERVAL_MS;
        need_decode = true; // 告知主线程：新数据就绪，请在下一次 update 中解码！
    } else {
        current_state = SNAP_STATE_ERROR;
        last_fetch_ms = millis();
        fail_count++;
        // 智能指数退避：10s -> 20s -> 40s，避免高频请求轰炸导致被境外CDN封锁
        uint32_t backoff_sec = 10 * (1 << std::min((int)fail_count - 1, 2));
        next_allowed_fetch_ms = millis() + backoff_sec * 1000;
        Serial.printf("[SnapshotPlayer] [WARN] Fetch failed (fail #%u), backoff %u sec. Keeping active frame/cache.\n",
                      fail_count, backoff_sec);
    }

    task_running = false;
}

// ----------------------------------------------------------------------
// 预解码管线：TJpgDec 极速流式硬件解码 (优先) → STB 全功能兜底 (备用)
// ----------------------------------------------------------------------
void SnapshotPlayer::decodeAndCacheFrame(M5GFX& display) {
    if (!frame_buffer || buffer_size == 0) return;

    if (xSemaphoreTake(buffer_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        bool decoded = false;

        // 1. 优先使用 LovyanGFX 硬件级 TJpgDec 流式解码器
        // 极低内存消耗 (仅 3.9KB pool)，原生硬件 sub-sampling，完美支持 1080P/4K 高清快照等比自适应居中！
        canvas_fullscreen.fillScreen(0x0000);
        canvas_preview.fillScreen(0x0000);

        bool ok_full = canvas_fullscreen.drawJpg(frame_buffer, buffer_size, 0, 0, 406, 228, 0, 0, 0.0f, 0.0f, datum_t::middle_center);
        bool ok_prev = canvas_preview.drawJpg(frame_buffer, buffer_size, 0, 0, 240, 135, 0, 0, 0.0f, 0.0f, datum_t::middle_center);

        if (ok_full && ok_prev) {
            decoded = true;
            has_decoded_frame = true;
            is_using_cache = false;
            Serial.printf("[SnapshotPlayer] TJpgDec Stream Decoded frame #%u (406x228 & 240x135)\n", frame_seq);
        } else {
            // 2. 若遇特殊格式（如 Progressive 渐进式 JPEG），安全回退到 STB 解码
            int img_w = 0, img_h = 0, channels = 0;
            if (stbi_info_from_memory(frame_buffer, (int)buffer_size, &img_w, &img_h, &channels)) {
                // 内存安全红线防护：仅对 <= 1280x720 的中小型特殊格式图使用 STB，杜绝 6MB+ 导致 PSRAM 溢出
                if ((size_t)img_w * img_h * 3 < 3 * 1024 * 1024) {
                    uint8_t* rgb = stbi_load_from_memory(frame_buffer, (int)buffer_size,
                                                          &img_w, &img_h, &channels, 3);
                    if (rgb && img_w > 0 && img_h > 0) {
                        float scale_f = std::min(406.0f / (float)img_w, 228.0f / (float)img_h);
                        int dst_w_f   = (int)(img_w * scale_f);
                        int dst_h_f   = (int)(img_h * scale_f);
                        if (dst_w_f > 406) dst_w_f = 406;
                        if (dst_h_f > 228) dst_h_f = 228;
                        int ox_f = (406 - dst_w_f) / 2;
                        int oy_f = (228 - dst_h_f) / 2;

                        float scale_p = std::min(240.0f / (float)img_w, 135.0f / (float)img_h);
                        int dst_w_p   = (int)(img_w * scale_p);
                        int dst_h_p   = (int)(img_h * scale_p);
                        if (dst_w_p > 240) dst_w_p = 240;
                        if (dst_h_p > 135) dst_h_p = 135;
                        int ox_p = (240 - dst_w_p) / 2;
                        int oy_p = (135 - dst_h_p) / 2;

                        canvas_fullscreen.startWrite();
                        for (int dy = 0; dy < dst_h_f; ++dy) {
                            int src_y = (int)((dy + 0.5f) * img_h / dst_h_f);
                            if (src_y >= img_h) src_y = img_h - 1;
                            const uint8_t* src_row = rgb + (size_t)(src_y * img_w * 3);
                            for (int dx = 0; dx < dst_w_f; ++dx) {
                                int src_x = (int)((dx + 0.5f) * img_w / dst_w_f);
                                if (src_x >= img_w) src_x = img_w - 1;
                                const uint8_t* p = src_row + src_x * 3;
                                canvas_fullscreen.writePixel(ox_f + dx, oy_f + dy,
                                    canvas_fullscreen.color565(p[0], p[1], p[2]));
                            }
                        }
                        canvas_fullscreen.endWrite();

                        canvas_preview.startWrite();
                        for (int dy = 0; dy < dst_h_p; ++dy) {
                            int src_y = (int)((dy + 0.5f) * img_h / dst_h_p);
                            if (src_y >= img_h) src_y = img_h - 1;
                            const uint8_t* src_row = rgb + (size_t)(src_y * img_w * 3);
                            for (int dx = 0; dx < dst_w_p; ++dx) {
                                int src_x = (int)((dx + 0.5f) * img_w / dst_w_p);
                                if (src_x >= img_w) src_x = img_w - 1;
                                const uint8_t* p = src_row + src_x * 3;
                                canvas_preview.writePixel(ox_p + dx, oy_p + dy,
                                    canvas_preview.color565(p[0], p[1], p[2]));
                            }
                        }
                        canvas_preview.endWrite();

                        stbi_image_free(rgb);
                        decoded = true;
                        has_decoded_frame = true;
                        is_using_cache = false;
                        Serial.printf("[SnapshotPlayer] STB Decoded progressive frame #%u (%dx%d)\n", frame_seq, img_w, img_h);
                    }
                } else {
                    Serial.printf("[SnapshotPlayer] [WARN] Image too large for STB fallback (%dx%d), dropped to protect RAM.\n", img_w, img_h);
                }
            }
        }

        if (!decoded) {
            Serial.printf("[SnapshotPlayer] [FAIL] All JPEG decoders failed! buf_size=%u\n", (uint32_t)buffer_size);
        }

        // 记录帧间隔（用于计算实际刷新频率）
        uint32_t now_ms = millis();
        if (last_decode_done_ms > 0) {
            last_frame_interval_ms = now_ms - last_decode_done_ms;
        }
        last_decode_done_ms = now_ms;

        xSemaphoreGive(buffer_mutex);
    }
}

void SnapshotPlayer::renderPreview(M5GFX& display, int16_t x, int16_t y, int16_t w, int16_t h) {
    if (has_decoded_frame && canvas_preview.getBuffer()) {
        canvas_preview.pushSprite(&display, x, y);
        // 关键硬件级视口保护：pushSprite 写入局部视口后，必须立即重置硬件地址窗口为全屏
        display.setClipRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        display.setAddrWindow(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

        // 若当前展示的是本地预热缩略图缓存，右上角打上极具科技感的 CACHED 标签
        if (is_using_cache) {
            display.setFont(&fonts::Font0);
            display.setTextDatum(top_right);
            display.setTextSize(1);
            display.fillRect(x + w - 46, y + 4, 42, 11, 0x0000);
            display.drawRect(x + w - 46, y + 4, 42, 11, 0x1B55);
            display.setTextColor(COLOR_ACCENT_CYAN, 0x0000);
            display.drawString("CACHED", x + w - 6, y + 6);
        }
    } else {
        display.fillRect(x, y, w, h, 0x0842);
        display.setFont(&fonts::Font0);
        display.setTextDatum(middle_center);
        display.setTextSize(1);
        if (current_state == SNAP_STATE_FETCHING) {
            display.setTextColor(0x07FF, 0x0842);
            display.drawString("SYNCING FEED...", x + w / 2, y + h / 2);
        } else {
            display.setTextColor(0xA554, 0x0842);
            display.drawString("STANDBY // AWAITING STREAM", x + w / 2, y + h / 2);
        }
    }
}

void SnapshotPlayer::renderFullscreen(M5GFX& display, int16_t x, int16_t y, int16_t w, int16_t h) {
    if (has_decoded_frame && canvas_fullscreen.getBuffer()) {
        canvas_fullscreen.pushSprite(&display, x, y);
        // 关键硬件级视口保护：推送 406x228 局部窗口后，必须彻底重置 CASET/RASET 硬件寄存器为全屏 (0, 0, 466, 466)
        // 彻底根治 AMOLED CO5300 硬件控制器残留 X=30 偏移导致随后的全屏清屏与文字整体左移 30 像素的致命 Bug！
        display.setClipRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        display.setAddrWindow(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

        // 全屏模式下同样维持缓存指示，绝不给用户黑屏体验
        if (is_using_cache) {
            display.setFont(&fonts::Font0);
            display.setTextDatum(top_right);
            display.setTextSize(1);
            display.fillRect(x + w - 50, y + 6, 46, 12, 0x0000);
            display.drawRect(x + w - 50, y + 6, 46, 12, 0x1B55);
            display.setTextColor(COLOR_ACCENT_CYAN, 0x0000);
            display.drawString("CACHED", x + w - 7, y + 8);
        }
    } else {
        display.fillRect(x, y, w, h, 0x0821);
        display.setFont(&fonts::Font0);
        display.setTextDatum(middle_center);
        display.setTextSize(1);
        if (current_state == SNAP_STATE_FETCHING) {
            display.setTextColor(0x07FF, 0x0821);
            display.drawString("CONNECTING HIGH-RES FEED...", x + w / 2, y + h / 2);
        } else {
            display.setTextColor(0xA554, 0x0821);
            display.drawString("STANDBY // NO SIGNAL", x + w / 2, y + h / 2);
        }
    }
}

void SnapshotPlayer::update(uint32_t now_ms) {
    // 1. 如果后台任务刚下载完新 JPEG，立即在主线程 PSRAM 中执行解码与重采样
    if (need_decode) {
        need_decode = false;
        decodeAndCacheFrame(M5.Display);
        frame_ready = true; // 解码完成，立刻通知视图层重绘刷新底图！
    }

    // 2. 如果之前切换机位时有挂起的拉取目标（等待前序任务退出），前序任务退出后立即无缝启动新目标拉取！
    if (!task_running && pending_place != nullptr) {
        const PlaceLocation* p = pending_place;
        pending_place = nullptr;
        loadPlace(p, true);
        return;
    }

    // 3. 定时轮询与自动重刷新逻辑 (严格遵循退避时间与合理间隔，杜绝并发频控)
    if (active_place && active_place->snapshot_url && strlen(active_place->snapshot_url) > 0) {
        if (!task_running && (now_ms >= next_allowed_fetch_ms) && (now_ms - last_fetch_ms > SNAPSHOT_REFRESH_INTERVAL_MS)) {
            loadPlace(active_place, true);
        }
    }
}

void SnapshotPlayer::stop() {
    stop_requested = true;
    current_state = SNAP_STATE_IDLE;
    active_place = nullptr;
    pending_place = nullptr;
    fetching_place = nullptr;
    has_decoded_frame = false;
    need_decode = false;
    frame_ready = false;
}
