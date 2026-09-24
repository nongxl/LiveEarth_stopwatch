#pragma once

#include <M5Unified.h>
#include "place_marker.h"

// -------------------------------------------------------------
// LiveEarth PlaceDetailView (Phase 5 - Step 2 Enhanced)
// Large-Text Readability & Dynamic Globe Avoidance Layout
// -------------------------------------------------------------

class PlaceDetailView {
public:
    PlaceDetailView();

    void init();
    void update(uint32_t now_ms);

    // 渲染中等视频预览窗口及大字号易读多维信息
    void render(M5GFX& display, const PlaceLocation* place);

    void requestRedraw() { needs_full_redraw = true; }

    // 中等视频预览窗几何常量 (16:9 比例，宽 240，高 135，大触摸热区)
    static constexpr int16_t PREVIEW_W = 240;
    static constexpr int16_t PREVIEW_H = 135;
    static constexpr int16_t PREVIEW_X = 113; // (466 - 240) / 2
    static constexpr int16_t PREVIEW_Y = 122;

    // 点击命中检测：扩大触控判定裕度，极易点击操作
    static bool isInsidePreviewWindow(int16_t tx, int16_t ty) {
        return (tx >= PREVIEW_X - 12 && tx <= PREVIEW_X + PREVIEW_W + 12 &&
                ty >= PREVIEW_Y - 12 && ty <= PREVIEW_Y + PREVIEW_H + 12);
    }

private:
    uint32_t anim_start_ms;
    float pulse_phase;
    float scanline_y;
    bool needs_full_redraw;
    const PlaceLocation* last_rendered_place;

    void drawTopInfo(M5GFX& display, const PlaceLocation* place);
    void drawLeftPanel(M5GFX& display, const PlaceLocation* place);
    void drawRightPanel(M5GFX& display, const PlaceLocation* place);
    void drawMediumPreviewWindow(M5GFX& display, const PlaceLocation* place);
    void drawFooterGuidance(M5GFX& display);
};
