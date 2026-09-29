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

struct ThumbSlot {
    int place_idx;
    uint16_t* pixels;
    uint32_t last_used;
    bool is_ready;
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

    // 逻辑更新 (动画推进与惯性衰减，以及顺序后台预热调度)
    void update(uint32_t now_ms);

    // 绘制抽屉面板及内部列表 (每行包含 16:9 实时微缩视窗)
    void render(M5GFX& display, const PlaceMarkerManager& place_mgr);

    // 绘制关闭状态下的左侧边缘微光唤出拉手
    void renderEdgeHandle(M5GFX& display);

    // 触控交互处理：返回 >=0 表示用户选中了该地点索引，-1 表示未选或仅滑动
    int handleTouch(bool is_pressed, int16_t x, int16_t y, uint32_t now_ms);

    // 请求滚动到当前选中的地点项
    void scrollToSelected(int selected_idx);

    // 实体按键步进选择下一项：返回新选中的地点全局索引 (>=0)，若列表为空返回 -1
    int selectNextItem(int current_selected_idx);

    // 过滤功能：仅查看拥有 LIVE 实时流的目标
    bool isFilterLiveOnly() const { return filter_live_only; }
    void setFilterLiveOnly(bool live_only);
    void toggleFilter();

    size_t getFilteredCount() const;
    int getFilteredPlaceIndex(int filtered_row) const;

    // 缩略图获取与共享：外部（如 SnapshotPlayer）可直接复用该缩略图实现瞬间秒开！
    uint16_t* getThumbnail(int place_idx);

    static constexpr int16_t THUMB_W = 84;
    static constexpr int16_t THUMB_H = 48;
    static constexpr int16_t ITEM_HEIGHT = 66; // 66px * 5 = 330px，一屏整齐排布 5 条数据
    static constexpr int16_t LIST_TOP_Y = 82;
    static constexpr int16_t LIST_BOTTOM_Y = 415;
    static constexpr int16_t LIST_VISIBLE_H = LIST_BOTTOM_Y - LIST_TOP_Y;

private:
    DrawerAnimState state;
    uint32_t anim_start_ms;
    float current_slide_x; // 当前 X 轴滑出位置: -DRAWER_WIDTH ~ 0

    bool filter_live_only; // 是否仅显示 LIVE 在线摄像头

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
    bool is_horizontal_swipe;

    float pulse_phase;

    static constexpr int16_t DRAWER_WIDTH = 345;

    // 缩略图 LRU 缓存池
    static constexpr size_t THUMB_SLOTS_NUM = 8;
    ThumbSlot thumb_slots[THUMB_SLOTS_NUM];
    SemaphoreHandle_t slots_mutex;
    int findSlotForPlace(int place_idx);
    ThumbSlot* allocateSlotForPlace(int place_idx);

    // 顺序单并发后台预热状态
    volatile bool prefetch_running;
    volatile int pending_prefetch_idx;
    uint32_t last_prefetch_attempt_ms;
    TaskHandle_t prefetch_task_handle;
    uint32_t place_fail_cd[128]; // 失败冷却保护，避免单个故障摄像头阻塞队列

    void checkAndPrefetchVisible();
    static void prefetchTaskStatic(void* param);
    void prefetchTaskWorker();
};

extern CameraDrawerView camera_drawer;
