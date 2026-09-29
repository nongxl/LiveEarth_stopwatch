#include "camera_drawer_view.h"
#include "config.h"
#include "network_manager.h"
#include <cmath>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_heap_caps.h>
#include "stb_image.h"
#include "snapshot_player.h"

CameraDrawerView camera_drawer;

static uint8_t* prefetch_temp_buf = nullptr;
static constexpr size_t PREFETCH_TEMP_BUF_SIZE = 512 * 1024; // 提升至 512KB PSRAM 接收缓冲区，彻底杜绝高清快照截断

CameraDrawerView::CameraDrawerView()
    : state(DRAWER_CLOSED),
      anim_start_ms(0),
      current_slide_x(-DRAWER_WIDTH),
      filter_live_only(false),
      scroll_y(0.0f),
      target_scroll_y(0.0f),
      vel_scroll_y(0.0f),
      was_touching(false),
      touch_start_x(0),
      touch_start_y(0),
      touch_last_x(0),
      touch_last_y(0),
      touch_start_ms(0),
      is_dragging_list(false),
      is_horizontal_swipe(false),
      pulse_phase(0.0f),
      slots_mutex(nullptr),
      prefetch_running(false),
      pending_prefetch_idx(-1),
      last_prefetch_attempt_ms(0),
      prefetch_task_handle(nullptr)
{
    memset(place_fail_cd, 0, sizeof(place_fail_cd));
    for (size_t i = 0; i < THUMB_SLOTS_NUM; i++) {
        thumb_slots[i].place_idx = -1;
        thumb_slots[i].pixels = nullptr;
        thumb_slots[i].last_used = 0;
        thumb_slots[i].is_ready = false;
    }
}

void CameraDrawerView::init() {
    state = DRAWER_CLOSED;
    current_slide_x = -DRAWER_WIDTH;
    filter_live_only = false;
    scroll_y = 0.0f;
    target_scroll_y = 0.0f;
    vel_scroll_y = 0.0f;
    was_touching = false;
    is_dragging_list = false;
    is_horizontal_swipe = false;
    pulse_phase = 0.0f;
    memset(place_fail_cd, 0, sizeof(place_fail_cd));

    if (!slots_mutex) {
        slots_mutex = xSemaphoreCreateMutex();
    }

    // 在 PSRAM 中分配 8 个槽位的缩略图显存 (8 * 84 * 48 * 2 = ~64KB)
    for (size_t i = 0; i < THUMB_SLOTS_NUM; i++) {
        thumb_slots[i].place_idx = -1;
        thumb_slots[i].last_used = 0;
        thumb_slots[i].is_ready = false;
        if (!thumb_slots[i].pixels) {
            thumb_slots[i].pixels = (uint16_t*)heap_caps_malloc(
                THUMB_W * THUMB_H * sizeof(uint16_t),
                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
            );
            if (thumb_slots[i].pixels) {
                memset(thumb_slots[i].pixels, 0, THUMB_W * THUMB_H * sizeof(uint16_t));
            }
        }
    }

    // 分配 48KB 的 PSRAM 预加载临时网络接收缓冲
    if (!prefetch_temp_buf) {
        prefetch_temp_buf = (uint8_t*)heap_caps_malloc(PREFETCH_TEMP_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }

    // 启动后台预加载任务 (绑定在 Core 0，优先级 1，顺序调度)
    if (!prefetch_task_handle) {
        xTaskCreatePinnedToCore(
            prefetchTaskStatic,
            "drawer_thumb",
            8192,
            this,
            1,
            &prefetch_task_handle,
            0 // Core 0
        );
    }
}

int CameraDrawerView::findSlotForPlace(int place_idx) {
    if (place_idx < 0) return -1;
    int found = -1;
    if (slots_mutex && xSemaphoreTake(slots_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        for (size_t i = 0; i < THUMB_SLOTS_NUM; i++) {
            if (thumb_slots[i].place_idx == place_idx && thumb_slots[i].is_ready) {
                thumb_slots[i].last_used = millis();
                found = (int)i;
                break;
            }
        }
        xSemaphoreGive(slots_mutex);
    }
    return found;
}

ThumbSlot* CameraDrawerView::allocateSlotForPlace(int place_idx) {
    if (place_idx < 0) return nullptr;

    // 1. 若已存在该地点的槽位直接复用
    for (size_t i = 0; i < THUMB_SLOTS_NUM; i++) {
        if (thumb_slots[i].place_idx == place_idx) {
            thumb_slots[i].last_used = millis();
            return &thumb_slots[i];
        }
    }

    // 2. 查找空闲槽位
    for (size_t i = 0; i < THUMB_SLOTS_NUM; i++) {
        if (thumb_slots[i].place_idx == -1) {
            thumb_slots[i].place_idx = place_idx;
            thumb_slots[i].is_ready = false;
            thumb_slots[i].last_used = millis();
            return &thumb_slots[i];
        }
    }

    // 3. LRU 淘汰最久未使用的槽位
    size_t lru_idx = 0;
    uint32_t oldest_time = 0xFFFFFFFF;
    for (size_t i = 0; i < THUMB_SLOTS_NUM; i++) {
        if (thumb_slots[i].last_used < oldest_time) {
            oldest_time = thumb_slots[i].last_used;
            lru_idx = i;
        }
    }

    thumb_slots[lru_idx].place_idx = place_idx;
    thumb_slots[lru_idx].is_ready = false;
    thumb_slots[lru_idx].last_used = millis();
    return &thumb_slots[lru_idx];
}

uint16_t* CameraDrawerView::getThumbnail(int place_idx) {
    if (place_idx < 0) return nullptr;
    uint16_t* ret = nullptr;
    if (slots_mutex && xSemaphoreTake(slots_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        for (size_t i = 0; i < THUMB_SLOTS_NUM; i++) {
            if (thumb_slots[i].place_idx == place_idx && thumb_slots[i].is_ready) {
                thumb_slots[i].last_used = millis();
                ret = thumb_slots[i].pixels;
                break;
            }
        }
        xSemaphoreGive(slots_mutex);
    }
    return ret;
}

size_t CameraDrawerView::getFilteredCount() const {
    if (!filter_live_only) return GLOBAL_PLACES_COUNT;
    size_t count = 0;
    for (size_t i = 0; i < GLOBAL_PLACES_COUNT; i++) {
        if (GLOBAL_PLACES[i].is_online) count++;
    }
    return count;
}

int CameraDrawerView::getFilteredPlaceIndex(int filtered_row) const {
    if (filtered_row < 0) return -1;
    if (!filter_live_only) {
        return (filtered_row < (int)GLOBAL_PLACES_COUNT) ? filtered_row : -1;
    }
    int current_row = 0;
    for (size_t i = 0; i < GLOBAL_PLACES_COUNT; i++) {
        if (GLOBAL_PLACES[i].is_online) {
            if (current_row == filtered_row) return (int)i;
            current_row++;
        }
    }
    return -1;
}

void CameraDrawerView::setFilterLiveOnly(bool live_only) {
    if (filter_live_only != live_only) {
        filter_live_only = live_only;
        scroll_y = 0.0f;
        target_scroll_y = 0.0f;
        vel_scroll_y = 0.0f;
    }
}

void CameraDrawerView::toggleFilter() {
    setFilterLiveOnly(!filter_live_only);
}

void CameraDrawerView::open() {
    state = DRAWER_OPENING;
    anim_start_ms = millis();
}

void CameraDrawerView::close() {
    state = DRAWER_CLOSING;
    anim_start_ms = millis();
}

void CameraDrawerView::toggle() {
    if (state == DRAWER_OPEN || state == DRAWER_OPENING) {
        close();
    } else {
        open();
    }
}

void CameraDrawerView::scrollToSelected(int selected_idx) {
    if (selected_idx < 0 || selected_idx >= (int)GLOBAL_PLACES_COUNT) return;
    size_t f_count = getFilteredCount();
    if (f_count == 0) return;

    int target_row = -1;
    if (!filter_live_only) {
        target_row = selected_idx;
    } else {
        int current_row = 0;
        for (size_t i = 0; i < GLOBAL_PLACES_COUNT; i++) {
            if (GLOBAL_PLACES[i].is_online) {
                if ((int)i == selected_idx) {
                    target_row = current_row;
                    break;
                }
                current_row++;
            }
        }
    }
    if (target_row < 0) return;

    float desired_y = -(target_row * ITEM_HEIGHT) + (LIST_VISIBLE_H / 2) - (ITEM_HEIGHT / 2);
    float max_scroll = 0.0f;
    float min_scroll = -((int)f_count * ITEM_HEIGHT - LIST_VISIBLE_H);
    if (min_scroll > 0.0f) min_scroll = 0.0f;

    if (desired_y > max_scroll) desired_y = max_scroll;
    if (desired_y < min_scroll) desired_y = min_scroll;

    target_scroll_y = desired_y;
    scroll_y = desired_y;
    vel_scroll_y = 0.0f;
}

int CameraDrawerView::selectNextItem(int current_selected_idx) {
    size_t f_count = getFilteredCount();
    if (f_count == 0) return -1;

    // 查找当前选中的项在当前列表（ALL 或 LIVE）中的行号
    int cur_row = -1;
    for (size_t r = 0; r < f_count; r++) {
        if (getFilteredPlaceIndex((int)r) == current_selected_idx) {
            cur_row = (int)r;
            break;
        }
    }

    // 向下走一行，循环往复
    int next_row = (cur_row + 1) % (int)f_count;
    int next_place_idx = getFilteredPlaceIndex(next_row);

    // 自动平滑滚动到该项居中显示
    scrollToSelected(next_place_idx);

    return next_place_idx;
}

void CameraDrawerView::checkAndPrefetchVisible() {
    if (state == DRAWER_CLOSED || prefetch_running) return;
    if (snapshot_player.isFetching()) return; // 礼让前台大图拉取，避免同 IP 并发 TLS 冲突
    if (fabsf(vel_scroll_y) > 3.0f) return; // 正在快速惯性拖拽中暂停网络请求

    uint32_t now_ms = millis();
    if (now_ms - last_prefetch_attempt_ms < 100) return;
    last_prefetch_attempt_ms = now_ms;

    size_t f_count = getFilteredCount();
    if (f_count == 0) return;

    // 计算当前视野中可见的 5 个条目的行号范围
    int row_start = (int)(-scroll_y / ITEM_HEIGHT);
    if (row_start < 0) row_start = 0;
    int row_end = row_start + 5;
    if (row_end > (int)f_count) row_end = (int)f_count;

    // 检查是否有未缓存且未处于冷却期的在线摄像头，有则启动流水线
    for (int r = row_start; r < row_end; r++) {
        int p_idx = getFilteredPlaceIndex(r);
        if (p_idx >= 0 && p_idx < (int)GLOBAL_PLACES_COUNT) {
            const PlaceLocation& place = GLOBAL_PLACES[p_idx];
            if (place.is_online && place.snapshot_url && place.snapshot_url[0] != '\0') {
                if (findSlotForPlace(p_idx) < 0) {
                    if (now_ms - place_fail_cd[p_idx] > 15000) {
                        prefetch_running = true;
                        if (prefetch_task_handle) {
                            xTaskNotifyGive(prefetch_task_handle);
                        }
                        break;
                    }
                }
            }
        }
    }
}

void CameraDrawerView::prefetchTaskStatic(void* param) {
    CameraDrawerView* instance = (CameraDrawerView*)param;
    instance->prefetchTaskWorker();
}

void CameraDrawerView::prefetchTaskWorker() {
    while (true) {
        // 等待主线程预取通知，或每 300ms 周期唤醒自省
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(300));

        if (!prefetch_running || state == DRAWER_CLOSED) {
            prefetch_running = false;
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        // 连续流水线拉取：只要抽屉处于展开/展开中状态，持续将当前可视的 5 个条目全部下载完毕！
        while (prefetch_running && (state == DRAWER_OPEN || state == DRAWER_OPENING)) {
            if (WiFi.status() != WL_CONNECTED) {
                break;
            }
            if (snapshot_player.isFetching()) {
                vTaskDelay(pdMS_TO_TICKS(200));
                continue;
            }

            int target_idx = -1;
            size_t f_count = getFilteredCount();
            int row_start = (int)(-scroll_y / ITEM_HEIGHT);
            if (row_start < 0) row_start = 0;
            int row_end = row_start + 5;
            if (row_end > (int)f_count) row_end = (int)f_count;

            uint32_t now_ms = millis();
            for (int r = row_start; r < row_end; r++) {
                int p_idx = getFilteredPlaceIndex(r);
                if (p_idx >= 0 && p_idx < (int)GLOBAL_PLACES_COUNT) {
                    const PlaceLocation& p = GLOBAL_PLACES[p_idx];
                    if (p.is_online && p.snapshot_url && p.snapshot_url[0] != '\0') {
                        if (findSlotForPlace(p_idx) < 0) {
                            // 检查冷却期（15秒）避免单个离线/慢速摄像头阻塞全列表
                            if (now_ms - place_fail_cd[p_idx] > 15000) {
                                target_idx = p_idx;
                                break;
                            }
                        }
                    }
                }
            }

            if (target_idx < 0) {
                // 当前屏幕内可见条目已全部就绪（或处于失败冷却），退出流水线进入待机
                break;
            }

            const PlaceLocation& place = GLOBAL_PLACES[target_idx];
            HTTPClient http;
            http.setReuse(false);
            http.setTimeout(4000); // 4秒连接超时
            http.begin(place.snapshot_url);
            http.addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
            http.addHeader("Accept", "image/jpeg,image/*,*/*");

            int http_code = http.GET();
            bool decode_success = false;

            if (http_code == HTTP_CODE_OK) {
                int content_len = http.getSize();
                WiFiClient* stream = http.getStreamPtr();
                size_t total_read = 0;
                uint32_t start_read = millis();

                // 严谨读取循环：
                // 1. 若已知 content_len > 0，必须读满 content_len 字节，绝不提前截断！
                // 2. 若未知 content_len (如 chunked 传输)，遇尾部 0xFF,0xD9 或连接结束才终止。
                while (http.connected() && (content_len > 0 ? (int)total_read < content_len : true)) {
                    size_t avail = stream->available();
                    if (avail > 0) {
                        size_t to_read = (avail > 4096) ? 4096 : avail;
                        if (total_read + to_read > PREFETCH_TEMP_BUF_SIZE) {
                            to_read = PREFETCH_TEMP_BUF_SIZE - total_read;
                        }
                        if (to_read == 0) break;

                        int bytes = stream->readBytes((char*)prefetch_temp_buf + total_read, to_read);
                        if (bytes > 0) {
                            total_read += bytes;
                            start_read = millis();

                            // 达到声明大小瞬间完成退出
                            if (content_len > 0 && (int)total_read >= content_len) {
                                break;
                            }
                        }
                    } else {
                        if (content_len > 0 && (int)total_read >= content_len) break;
                        // 若未声明 Content-Length，在末尾检测到 JPEG 结束标记 0xFF,0xD9 视为完整
                        if (content_len <= 0 && total_read >= 4) {
                            if (prefetch_temp_buf[total_read - 2] == 0xFF && prefetch_temp_buf[total_read - 1] == 0xD9) {
                                break;
                            }
                        }
                        // 跨境网络单包等待超时放宽至 4 秒
                        if (millis() - start_read > 4000) {
                            break;
                        }
                        vTaskDelay(pdMS_TO_TICKS(4));
                    }
                    if (total_read >= PREFETCH_TEMP_BUF_SIZE) break;
                }

                http.end(); // 关闭网络连接

                // 严格完整性验证：确保文件是完整的有效 JPEG（必须有 SOI 0xFF,0xD8，且完整收齐全包）
                bool is_valid_jpeg = false;
                if (total_read > 4096 && prefetch_temp_buf[0] == 0xFF && prefetch_temp_buf[1] == 0xD8) {
                    if (content_len > 0) {
                        // 必须 100% 完整接收，绝不将半截数据送入解码器
                        is_valid_jpeg = ((int)total_read >= content_len);
                    } else {
                        // 未知长度时，探测尾部 32 字节内是否有 0xFF, 0xD9
                        for (int i = (int)total_read - 1; i >= (int)total_read - 32 && i >= 1; --i) {
                            if (prefetch_temp_buf[i - 1] == 0xFF && prefetch_temp_buf[i] == 0xD9) {
                                is_valid_jpeg = true;
                                break;
                            }
                        }
                    }
                }

                if (is_valid_jpeg) {
                    // 使用 LovyanGFX 硬件级 TJpgDec 流式缩放至 84x48，仅消耗微量栈内存，彻底杜绝 6.2MB PSRAM 溢出！
                    LGFX_Sprite thumb_spr(&M5.Display);
                    thumb_spr.setColorDepth(16);
                    thumb_spr.setPsram(true);
                    if (thumb_spr.createSprite(THUMB_W, THUMB_H)) {
                        bool ok = thumb_spr.drawJpg(prefetch_temp_buf, total_read, 0, 0, THUMB_W, THUMB_H, 0, 0, 0.0f, 0.0f, datum_t::middle_center);
                        if (ok) {
                            if (slots_mutex && xSemaphoreTake(slots_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                                ThumbSlot* slot = allocateSlotForPlace(target_idx);
                                if (slot && slot->pixels) {
                                    memcpy(slot->pixels, thumb_spr.getBuffer(), THUMB_W * THUMB_H * sizeof(uint16_t));
                                    slot->is_ready = true;
                                    slot->last_used = millis();
                                    decode_success = true;
                                }
                                xSemaphoreGive(slots_mutex);
                                Serial.printf("[Prefetch Pipeline] Valid Thumbnail: %s (84x48, %u KB)\n",
                                              place.name, (unsigned int)(total_read / 1024));
                            }
                        }
                        thumb_spr.deleteSprite();
                    }
                } else {
                    Serial.printf("[Prefetch Pipeline] Incomplete Image Dropped: %s (%u KB, expected %d)\n",
                                  place.name, (unsigned int)(total_read / 1024), content_len);
                }
            } else {
                http.end();
            }

            if (!decode_success) {
                // 失败进入 15 秒冷却保护，让列表中其他有效摄像头能被优先拉取
                place_fail_cd[target_idx] = millis();
            }

            vTaskDelay(pdMS_TO_TICKS(30)); // 仅 30ms 极轻间歇，立即流水线拉取下一个
        }

        prefetch_running = false;
        pending_prefetch_idx = -1;
    }
}

void CameraDrawerView::update(uint32_t now_ms) {
    pulse_phase += 0.08f;
    if (pulse_phase >= 6.2831853f) pulse_phase -= 6.2831853f;

    // 1. 抽屉滑入滑出过渡动画
    if (state == DRAWER_OPENING) {
        uint32_t elapsed = now_ms - anim_start_ms;
        if (elapsed >= 220) {
            state = DRAWER_OPEN;
            current_slide_x = 0.0f;
        } else {
            float t = (float)elapsed / 220.0f;
            float ease = 1.0f - powf(1.0f - t, 3.0f); // Ease-out cubic
            current_slide_x = -DRAWER_WIDTH * (1.0f - ease);
        }
    } else if (state == DRAWER_CLOSING) {
        uint32_t elapsed = now_ms - anim_start_ms;
        if (elapsed >= 200) {
            state = DRAWER_CLOSED;
            current_slide_x = -DRAWER_WIDTH;
        } else {
            float t = (float)elapsed / 200.0f;
            float ease = t * t; // Ease-in
            current_slide_x = -DRAWER_WIDTH * ease;
        }
    }

    // 2. 列表上下惯性滚动与回弹阻尼
    if (!was_touching) {
        size_t f_count = getFilteredCount();
        float max_scroll = 0.0f;
        float min_scroll = -((int)f_count * ITEM_HEIGHT - LIST_VISIBLE_H);
        if (min_scroll > 0.0f) min_scroll = 0.0f;

        if (scroll_y > max_scroll) {
            scroll_y += (max_scroll - scroll_y) * 0.25f;
            vel_scroll_y = 0.0f;
        } else if (scroll_y < min_scroll) {
            scroll_y += (min_scroll - scroll_y) * 0.25f;
            vel_scroll_y = 0.0f;
        } else if (fabsf(vel_scroll_y) > 0.1f) {
            scroll_y += vel_scroll_y;
            vel_scroll_y *= 0.88f; // 惯性衰减
        }
    }

    // 3. 抽屉展开或正在展开时，即刻调度后台顺序预取
    if (state == DRAWER_OPEN || state == DRAWER_OPENING) {
        checkAndPrefetchVisible();
    }
}

void CameraDrawerView::render(M5GFX& display, const PlaceMarkerManager& place_mgr) {
    if (state == DRAWER_CLOSED) return;

    int panel_x = (int)current_slide_x;
    int selected_idx = place_mgr.getSelectedIndex();

    // 1. 抽屉背景与右侧边缘微光光轨
    display.fillRect(panel_x, 0, DRAWER_WIDTH, SCREEN_HEIGHT, 0x0821); // 深沉暗黑科技底板
    // 右边缘垂直霓虹微光线与渐变暗线（形成抽屉与右侧背景 3D 地球仪的高科幻景深隔离，右侧 121px 完整呈现灵动地球仪！）
    display.drawFastVLine(panel_x + DRAWER_WIDTH - 1, 0, SCREEN_HEIGHT, COLOR_ACCENT_CYAN);
    display.drawFastVLine(panel_x + DRAWER_WIDTH - 2, 0, SCREEN_HEIGHT, 0x1B55);
    display.drawFastVLine(panel_x + DRAWER_WIDTH - 3, 0, SCREEN_HEIGHT, 0x0842);

    // 2. 顶部导航 HUD 与过滤筛选开关 (Y: 28 ~ 78)
    size_t online_count = 0;
    for (size_t i = 0; i < GLOBAL_PLACES_COUNT; i++) {
        if (GLOBAL_PLACES[i].is_online) online_count++;
    }

    display.setFont(&fonts::Font0);
    display.setTextDatum(middle_center);
    display.setTextSize(1);
    display.setTextColor(COLOR_ACCENT_CYAN, 0x0821);
    display.drawString("* CAMERA DIRECTORY *", panel_x + DRAWER_WIDTH / 2, 30);

    // 科技双选过滤胶囊开关 (Y: 44 ~ 72, 宽 240px, 高 28px, 圆角 14px)
    int16_t toggle_w = 240;
    int16_t toggle_h = 28;
    int16_t toggle_x = panel_x + (DRAWER_WIDTH - toggle_w) / 2;
    int16_t toggle_y = 44;
    int16_t half_w = toggle_w / 2;

    display.fillRoundRect(toggle_x, toggle_y, toggle_w, toggle_h, 14, 0x0842);
    display.drawRoundRect(toggle_x, toggle_y, toggle_w, toggle_h, 14, 0x2354);

    if (!filter_live_only) {
        // [ ALL (107) ] 选中态高亮
        display.fillRoundRect(toggle_x + 2, toggle_y + 2, half_w - 2, toggle_h - 4, 12, 0x1B55);
        display.drawRoundRect(toggle_x + 2, toggle_y + 2, half_w - 2, toggle_h - 4, 12, COLOR_ACCENT_CYAN);
        
        display.setTextDatum(middle_center);
        display.setTextColor(0xFFFF, 0x1B55);
        char all_text[32];
        snprintf(all_text, sizeof(all_text), "ALL (%u)", (unsigned int)GLOBAL_PLACES_COUNT);
        display.drawString(all_text, toggle_x + half_w / 2, toggle_y + toggle_h / 2);

        // [ * LIVE (11) ] 未选中态
        char live_text[32];
        snprintf(live_text, sizeof(live_text), "* LIVE (%u)", (unsigned int)online_count);
        display.setTextColor(0x07E0, 0x0842);
        display.drawString(live_text, toggle_x + half_w + half_w / 2, toggle_y + toggle_h / 2);
    } else {
        // [ * LIVE (11) ] 选中态高亮
        display.fillRoundRect(toggle_x + half_w, toggle_y + 2, half_w - 2, toggle_h - 4, 12, 0x0320);
        display.drawRoundRect(toggle_x + half_w, toggle_y + 2, half_w - 2, toggle_h - 4, 12, 0x07E0);

        // [ ALL (107) ] 未选中态
        display.setTextDatum(middle_center);
        display.setTextColor(0x7BEF, 0x0842);
        char all_text[32];
        snprintf(all_text, sizeof(all_text), "ALL (%u)", (unsigned int)GLOBAL_PLACES_COUNT);
        display.drawString(all_text, toggle_x + half_w / 2, toggle_y + toggle_h / 2);

        // [ * LIVE (11) ] 纯白文字高亮
        char live_text[32];
        snprintf(live_text, sizeof(live_text), "* LIVE (%u)", (unsigned int)online_count);
        display.setTextColor(0xFFFF, 0x0320);
        display.drawString(live_text, toggle_x + half_w + half_w / 2, toggle_y + toggle_h / 2);
        // 电光绿高亮脉冲微核
        display.fillCircle(toggle_x + half_w + 14, toggle_y + toggle_h / 2, 3, 0x07E0);
    }

    display.drawFastHLine(panel_x + 16, 78, DRAWER_WIDTH - 32, 0x2354);

    // 3. 滚动地点列表 (Y: 82 ~ 415，一屏整齐排布 5 条数据，高度 66px)
    // 硬件级裁剪区域隔离：限制列表仅在 Y: [LIST_TOP_Y, LIST_BOTTOM_Y] 内渲染，彻底避免滚动时与上方胶囊按钮和底部状态栏重叠！
    int32_t clip_x = std::max((int)0, panel_x);
    int32_t clip_r = std::min((int)SCREEN_WIDTH, panel_x + DRAWER_WIDTH);
    int32_t clip_w = clip_r - clip_x;
    if (clip_w > 0) {
        display.setClipRect(clip_x, LIST_TOP_Y, clip_w, LIST_VISIBLE_H);
    }

    size_t cur_count = getFilteredCount();
    for (size_t row = 0; row < cur_count; row++) {
        int item_y = LIST_TOP_Y + (int)scroll_y + (int)row * ITEM_HEIGHT;
        if (item_y + ITEM_HEIGHT < LIST_TOP_Y || item_y > LIST_BOTTOM_Y) {
            continue; // 视野外剔除
        }

        int real_idx = getFilteredPlaceIndex((int)row);
        if (real_idx < 0) continue;
        const PlaceLocation& place = GLOBAL_PLACES[real_idx];
        bool is_sel = (real_idx == selected_idx);

        // 选中项绘制发光底板
        if (is_sel) {
            display.fillRoundRect(panel_x + 8, item_y + 2, DRAWER_WIDTH - 16, ITEM_HEIGHT - 4, 6, 0x10A2);
            display.drawRoundRect(panel_x + 8, item_y + 2, DRAWER_WIDTH - 16, ITEM_HEIGHT - 4, 6, place.is_online ? 0x07E0 : place.theme_color);
        } else {
            display.drawFastHLine(panel_x + 14, item_y + ITEM_HEIGHT - 1, DRAWER_WIDTH - 28, 0x1082);
        }

        // =========================================================
        // 右侧 16:9 微缩实时预览视窗 (84 x 48 px，垂直居中 Y 偏移 9px，右边距 14px)
        // =========================================================
        int thumb_x = panel_x + DRAWER_WIDTH - 14 - THUMB_W; // 345 - 14 - 84 = 247
        int thumb_y = item_y + 9;

        int slot_idx = findSlotForPlace(real_idx);
        bool has_thumb = (slot_idx >= 0 && thumb_slots[slot_idx].is_ready && thumb_slots[slot_idx].pixels);

        if (has_thumb) {
            // 将 84x48 RGB565 贴图推送到屏幕右侧，显式指定为 rgb565_t 格式
            display.pushImage(thumb_x, thumb_y, THUMB_W, THUMB_H, (const lgfx::rgb565_t*)thumb_slots[slot_idx].pixels);

            // 缩略图右下角半透明深黑底框与状态角标
            if (place.is_online) {
                display.fillRect(thumb_x + THUMB_W - 25, thumb_y + THUMB_H - 12, 25, 12, 0x0000);
                display.setFont(&fonts::Font0);
                display.setTextDatum(middle_center);
                display.setTextSize(1);
                display.setTextColor(0x07E0, 0x0000);
                display.drawString("LIVE", thumb_x + THUMB_W - 12, thumb_y + THUMB_H - 6);
                // 缩略图右上角微光绿核
                display.fillCircle(thumb_x + THUMB_W - 5, thumb_y + 5, 2, 0x07E0);
            }
        } else {
            // 视窗底板与科技感十字准星
            display.fillRect(thumb_x, thumb_y, THUMB_W, THUMB_H, 0x0842);
            display.drawFastHLine(thumb_x + 20, thumb_y + THUMB_H / 2, THUMB_W - 40, 0x18E3);
            display.drawFastVLine(thumb_x + THUMB_W / 2, thumb_y + 10, THUMB_H - 20, 0x18E3);

            // 内部状态文字
            display.setFont(&fonts::Font0);
            display.setTextDatum(middle_center);
            display.setTextSize(1);
            if (place.is_online) {
                display.setTextColor(0x07E0, 0x0842);
                display.drawString("STANDBY", thumb_x + THUMB_W / 2, thumb_y + THUMB_H / 2);
            } else {
                display.setTextColor(0x52AA, 0x0842);
                display.drawString("LOC CAM", thumb_x + THUMB_W / 2, thumb_y + THUMB_H / 2);
            }
        }

        // 视窗科技外边框与 L-Bracket 四角微标
        uint16_t border_col = is_sel ? (place.is_online ? 0x07E0 : COLOR_ACCENT_CYAN) : 0x2354;
        display.drawRect(thumb_x - 1, thumb_y - 1, THUMB_W + 2, THUMB_H + 2, border_col);
        // 四角赛博强化标记
        display.drawPixel(thumb_x - 2, thumb_y - 2, border_col);
        display.drawPixel(thumb_x + THUMB_W + 1, thumb_y - 2, border_col);
        display.drawPixel(thumb_x - 2, thumb_y + THUMB_H + 1, border_col);
        display.drawPixel(thumb_x + THUMB_W + 1, thumb_y + THUMB_H + 1, border_col);

        // =========================================================
        // 左侧文字信息区 (X: panel_x + 16 ~ thumb_x - 10)
        // =========================================================
        int text_x = panel_x + 18;

        // 选中项左边缘优雅的垂直微光指示条
        if (is_sel) {
            display.fillRoundRect(panel_x + 10, item_y + 10, 3, ITEM_HEIGHT - 20, 1,
                                 place.is_online ? 0x07E0 : COLOR_ACCENT_CYAN);
            text_x = panel_x + 22; // 微向右避让
        }

        // 序号徽标与地点大标题 (Font2，大号清晰高雅)
        display.setFont(&fonts::Font0);
        display.setTextDatum(top_left);
        display.setTextSize(1);
        char num_buf[8];
        snprintf(num_buf, sizeof(num_buf), "%02u", (uint32_t)(filter_live_only ? (row + 1) : (real_idx + 1)));
        display.setTextColor(is_sel ? 0xFFFF : 0x7BEF, is_sel ? 0x10A2 : 0x0821);
        display.drawString(num_buf, text_x, item_y + 14);

        // 主题色星核圆点
        display.fillCircle(text_x + 18, item_y + 18, is_sel ? 3 : 2, place.is_online ? 0x07E0 : place.theme_color);

        // 地点英文大名称 (Font2，大号醒目)
        display.setFont(&fonts::Font2);
        display.setTextDatum(top_left);
        display.setTextSize(1);
        display.setTextColor(is_sel ? 0xFFFF : 0xE71C, is_sel ? 0x10A2 : 0x0821);
        display.drawString(place.name, text_x + 26, item_y + 10);

        // 副标题：国家与机位名 (Font0)
        display.setFont(&fonts::Font0);
        char sub_buf[64];
        snprintf(sub_buf, sizeof(sub_buf), "%s | %s", place.country, place.cam_name);
        display.setTextColor(is_sel ? (place.is_online ? 0x07E0 : place.theme_color) : 0x8410, is_sel ? 0x10A2 : 0x0821);
        display.drawString(sub_buf, text_x, item_y + 36);
    }

    // 恢复全屏绘制区域
    display.setClipRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    // 4. 底部指示 HUD (Y: 420 ~ 455)
    display.setFont(&fonts::Font0);
    display.drawFastHLine(panel_x + 16, 420, DRAWER_WIDTH - 32, 0x2354);
    display.setTextDatum(middle_center);
    display.setTextSize(1);
    display.setTextColor(0xA554, 0x0821);
    display.drawString("< SWIPE L/R: ALL <-> LIVE | TAP: FLY >", panel_x + DRAWER_WIDTH / 2, 436);
}

void CameraDrawerView::renderEdgeHandle(M5GFX& display) {
    if (state != DRAWER_CLOSED) return;

    // 在屏幕左边缘居中 (X: 0~12, Y: 213~253) 呈现赛博微光拉手把柄
    float pulse = 0.5f + 0.5f * sinf(pulse_phase);
    uint16_t handle_col = (pulse > 0.4f) ? COLOR_ACCENT_CYAN : 0x2354;

    display.fillRoundRect(0, 213, 12, 40, 5, 0x0842);
    display.drawRoundRect(0, 213, 12, 40, 5, handle_col);

    display.setFont(&fonts::Font0);
    display.setTextDatum(middle_center);
    display.setTextSize(1);
    display.setTextColor(0xFFFF, 0x0842);
    display.drawString(">", 6, 233);
}

int CameraDrawerView::handleTouch(bool is_pressed, int16_t x, int16_t y, uint32_t now_ms) {
    if (state == DRAWER_CLOSED) {
        if (is_pressed) {
            if (!was_touching) {
                was_touching = true;
                touch_start_x = x;
                touch_start_y = y;
                touch_last_x = x;
                touch_last_y = y;
                touch_start_ms = now_ms;
                is_dragging_list = false;
                is_horizontal_swipe = false;
            } else {
                int16_t dx = x - touch_start_x;
                if (touch_start_x < 50 && dx > 30) {
                    open();
                    was_touching = false;
                }
            }
        } else {
            if (was_touching) {
                if (touch_start_x < 36 && abs(touch_start_y - 233) < 35) {
                    open();
                }
                was_touching = false;
            }
        }
        return -1;
    }

    if (state != DRAWER_OPEN) {
        return -1; // 动画过渡中不响应触控
    }

    // 抽屉展开状态下的触控处理
    if (is_pressed) {
        if (!was_touching) {
            was_touching = true;
            touch_start_x = x;
            touch_start_y = y;
            touch_last_x = x;
            touch_last_y = y;
            touch_start_ms = now_ms;
            is_dragging_list = false;
            is_horizontal_swipe = false;
            vel_scroll_y = 0.0f;
        } else {
            int16_t dy = y - touch_last_y;
            int16_t total_dy = y - touch_start_y;
            int16_t total_dx = x - touch_start_x;

            if (!is_dragging_list && (abs(total_dy) > 8 || abs(total_dx) > 10)) {
                is_dragging_list = true;
                if (abs(total_dx) > abs(total_dy) * 1.2f) {
                    is_horizontal_swipe = true;
                } else {
                    is_horizontal_swipe = false;
                }
            }

            if (is_dragging_list && !is_horizontal_swipe) {
                scroll_y += dy;
                vel_scroll_y = dy * 0.75f;
            }

            touch_last_x = x;
            touch_last_y = y;
        }
    } else {
        if (was_touching) {
            was_touching = false;
            int16_t total_dx = touch_last_x - touch_start_x;
            int16_t total_dy = touch_last_y - touch_start_y;
            uint32_t press_duration = now_ms - touch_start_ms;

            // 1. 横向滑动：左右切换 ALL 与 LIVE
            if (abs(total_dx) > 35 && abs(total_dx) > abs(total_dy) * 1.2f) {
                if (total_dx < 0) {
                    setFilterLiveOnly(true);
                    Serial.println("[Drawer Gesture] Swipe Left -> Switched to LIVE ONLY");
                } else {
                    setFilterLiveOnly(false);
                    Serial.println("[Drawer Gesture] Swipe Right -> Switched to ALL CAMERAS");
                }
                return -1;
            }

            // 2. 点击右侧空白遮罩区域：关闭抽屉
            if (touch_start_x > DRAWER_WIDTH && abs(total_dx) < 20 && abs(total_dy) < 20) {
                close();
                return -1;
            }

            // 3. 点击判定 (过滤开关或列表项)
            if (!is_dragging_list && press_duration < 350 && abs(total_dx) < 15 && abs(total_dy) < 15) {
                // A. 顶部胶囊过滤筛选开关点击判定 (放宽点击热区 Y: 32 ~ 78)
                if (touch_start_y >= 32 && touch_start_y <= 78 && touch_start_x < DRAWER_WIDTH) {
                    int16_t panel_x = (int)current_slide_x;
                    int16_t btn_mid = panel_x + DRAWER_WIDTH / 2;
                    bool select_live = (touch_start_x >= btn_mid);
                    setFilterLiveOnly(select_live);
                    Serial.printf("[Drawer Filter] Tap: %s\n", filter_live_only ? "LIVE ONLY" : "ALL");
                    return -1;
                }

                // B. 列表项点击判定 (Y: 82 ~ 415，每项 66px)
                if (touch_start_y >= LIST_TOP_Y && touch_start_y <= LIST_BOTTOM_Y && touch_start_x < DRAWER_WIDTH) {
                    int clicked_row = (int)(touch_start_y - LIST_TOP_Y - scroll_y) / ITEM_HEIGHT;
                    if (clicked_row >= 0 && clicked_row < (int)getFilteredCount()) {
                        int real_idx = getFilteredPlaceIndex(clicked_row);
                        if (real_idx >= 0 && real_idx < (int)GLOBAL_PLACES_COUNT) {
                            close(); // 选中后平滑缩回收起抽屉
                            return real_idx;
                        }
                    }
                }
            }
        }
    }

    return -1;
}
