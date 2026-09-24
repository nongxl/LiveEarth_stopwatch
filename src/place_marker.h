#pragma once

#include <M5Unified.h>
#include "globe.h"

// -------------------------------------------------------------
// LiveEarth Geographic Place Marker System (Phase 3 & Phase 4)
// Handles 3D spherical projection, occlusion culling, and glowing HUD
// -------------------------------------------------------------

struct PlaceLocation {
    const char* id;
    const char* name;
    const char* local_name;
    const char* country;
    const char* region;
    float latitude;
    float longitude;
    const char* category;
    uint16_t theme_color;
    const char* description;
    const char* highlight;
    const char* cam_name;
    const char* cam_source;
    const char* snapshot_url;
    int cam_count;
    bool is_online;
};

class PlaceMarkerManager {
public:
    PlaceMarkerManager();

    void init();

    // 更新动画状态
    void update(uint32_t now_ms);

    // 渲染所有正面可见的发光地点 Markers 到 3D 地球视口 Canvas
    void renderMarkers(M5Canvas& canvas, const GlobeEngine& globe);

    // 触控命中检测（根据全屏触控坐标匹配最近的可见 Marker，返回其索引，未命中返回 -1）
    // 注意：Canvas 在屏幕上的偏移量 (GLOBE_CANVAS_OFFSET_X, GLOBE_CANVAS_OFFSET_Y) 会在内部自动转换
    int hitTest(int16_t screen_touch_x, int16_t screen_touch_y, const GlobeEngine& globe, float max_radius_px = 28.0f);

    // 获取地点信息
    const PlaceLocation* getPlace(int index) const;
    const PlaceLocation* getSelectedPlace() const;
    size_t getCount() const;

    void setSelectedIndex(int idx);
    int getSelectedIndex() const { return selected_idx; }
    void nextPlace();
    void prevPlace();

private:
    float pulse_phase;
    int selected_idx;
    uint32_t last_anim_ms;
};

