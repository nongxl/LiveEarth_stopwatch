#include "camera_drawer_view.h"
#include "config.h"
#include <cmath>

CameraDrawerView camera_drawer;

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
      pulse_phase(0.0f)
{
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
    pulse_phase = 0.0f;
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
}

void CameraDrawerView::render(M5GFX& display, const PlaceMarkerManager& place_mgr) {
    if (state == DRAWER_CLOSED) return;

    int panel_x = (int)current_slide_x;
    int selected_idx = place_mgr.getSelectedIndex();

    // 1. 抽屉背景与右侧阴影遮罩
    display.fillRect(panel_x, 0, DRAWER_WIDTH, SCREEN_HEIGHT, 0x0821); // 深沉暗黑科技底板
    // 右边缘垂直霓虹微光线
    display.drawFastVLine(panel_x + DRAWER_WIDTH - 1, 0, SCREEN_HEIGHT, COLOR_ACCENT_CYAN);
    display.drawFastVLine(panel_x + DRAWER_WIDTH - 2, 0, SCREEN_HEIGHT, 0x1B55);

    // 抽屉右侧半透明暗黑遮罩（点击即可关闭抽屉）
    if (panel_x + DRAWER_WIDTH < SCREEN_WIDTH) {
        display.fillRect(panel_x + DRAWER_WIDTH, 0, SCREEN_WIDTH - (panel_x + DRAWER_WIDTH), SCREEN_HEIGHT, COLOR_BG);
    }

    // 2. 顶部导航 HUD 与过滤筛选开关 (Y: 28 ~ 78)
    size_t online_count = 0;
    for (size_t i = 0; i < GLOBAL_PLACES_COUNT; i++) {
        if (GLOBAL_PLACES[i].is_online) online_count++;
    }

    display.setTextDatum(middle_center);
    display.setTextSize(1);
    display.setTextColor(COLOR_ACCENT_CYAN, 0x0821);
    display.drawString("* CAMERA DIRECTORY *", panel_x + DRAWER_WIDTH / 2, 32);

    // 科技双选过滤胶囊开关 (Y: 46 ~ 72, 宽 240px, 高 26px, 圆角 13px)
    int16_t toggle_w = 240;
    int16_t toggle_h = 26;
    int16_t toggle_x = panel_x + (DRAWER_WIDTH - toggle_w) / 2;
    int16_t toggle_y = 46;
    int16_t half_w = toggle_w / 2;

    // 胶囊底座与微光科技框
    display.fillRoundRect(toggle_x, toggle_y, toggle_w, toggle_h, 13, 0x0842);
    display.drawRoundRect(toggle_x, toggle_y, toggle_w, toggle_h, 13, 0x2354);

    if (!filter_live_only) {
        // [ ALL (107) ] 选中态高亮
        display.fillRoundRect(toggle_x + 2, toggle_y + 2, half_w - 2, toggle_h - 4, 11, 0x1B55);
        display.drawRoundRect(toggle_x + 2, toggle_y + 2, half_w - 2, toggle_h - 4, 11, COLOR_ACCENT_CYAN);
        
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
        display.fillRoundRect(toggle_x + half_w, toggle_y + 2, half_w - 2, toggle_h - 4, 11, 0x0320);
        display.drawRoundRect(toggle_x + half_w, toggle_y + 2, half_w - 2, toggle_h - 4, 11, 0x07E0);

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

    // 3. 滚动地点列表 (Y: 82 ~ 415)
    // 根据当前筛选状态动态渲染过滤列表
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

        // 选中项绘制发光胶囊底板
        if (is_sel) {
            display.fillRoundRect(panel_x + 12, item_y + 2, DRAWER_WIDTH - 24, ITEM_HEIGHT - 4, 4, 0x10A2);
            display.drawRoundRect(panel_x + 12, item_y + 2, DRAWER_WIDTH - 24, ITEM_HEIGHT - 4, 4, place.is_online ? 0x07E0 : place.theme_color);
        } else {
            // 未选中项隔行微弱底色或分割线
            display.drawFastHLine(panel_x + 20, item_y + ITEM_HEIGHT - 1, DRAWER_WIDTH - 40, 0x1082);
        }

        // 序号徽章 (例如 01, 02...)
        char num_buf[8];
        if (filter_live_only) {
            snprintf(num_buf, sizeof(num_buf), "%02u", (uint32_t)(row + 1));
        } else {
            snprintf(num_buf, sizeof(num_buf), "%02u", (uint32_t)(real_idx + 1));
        }
        display.setTextDatum(middle_left);
        display.setTextSize(1);
        display.setTextColor(is_sel ? 0xFFFF : 0x7BEF, is_sel ? 0x10A2 : 0x0821);
        display.drawString(num_buf, panel_x + 20, item_y + ITEM_HEIGHT / 2);

        // 主题色微光星核
        display.fillCircle(panel_x + 46, item_y + ITEM_HEIGHT / 2, is_sel ? 4 : 3, place.is_online ? 0x07E0 : place.theme_color);
        if (is_sel) {
            display.drawPixel(panel_x + 46, item_y + ITEM_HEIGHT / 2, 0xFFFF);
        }

        // 地点英文大名称 (清晰纯白加粗)
        display.setTextDatum(top_left);
        display.setTextSize(1);
        display.setTextColor(is_sel ? 0xFFFF : 0xD69A, is_sel ? 0x10A2 : 0x0821);
        display.drawString(place.name, panel_x + 60, item_y + 8);

        // 副标题：国家与摄像头名
        char sub_buf[64];
        snprintf(sub_buf, sizeof(sub_buf), "%s | %s", place.country, place.cam_name);
        display.setTextColor(is_sel ? (place.is_online ? 0x07E0 : place.theme_color) : 0x8410, is_sel ? 0x10A2 : 0x0821);
        display.drawString(sub_buf, panel_x + 60, item_y + 26);

        // 状态徽标与选中标记
        display.setTextDatum(middle_right);
        if (is_sel) {
            display.setTextColor(place.is_online ? 0x07E0 : COLOR_ACCENT_CYAN, 0x10A2);
            display.drawString(place.is_online ? "LIVE >" : "FOCUS >", panel_x + DRAWER_WIDTH - 20, item_y + ITEM_HEIGHT / 2);
        } else if (place.is_online) {
            display.setTextColor(0x07E0, 0x0821);
            display.drawString("* LIVE", panel_x + DRAWER_WIDTH - 20, item_y + ITEM_HEIGHT / 2);
        }
    }

    // 4. 底部指示 HUD (Y: 422 ~ 455)
    display.drawFastHLine(panel_x + 16, 420, DRAWER_WIDTH - 32, 0x2354);
    display.setTextDatum(middle_center);
    display.setTextSize(1);
    display.setTextColor(0xA554, 0x0821);
    display.drawString("< SWIPE: SCROLL | TAP: FLY & ZOOM >", panel_x + DRAWER_WIDTH / 2, 436);
}

void CameraDrawerView::renderEdgeHandle(M5GFX& display) {
    if (state != DRAWER_CLOSED) return;

    // 在屏幕左边缘居中 (X: 0~12, Y: 213~253) 呈现优雅的赛博微光拉手把柄 (绝对杜绝负坐标越界)
    float pulse = 0.5f + 0.5f * sinf(pulse_phase);
    uint16_t handle_col = (pulse > 0.4f) ? COLOR_ACCENT_CYAN : 0x2354;

    display.fillRoundRect(0, 213, 12, 40, 5, 0x0842);
    display.drawRoundRect(0, 213, 12, 40, 5, handle_col);

    display.setTextDatum(middle_center);
    display.setTextSize(1);
    display.setTextColor(0xFFFF, 0x0842);
    display.drawString(">", 6, 233);
}

int CameraDrawerView::handleTouch(bool is_pressed, int16_t x, int16_t y, uint32_t now_ms) {
    if (state == DRAWER_CLOSED) {
        // 如果在最左边缘 (X < 45) 向右滑动或轻按把手，唤出抽屉
        if (is_pressed) {
            if (!was_touching) {
                was_touching = true;
                touch_start_x = x;
                touch_start_y = y;
                touch_last_x = x;
                touch_last_y = y;
                touch_start_ms = now_ms;
            } else {
                int16_t dx = x - touch_start_x;
                if (touch_start_x < 50 && dx > 30) {
                    open();
                    was_touching = false;
                }
            }
        } else {
            if (was_touching) {
                // 点击左侧把手区域也可以直接打开
                if (touch_start_x < 36 && abs(touch_start_y - 233) < 35) {
                    open();
                }
                was_touching = false;
            }
        }
        return -1;
    }

    if (state != DRAWER_OPEN) {
        return -1; // 动画过渡中不接受列表点击
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
            vel_scroll_y = 0.0f;
        } else {
            int16_t dy = y - touch_last_y;
            int16_t total_dy = y - touch_start_y;
            int16_t total_dx = x - touch_start_x;

            if (!is_dragging_list && (abs(total_dy) > 10 || abs(total_dx) > 15)) {
                is_dragging_list = true;
            }

            if (is_dragging_list) {
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

            // 1. 向左轻扫手势：直接关闭抽屉
            if (total_dx < -40 && abs(total_dx) > abs(total_dy)) {
                close();
                return -1;
            }

            // 2. 点击右侧空白遮罩区域：关闭抽屉
            if (touch_start_x > DRAWER_WIDTH && abs(total_dx) < 15 && abs(total_dy) < 15) {
                close();
                return -1;
            }

            // 3. 点击判定 (过滤开关或列表项)
            if (!is_dragging_list && press_duration < 350 && abs(total_dx) < 15 && abs(total_dy) < 15) {
                // A. 顶部胶囊过滤筛选开关点击判定 (Y: 40 ~ 78)
                if (touch_start_y >= 40 && touch_start_y <= 78 && touch_start_x < DRAWER_WIDTH) {
                    int16_t panel_x = (int)current_slide_x;
                    int16_t btn_mid = panel_x + DRAWER_WIDTH / 2;
                    bool select_live = (touch_start_x >= btn_mid);
                    setFilterLiveOnly(select_live);
                    Serial.printf("[Drawer Filter] Mode: %s\n", filter_live_only ? "LIVE ONLY" : "ALL");
                    return -1; // 消费点击事件，抽屉保持打开供选择
                }

                // B. 列表项点击判定 (Y: 82 ~ 415)
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
