#pragma once

#include <M5Unified.h>
#include "place_marker.h"

// -------------------------------------------------------------
// LiveEarth PlaceDetailView (Phase 5)
// Dedicated Circular AMOLED Experience for Location & Camera Exploration
// -------------------------------------------------------------

enum DetailViewAction {
    ACTION_NONE = 0,
    ACTION_BACK_TO_GLOBE = 1,
    ACTION_PREV_PLACE = 2,
    ACTION_NEXT_PLACE = 3,
    ACTION_VIEW_CAM = 4
};

class PlaceDetailView {
public:
    PlaceDetailView();

    void init();
    void update(uint32_t now_ms);

    // 渲染全屏 466x466 圆形地点详情视图
    void render(M5GFX& display, const PlaceLocation* place);

    // 标记需要重绘（如地点切换时）
    void requestRedraw() { needs_full_redraw = true; }

private:
    uint32_t anim_start_ms;
    float pulse_phase;
    bool needs_full_redraw;
    const PlaceLocation* last_rendered_place;

    void drawHeader(M5GFX& display, const PlaceLocation* place);
    void drawCoreInfo(M5GFX& display, const PlaceLocation* place);
    void drawDescription(M5GFX& display, const PlaceLocation* place);
    void drawCameraCard(M5GFX& display, const PlaceLocation* place);
    void drawFooterControls(M5GFX& display, const PlaceLocation* place);
};
