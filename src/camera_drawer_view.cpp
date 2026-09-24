#include "camera_drawer_view.h"
#include "config.h"
#include <cmath>

CameraDrawerView camera_drawer;

CameraDrawerView::CameraDrawerView()
    : state(DRAWER_CLOSED),
      anim_start_ms(0),
      current_slide_x(-DRAWER_WIDTH),
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
    scroll_y = 0.0f;
    target_scroll_y = 0.0f;
    vel_scroll_y = 0.0f;
    was_touching = false;
    is_dragging_list = false;
    pulse_phase = 0.0f;
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
    float desired_y = -(selected_idx * ITEM_HEIGHT) + (LIST_VISIBLE_H / 2) - (ITEM_HEIGHT / 2);
    float max_scroll = 0.0f;
    float min_scroll = -((int)GLOBAL_PLACES_COUNT * ITEM_HEIGHT - LIST_VISIBLE_H);
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
        float max_scroll = 0.0f;
        float min_scroll = -((int)GLOBAL_PLACES_COUNT * ITEM_HEIGHT - LIST_VISIBLE_H);
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

    // 2. 顶部导航 HUD (Y: 28 ~ 78)
    display.setTextDatum(middle_center);
    display.setTextSize(1);
    display.setTextColor(COLOR_ACCENT_CYAN, 0x0821);
    display.drawString("* CAMERA DIRECTORY *", panel_x + DRAWER_WIDTH / 2, 34);

    display.setTextColor(0x07E0, 0x0821);
    display.drawString("* 3 VERIFIED LIVE BROADCASTS *", panel_x + DRAWER_WIDTH / 2, 50);

    display.setTextColor(0xA554, 0x0821);
    display.drawString("100 GLOBAL LOCATIONS", panel_x + DRAWER_WIDTH / 2, 66);

    display.drawFastHLine(panel_x + 16, 76, DRAWER_WIDTH - 32, 0x2354);

    // 3. 滚动地点列表 (Y: 82 ~ 415)
    // 采用视口局部直接绘制，超限跳过，极高刷新效率
    for (size_t i = 0; i < GLOBAL_PLACES_COUNT; i++) {
        int item_y = LIST_TOP_Y + (int)scroll_y + (int)i * ITEM_HEIGHT;
        if (item_y + ITEM_HEIGHT < LIST_TOP_Y || item_y > LIST_BOTTOM_Y) {
            continue; // 视野外剔除
        }

        const PlaceLocation& place = GLOBAL_PLACES[i];
        bool is_sel = ((int)i == selected_idx);

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
        snprintf(num_buf, sizeof(num_buf), "%02u", (uint32_t)(i + 1));
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

            // 3. 点击列表项判定
            if (!is_dragging_list && press_duration < 350 && abs(total_dx) < 15 && abs(total_dy) < 15) {
                if (touch_start_y >= LIST_TOP_Y && touch_start_y <= LIST_BOTTOM_Y && touch_start_x < DRAWER_WIDTH) {
                    int clicked_idx = (int)(touch_start_y - LIST_TOP_Y - scroll_y) / ITEM_HEIGHT;
                    if (clicked_idx >= 0 && clicked_idx < (int)GLOBAL_PLACES_COUNT) {
                        close(); // 选中后平滑缩回收起抽屉
                        return clicked_idx;
                    }
                }
            }
        }
    }

    return -1;
}
