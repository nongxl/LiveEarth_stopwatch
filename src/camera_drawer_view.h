#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include "place_marker.h"

enum DrawerAnimState {
    DRAWER_CLOSED = 0,
    DRAWER_OPENING,
    DRAWER_OPEN,
    DRAWER_CLOSING
};

class CameraDrawerView {
public:
    CameraDrawerView();

    void init();
    void open();
    void close();
    void toggle();

    bool isOpen() const { return state == DRAWER_OPEN; }
    bool isVisible() const { return state != DRAWER_CLOSED; }

    // 逻辑更新 (动画推进与惯性衰减)
    void update(uint32_t now_ms);

    // 绘制抽屉面板及内部列表 (直接利用视口高效渲染)
    void render(M5GFX& display, const PlaceMarkerManager& place_mgr);

    // 绘制关闭状态下的左侧边缘微光唤出拉手
    void renderEdgeHandle(M5GFX& display);

    // 触控交互处理：返回 >=0 表示用户选中了该地点索引，-1 表示未选或仅滑动
    int handleTouch(bool is_pressed, int16_t x, int16_t y, uint32_t now_ms);

    // 请求滚动到当前选中的地点项
    void scrollToSelected(int selected_idx);

private:
    DrawerAnimState state;
    uint32_t anim_start_ms;
    float current_slide_x; // 当前 X 轴滑出位置: -DRAWER_WIDTH ~ 0

    // 列表滚动状态
    float scroll_y;
    float target_scroll_y;
    float vel_scroll_y;

    // 触控手势跟踪
    bool was_touching;
    int16_t touch_start_x;
    int16_t touch_start_y;
    int16_t touch_last_x;
    int16_t touch_last_y;
    uint32_t touch_start_ms;
    bool is_dragging_list;

    float pulse_phase;

    static constexpr int16_t DRAWER_WIDTH = 345;
    static constexpr int16_t ITEM_HEIGHT = 48;
    static constexpr int16_t LIST_TOP_Y = 82;
    static constexpr int16_t LIST_BOTTOM_Y = 415;
    static constexpr int16_t LIST_VISIBLE_H = LIST_BOTTOM_Y - LIST_TOP_Y;
};

extern CameraDrawerView camera_drawer;
