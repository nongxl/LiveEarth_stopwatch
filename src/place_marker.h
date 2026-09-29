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
    int elevation_m;
    int8_t utc_offset;
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
    ~PlaceMarkerManager();

    void init();

    // 从本地 LittleFS 加载 /places.json (若不存在或解析失败回退至默认硬编码)
    bool loadFromLittleFS(const char* filepath = "/places.json");

    // 从 JSON 字符串解析并原子切换地点池 (在 PSRAM 中分配)
    bool loadFromJson(const char* json_str, size_t json_len);

    // 重置回固件硬编码默认地点
    void resetToDefault();

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

    bool isUsingDynamicPlaces() const { return is_dynamic; }

private:
    float pulse_phase;
    int selected_idx;
    uint32_t last_anim_ms;

    bool is_dynamic;
    PlaceLocation* dynamic_places;
    size_t dynamic_count;
    char* dynamic_str_pool;

    void freeDynamicMemory();
};

extern const PlaceLocation DEFAULT_GLOBAL_PLACES[];
extern const size_t DEFAULT_GLOBAL_PLACES_COUNT;

// 当前活跃的地点数组（启动时指向硬编码默认值，若 LittleFS 或网络存在则指向动态加载的地点池）
extern const PlaceLocation* GLOBAL_PLACES;
extern size_t GLOBAL_PLACES_COUNT;

extern PlaceMarkerManager place_mgr;

