#pragma once

#include <M5Unified.h>
#include "place_marker.h"

// -------------------------------------------------------------
// LiveEarth VideoPlayerView (Phase 5 - Step 3)
// 16:9 Max-Inscribed Fullscreen Player (Diagonal = 466px)
// Geometry: Width = 406px, Height = 228px, Diagonal = sqrt(406^2 + 228^2) = 466px
// -------------------------------------------------------------

class VideoPlayerView {
public:
    VideoPlayerView();

    void init();
    void update(uint32_t now_ms);

    // 渲染 406x228 黄金内接全屏播放器
    void render(M5GFX& display, const PlaceLocation* place);

    void requestRedraw() { needs_full_redraw = true; }

    // 几何尺寸常量
    static constexpr int16_t VIDEO_W = 406;
    static constexpr int16_t VIDEO_H = 228;
    static constexpr int16_t VIDEO_X = 30;  // (466 - 406) / 2
    static constexpr int16_t VIDEO_Y = 119; // (466 - 228) / 2

private:
    uint32_t anim_start_ms;
    float scan_pos;
    bool needs_full_redraw;
    const PlaceLocation* last_rendered_place;
    uint32_t simulated_seconds;
};
