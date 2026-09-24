#include "video_player_view.h"
#include "config.h"
#include "snapshot_player.h"
#include "network_manager.h"
#include <cmath>

#pragma GCC optimize ("O3,fast-math")

VideoPlayerView::VideoPlayerView()
    : anim_start_ms(0), scan_pos(0.0f), needs_full_redraw(true),
      last_rendered_place(nullptr), simulated_seconds(0) {
}

void VideoPlayerView::init() {
    anim_start_ms = millis();
    scan_pos = 0.0f;
    needs_full_redraw = true;
    last_rendered_place = nullptr;
    simulated_seconds = 1845;
}

void VideoPlayerView::update(uint32_t now_ms) {
    if (anim_start_ms == 0) anim_start_ms = now_ms;
    float dt = (now_ms - anim_start_ms) * 0.001f;
    anim_start_ms = now_ms;
    if (dt > 0.1f) dt = 0.1f;

    // 当网络新帧就绪时自动刷新全屏底图
    if (snapshot_player.hasNewFrame()) {
        needs_full_redraw = true;
    }

    scan_pos += dt * 70.0f;
    if (scan_pos >= VIDEO_H) {
        scan_pos -= VIDEO_H;
        simulated_seconds++;
    }
}

void VideoPlayerView::render(M5GFX& display, const PlaceLocation* place) {
    if (!place) return;

    bool has_stream = place->is_online && (place->snapshot_url != nullptr) && (strlen(place->snapshot_url) > 0);

    if (needs_full_redraw || place != last_rendered_place) {
        display.fillScreen(COLOR_BG);

        // 1. 顶部弧形 HUD (Y: 36 ~ 100)
        display.setTextDatum(middle_center);
        display.setTextSize(1);
        if (has_stream) {
            display.setTextColor(0x07E0, COLOR_BG);
            display.drawString("* LIVE BROADCAST STREAM *", SCREEN_CENTER_X, 42);
        } else {
            display.setTextColor(0xFDC0, COLOR_BG);
            display.drawString("* OFFICIAL PLACE DOSSIER *", SCREEN_CENTER_X, 42);
        }

        display.setTextSize(2);
        display.setTextColor(0xFFFF, COLOR_BG);
        display.drawString(place->name, SCREEN_CENTER_X, 70);

        char sub_buf[64];
        snprintf(sub_buf, sizeof(sub_buf), "%s  |  %s", place->country, place->cam_name);
        display.setTextSize(1);
        display.setTextColor(0xA554, COLOR_BG);
        display.drawString(sub_buf, SCREEN_CENTER_X, 96);

        // 2. 16:9 黄金内接视频视窗底色与微光科技框 (X: 30, Y: 119, W: 406, H: 228)
        if (has_stream && snapshot_player.hasValidFrame()) {
            snapshot_player.renderFullscreen(display, VIDEO_X, VIDEO_Y, VIDEO_W, VIDEO_H);
        } else {
            display.fillRect(VIDEO_X, VIDEO_Y, VIDEO_W, VIDEO_H, 0x0821);
            if (has_stream && snapshot_player.getState() == SNAP_STATE_FETCHING) {
                display.setTextDatum(middle_center);
                display.setTextSize(1);
                display.setTextColor(0x07FF, 0x0821);
                display.drawString("FETCHING HIGH RESOLUTION STREAM...", SCREEN_CENTER_X, VIDEO_Y + VIDEO_H / 2);
            } else if (!has_stream) {
                // 离线典藏卡大屏内容
                display.setTextDatum(middle_center);
                display.setTextSize(1);
                display.setTextColor(place->theme_color, 0x0821);
                display.drawString(place->local_name, SCREEN_CENTER_X, VIDEO_Y + 45);

                display.setTextColor(0xFFFF, 0x0821);
                char geo_buf[64];
                snprintf(geo_buf, sizeof(geo_buf), "%4.2f%c, %5.2f%c  |  ELEV: %dm  |  UTC%+d",
                         fabsf(place->latitude), (place->latitude >= 0 ? 'N' : 'S'),
                         fabsf(place->longitude), (place->longitude >= 0 ? 'E' : 'W'),
                         place->elevation_m, place->utc_offset);
                display.drawString(geo_buf, SCREEN_CENTER_X, VIDEO_Y + 75);

                display.setTextColor(0xD69A, 0x0821);
                char hl_buf[72];
                snprintf(hl_buf, sizeof(hl_buf), "%.48s", place->highlight);
                display.drawString(hl_buf, SCREEN_CENTER_X, VIDEO_Y + 115);

                display.setTextColor(0x4EEB, 0x0821);
                display.drawString("VERIFIED GLOBAL LANDMARK  *  STREAM IN AUDIT", SCREEN_CENTER_X, VIDEO_Y + 155);

                display.setTextColor(0x7BEF, 0x0821);
                char src_desc[64];
                snprintf(src_desc, sizeof(src_desc), "Authority: %s", place->cam_source);
                display.drawString(src_desc, SCREEN_CENTER_X, VIDEO_Y + 185);
            }
        }

        display.drawRect(VIDEO_X, VIDEO_Y, VIDEO_W, VIDEO_H, has_stream ? 0x3C19 : 0x2354);
        display.drawRect(VIDEO_X - 1, VIDEO_Y - 1, VIDEO_W + 2, VIDEO_H + 2, 0x1B55);

        // 四角 HUD 瞄准锁框
        int16_t b_len = 10;
        display.drawLine(VIDEO_X, VIDEO_Y, VIDEO_X + b_len, VIDEO_Y, 0xFFFF);
        display.drawLine(VIDEO_X, VIDEO_Y, VIDEO_X, VIDEO_Y + b_len, 0xFFFF);

        display.drawLine(VIDEO_X + VIDEO_W - 1, VIDEO_Y, VIDEO_X + VIDEO_W - 1 - b_len, VIDEO_Y, 0xFFFF);
        display.drawLine(VIDEO_X + VIDEO_W - 1, VIDEO_Y, VIDEO_X + VIDEO_W - 1, VIDEO_Y + b_len, 0xFFFF);

        display.drawLine(VIDEO_X, VIDEO_Y + VIDEO_H - 1, VIDEO_X + b_len, VIDEO_Y + VIDEO_H - 1, 0xFFFF);
        display.drawLine(VIDEO_X, VIDEO_Y + VIDEO_H - 1, VIDEO_X, VIDEO_Y + VIDEO_H - 1 - b_len, 0xFFFF);

        display.drawLine(VIDEO_X + VIDEO_W - 1, VIDEO_Y + VIDEO_H - 1, VIDEO_X + VIDEO_W - 1 - b_len, VIDEO_Y + VIDEO_H - 1, 0xFFFF);
        display.drawLine(VIDEO_X + VIDEO_W - 1, VIDEO_Y + VIDEO_H - 1, VIDEO_X + VIDEO_W - 1, VIDEO_Y + VIDEO_H - 1 - b_len, 0xFFFF);

        // 水印与合规背书
        display.setTextDatum(top_left);
        display.setTextColor(0x7BEF, 0x0821);
        char wm_buf[64];
        snprintf(wm_buf, sizeof(wm_buf), "FEED: %s", place->cam_source);
        display.drawString(wm_buf, VIDEO_X + 12, VIDEO_Y + 10);

        // 3. 底部弧形控制提示 (Y: 365 ~ 430)
        display.setTextDatum(middle_center);
        display.setTextColor(0x7BEF, COLOR_BG);
        display.drawString("DIAGONAL: 466px (MAX INSCRIBED 16:9)", SCREEN_CENTER_X, 375);

        display.setTextColor(COLOR_ACCENT_CYAN, COLOR_BG);
        display.drawString("[TAP SCREEN OR PRESS BTNA TO RETURN]", SCREEN_CENTER_X, 405);

        display.drawArc(SCREEN_CENTER_X, SCREEN_CENTER_Y, 226, 224, 60, 120, 0x1B55);

        needs_full_redraw = false;
        last_rendered_place = place;
    }

    // 动态帧元素：实时时间码与状态码 (不破坏视频底图)
    if (has_stream) {
        char stat_buf[32];
        if (snapshot_player.getState() == SNAP_STATE_FETCHING) {
            snprintf(stat_buf, sizeof(stat_buf), "LIVE #%02u | SYNCING", (unsigned int)(snapshot_player.getFrameSeq()));
        } else {
            snprintf(stat_buf, sizeof(stat_buf), "LIVE #%02u | 4.6Mbps", (unsigned int)(snapshot_player.getFrameSeq()));
        }
        display.setTextDatum(top_right);
        display.setTextColor(0x4EEB, 0x0821);
        display.drawString(stat_buf, VIDEO_X + VIDEO_W - 22, VIDEO_Y + 10);
        display.fillCircle(VIDEO_X + VIDEO_W - 12, VIDEO_Y + 14, (millis() % 1000 < 500) ? 3 : 2, 0x07E0);

        uint32_t secs = simulated_seconds % 60;
        uint32_t mins = (simulated_seconds / 60) % 60;
        uint32_t hrs  = (simulated_seconds / 3600) % 24;
        char timecode[32];
        snprintf(timecode, sizeof(timecode), "LIVE %02u:%02u:%02u", hrs, mins, secs);

        display.setTextDatum(bottom_left);
        display.setTextColor(0xFFFF, 0x0821);
        display.drawString(timecode, VIDEO_X + 12, VIDEO_Y + VIDEO_H - 10);

        display.setTextDatum(bottom_right);
        display.setTextColor(0x07E0, 0x0821);
        display.drawString("AUDIO OK", VIDEO_X + VIDEO_W - 12, VIDEO_Y + VIDEO_H - 10);
    } else {
        display.setTextDatum(top_right);
        display.setTextColor(0xFDC0, 0x0821);
        display.drawString("ARCHIVE", VIDEO_X + VIDEO_W - 12, VIDEO_Y + 10);

        display.setTextDatum(bottom_left);
        display.setTextColor(0x7BEF, 0x0821);
        display.drawString("AUDIT QUEUE #2026", VIDEO_X + 12, VIDEO_Y + VIDEO_H - 10);

        display.setTextDatum(bottom_right);
        display.setTextColor(0x4EEB, 0x0821);
        display.drawString("100 PLACES", VIDEO_X + VIDEO_W - 12, VIDEO_Y + VIDEO_H - 10);
    }
}
