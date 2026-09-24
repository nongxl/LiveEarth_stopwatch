#pragma once

#include <M5Unified.h>
#include "place_marker.h"

// -------------------------------------------------------------
// LiveEarth PlaceDetailView (Phase 5 - Step 2)
// Medium Video Preview Window with Top, Left, Right Information HUD
// -------------------------------------------------------------

class PlaceDetailView {
public:
    PlaceDetailView();

    void init();
    void update(uint32_t now_ms);

    // 渲染中等视频预览窗口及上、左、右多维环绕信息
    void render(M5GFX& display, const PlaceLocation* place);

    void requestRedraw() { needs_full_redraw = true; }

    // 中等视频预览窗几何常量 (16:9)
    static constexpr int16_t PREVIEW_W = 224;
    static constexpr int16_t PREVIEW_H = 126;
    static constexpr int16_t PREVIEW_X = 121; // (466 - 224) / 2
    static constexpr int16_t PREVIEW_Y = 135;

    // 点击命中检测：是否点击了中央中等视频窗口
    static bool isInsidePreviewWindow(int16_t tx, int16_t ty) {
        return (tx >= PREVIEW_X - 6 && tx <= PREVIEW_X + PREVIEW_W + 6 &&
                ty >= PREVIEW_Y - 6 && ty <= PREVIEW_Y + PREVIEW_H + 6);
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
