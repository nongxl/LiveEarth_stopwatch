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


    display.setClipRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    display.setAddrWindow(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

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

        // 水印与来源信息 (视频视窗上方，绝不遮挡视频)
        char wm_buf[64];
        snprintf(wm_buf, sizeof(wm_buf), "FEED: %s", place->cam_source);
        display.setTextColor(0x7BEF, COLOR_BG);
        display.drawString(wm_buf, SCREEN_CENTER_X, 110);

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

        // 视窗边框
        display.drawRect(VIDEO_X, VIDEO_Y, VIDEO_W, VIDEO_H, has_stream ? 0x3C19 : 0x2354);
        display.drawRect(VIDEO_X - 1, VIDEO_Y - 1, VIDEO_W + 2, VIDEO_H + 2, 0x1B55);

        // 3. 底部弧形控制提示 (Y: 365 ~ 430) - 视频视窗下方
        display.setTextDatum(middle_center);
        display.setTextColor(COLOR_ACCENT_CYAN, COLOR_BG);
        display.drawString("[TAP SCREEN OR PRESS BTNA TO RETURN]", SCREEN_CENTER_X, 398);

        display.drawArc(SCREEN_CENTER_X, SCREEN_CENTER_Y, 226, 224, 60, 120, 0x1B55);

        needs_full_redraw = false;
        last_rendered_place = place;
    }

    // 动态帧元素：在视频视窗正上方/正下方显示，不遮挡画面
    if (has_stream) {
        uint32_t interval_ms = snapshot_player.getLastFrameIntervalMs();

        // 帧率文字显示在视窗正下方 (Y: VIDEO_Y + VIDEO_H + 4 ~ 360)
        display.setTextDatum(bottom_left);
        display.setFont(&fonts::Font0);
        display.setTextSize(1);

        // 帧序号 + 实际刷新间隔 (左侧)
        char stat_buf[48];
        if (strstr(place->id, "kilauea") || strstr(place->id, "maunakea")) {
            snprintf(stat_buf, sizeof(stat_buf), "USGS #%02u", (unsigned int)(snapshot_player.getFrameSeq()));
        } else if (snapshot_player.getState() == SNAP_STATE_FETCHING) {
            snprintf(stat_buf, sizeof(stat_buf), "LIVE #%02u | SYNCING...", (unsigned int)(snapshot_player.getFrameSeq()));
        } else {
            snprintf(stat_buf, sizeof(stat_buf), "LIVE #%02u", (unsigned int)(snapshot_player.getFrameSeq()));
        }
        display.setTextColor(0x07FF, COLOR_BG);
        display.drawString(stat_buf, VIDEO_X, VIDEO_Y + VIDEO_H + 12);

        // 实际帧率 (右侧) - 显示两帧之间的真实刷新时间
        char fps_buf[32];
        if (interval_ms > 0 && interval_ms < 120000) {
            float fps = 1000.0f / (float)interval_ms;
            if (fps >= 0.1f) {
                snprintf(fps_buf, sizeof(fps_buf), "%.2f s/f", interval_ms / 1000.0f);
            } else {
                snprintf(fps_buf, sizeof(fps_buf), "~%us/f", (unsigned int)(interval_ms / 1000));
            }
        } else {
            snprintf(fps_buf, sizeof(fps_buf), "WAITING...");
        }
        display.setTextDatum(bottom_right);
        display.setTextColor(0x07E0, COLOR_BG);
        display.drawString(fps_buf, VIDEO_X + VIDEO_W, VIDEO_Y + VIDEO_H + 12);

        // 闪烁 LIVE 指示点 (左上角，视窗外紧贴边框上方)
        display.fillCircle(VIDEO_X + 6, VIDEO_Y - 7, (millis() % 1000 < 500) ? 4 : 3,
                           (millis() % 1000 < 500) ? 0xF940 : 0xFD00);
        display.setTextDatum(top_left);
        display.setTextColor(0xF940, COLOR_BG);
        display.drawString("LIVE", VIDEO_X + 14, VIDEO_Y - 12);

    } else {
        // 归档卡视窗下方标注
        display.setTextDatum(bottom_left);
        display.setFont(&fonts::Font0);
        display.setTextSize(1);
        display.setTextColor(0x7BEF, COLOR_BG);
        display.drawString("ARCHIVE #2026", VIDEO_X, VIDEO_Y + VIDEO_H + 12);

        display.setTextDatum(bottom_right);
        display.setTextColor(0x4EEB, COLOR_BG);
        display.drawString("100 PLACES", VIDEO_X + VIDEO_W, VIDEO_Y + VIDEO_H + 12);
    }
}
