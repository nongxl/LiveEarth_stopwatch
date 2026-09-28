#include "place_detail_view.h"
#include "config.h"
#include "snapshot_player.h"
#include "network_manager.h"
#include <cmath>

#pragma GCC optimize ("O3,fast-math")

PlaceDetailView::PlaceDetailView()
    : anim_start_ms(0), pulse_phase(0.0f), scanline_y(0.0f),
      needs_full_redraw(true), last_rendered_place(nullptr) {
}

void PlaceDetailView::init() {
    anim_start_ms = millis();
    pulse_phase = 0.0f;
    scanline_y = 0.0f;
    needs_full_redraw = true;
    last_rendered_place = nullptr;
}

void PlaceDetailView::update(uint32_t now_ms) {
    if (anim_start_ms == 0) anim_start_ms = now_ms;
    float dt = (now_ms - anim_start_ms) * 0.001f;
    anim_start_ms = now_ms;
    if (dt > 0.1f) dt = 0.1f;

    // 当网络后台成功下载新快照帧时，立即触发全量重绘刷新上屏
    if (snapshot_player.hasNewFrame()) {
        needs_full_redraw = true;
    }

    // 呼吸灯与扫描线动画
    pulse_phase += dt * 3.5f;
    if (pulse_phase >= 6.2831853f) pulse_phase -= 6.2831853f;

    scanline_y += dt * 55.0f;
    if (scanline_y >= PREVIEW_H) scanline_y -= PREVIEW_H;
}

void PlaceDetailView::drawTopInfo(M5GFX& display, const PlaceLocation* place) {
    // 1. 顶部表盘微光弧 (R: 226)
    display.drawArc(SCREEN_CENTER_X, SCREEN_CENTER_Y, 226, 224, 240, 300, 0x2354);

    // 2. 分类徽标胶囊 (Y: 28, 字号大方明了)
    char cat_str[32];
    snprintf(cat_str, sizeof(cat_str), "* %s *", place->category);
    for (int i = 0; cat_str[i]; i++) {
        if (cat_str[i] >= 'a' && cat_str[i] <= 'z') cat_str[i] -= 32;
    }
    display.setTextDatum(middle_center);
    display.setTextSize(1);
    display.setTextColor(place->theme_color, COLOR_BG);
    display.drawString(cat_str, SCREEN_CENTER_X, 28);

    // 3. 国家与大区 (Y: 48, 清晰白金文字)
    char reg_str[64];
    snprintf(reg_str, sizeof(reg_str), "%s  |  %s", place->country, place->region);
    display.setTextColor(0xD69A, COLOR_BG);
    display.drawString(reg_str, SCREEN_CENTER_X, 48);

    // 4. 地点大标题 (Y: 74, 字号 2, 纯白高亮加粗，极易阅读！)
    display.setTextSize(2);
    display.setTextColor(0xFFFF, COLOR_BG);
    display.drawString(place->name, SCREEN_CENTER_X, 74);

    // 5. 本土名称与经纬度 (Y: 100, 主题色与纯白配合，字号适中)
    display.setTextSize(1);
    char sub_str[64];
    snprintf(sub_str, sizeof(sub_str), "%s  |  %4.1f%c, %5.1f%c",
             place->local_name,
             fabsf(place->latitude), (place->latitude >= 0 ? 'N' : 'S'),
             fabsf(place->longitude), (place->longitude >= 0 ? 'E' : 'W'));
    display.setTextColor(place->theme_color, COLOR_BG);
    display.drawString(sub_str, SCREEN_CENTER_X, 100);

    // 6. 装饰横线 (Y: 114)
    display.drawLine(SCREEN_CENTER_X - 110, 114, SCREEN_CENTER_X + 110, 114, 0x2354);
}

void PlaceDetailView::drawLeftPanel(M5GFX& display, const PlaceLocation* place) {
    // 预览窗左侧仪表 (X 中心 58, 2 组大字参数，清晰不挤压)
    int lx = 58;

    display.setTextDatum(middle_center);
    display.setTextSize(1);

    // 第 1 组: 真实海拔高度 (Y: 154)
    display.setTextColor(0x4EEB, COLOR_BG); // 翡翠亮青
    char elev_buf[16];
    if (place->elevation_m >= 1000) {
        snprintf(elev_buf, sizeof(elev_buf), "%d,%03dM", place->elevation_m / 1000, abs(place->elevation_m % 1000));
    } else {
        snprintf(elev_buf, sizeof(elev_buf), "%dM", place->elevation_m);
    }
    display.drawString(elev_buf, lx, 154);
    display.setTextColor(0x7BEF, COLOR_BG); // 灰色标签
    display.drawString("ELEVATION", lx, 170);

    // 第 2 组: 真实法定当地时区 (Y: 208)
    display.setTextColor(0xFDC0, COLOR_BG); // 暖金
    char tz_str[16];
    snprintf(tz_str, sizeof(tz_str), "UTC%+d", place->utc_offset);
    display.drawString(tz_str, lx, 208);
    display.setTextColor(0x7BEF, COLOR_BG);
    display.drawString("TIMEZONE", lx, 224);
}

void PlaceDetailView::drawRightPanel(M5GFX& display, const PlaceLocation* place) {
    // 预览窗右侧仪表 (X 中心 408, 2 组大字参数，高对比度)
    int rx = 408;

    display.setTextDatum(middle_center);
    display.setTextSize(1);

    if (place->is_online) {
        // 实时广播状态 (Y: 154)
        display.setTextColor(0x07E0, COLOR_BG); // 纯绿大字
        display.drawString("* LIVE", rx, 154);
        display.setTextColor(0x7BEF, COLOR_BG);
        display.drawString("ONLINE", rx, 170);

        // 官方认证 (Y: 208)
        display.setTextColor(0xFFFF, COLOR_BG); // 纯白高亮
        if (strstr(place->id, "kilauea") || strstr(place->id, "maunakea")) {
            display.drawString("USGS CAM", rx, 208);
            display.setTextColor(0xFD64, COLOR_BG); // 提示野外间歇刷新
            display.drawString("~10M SYNC", rx, 224);
        } else {
            display.drawString("OFFICIAL", rx, 208);
            display.setTextColor(0x7BEF, COLOR_BG);
            display.drawString("STREAM", rx, 224);
        }
    } else {
        // 离线典藏状态 (Y: 154)
        display.setTextColor(0xFDC0, COLOR_BG); // 琥珀金
        display.drawString("OFFLINE", rx, 154);
        display.setTextColor(0x7BEF, COLOR_BG);
        display.drawString("ARCHIVE", rx, 170);

        // 审核流状态 (Y: 208)
        display.setTextColor(0xFFFF, COLOR_BG); // 纯白
        display.drawString("IN REVIEW", rx, 208);
        display.setTextColor(0x7BEF, COLOR_BG);
        display.drawString("AUDIT", rx, 224);
    }
}

void PlaceDetailView::drawMediumPreviewWindow(M5GFX& display, const PlaceLocation* place) {
    // 中等大小预览视窗 (X: 113, Y: 122, W: 240, H: 135, 16:9 标准比例)
    bool has_stream = place->is_online && (place->snapshot_url != nullptr) && (strlen(place->snapshot_url) > 0);

    if (has_stream && snapshot_player.hasValidFrame()) {
        snapshot_player.renderPreview(display, PREVIEW_X, PREVIEW_Y, PREVIEW_W, PREVIEW_H);
    } else {
        display.fillRect(PREVIEW_X, PREVIEW_Y, PREVIEW_W, PREVIEW_H, 0x0821);
    }

    display.drawRect(PREVIEW_X, PREVIEW_Y, PREVIEW_W, PREVIEW_H, has_stream ? 0x4EEB : 0x2354);
    display.drawRect(PREVIEW_X - 1, PREVIEW_Y - 1, PREVIEW_W + 2, PREVIEW_H + 2, 0x1B55);

    // 四角 HUD 瞄准锁框
    int16_t b_len = 8;
    display.drawLine(PREVIEW_X, PREVIEW_Y, PREVIEW_X + b_len, PREVIEW_Y, 0xFFFF);
    display.drawLine(PREVIEW_X, PREVIEW_Y, PREVIEW_X, PREVIEW_Y + b_len, 0xFFFF);

    display.drawLine(PREVIEW_X + PREVIEW_W - 1, PREVIEW_Y, PREVIEW_X + PREVIEW_W - 1 - b_len, PREVIEW_Y, 0xFFFF);
    display.drawLine(PREVIEW_X + PREVIEW_W - 1, PREVIEW_Y, PREVIEW_X + PREVIEW_W - 1, PREVIEW_Y + b_len, 0xFFFF);

    display.drawLine(PREVIEW_X, PREVIEW_Y + PREVIEW_H - 1, PREVIEW_X + b_len, PREVIEW_Y + PREVIEW_H - 1, 0xFFFF);
    display.drawLine(PREVIEW_X, PREVIEW_Y + PREVIEW_H - 1, PREVIEW_X, PREVIEW_Y + PREVIEW_H - 1 - b_len, 0xFFFF);

    display.drawLine(PREVIEW_X + PREVIEW_W - 1, PREVIEW_Y + PREVIEW_H - 1, PREVIEW_X + PREVIEW_W - 1 - b_len, PREVIEW_Y + PREVIEW_H - 1, 0xFFFF);
    display.drawLine(PREVIEW_X + PREVIEW_W - 1, PREVIEW_Y + PREVIEW_H - 1, PREVIEW_X + PREVIEW_W - 1, PREVIEW_Y + PREVIEW_H - 1 - b_len, 0xFFFF);

    int cx = PREVIEW_X + PREVIEW_W / 2;
    int cy = PREVIEW_Y + PREVIEW_H / 2;
    float pulse = 0.5f + 0.5f * sinf(pulse_phase);
    uint16_t btn_col = (pulse > 0.4f ? 0xFFFF : 0x4EEB);

    if (has_stream) {
        // === 在线实时快照模式 ===
        display.setTextDatum(top_left);
        display.setTextSize(1);
        if (snapshot_player.hasValidFrame()) {
            display.setTextColor(0x07E0, 0x0000);
            display.drawString("* LIVE SNAPSHOT", PREVIEW_X + 8, PREVIEW_Y + 7);
            if (snapshot_player.getState() == SNAP_STATE_FETCHING) {
                display.setTextDatum(top_right);
                display.setTextColor(0x07FF, 0x0000);
                display.drawString("[SYNCING]", PREVIEW_X + PREVIEW_W - 8, PREVIEW_Y + 7);
            } else {
                display.setTextDatum(top_right);
                display.setTextColor(network_mgr.isConnected() ? 0x07E0 : 0x7BEF, 0x0000);
                display.drawString("ONLINE", PREVIEW_X + PREVIEW_W - 8, PREVIEW_Y + 7);
            }
        } else if (snapshot_player.getState() == SNAP_STATE_FETCHING) {
            display.setTextColor(0x07FF, 0x0821);
            display.drawString("* LOADING...", PREVIEW_X + 8, PREVIEW_Y + 7);
            display.setTextDatum(top_right);
            display.setTextColor(0x7BEF, 0x0821);
            display.drawString("FETCHING", PREVIEW_X + PREVIEW_W - 8, PREVIEW_Y + 7);
        } else {
            display.setTextColor(0xFDC0, 0x0821);
            display.drawString("* CONNECTING...", PREVIEW_X + 8, PREVIEW_Y + 7);
            display.setTextDatum(top_right);
            display.setTextColor(0x7BEF, 0x0821);
            display.drawString("STANDBY", PREVIEW_X + PREVIEW_W - 8, PREVIEW_Y + 7);
        }

        if (!snapshot_player.hasValidFrame() && snapshot_player.getState() == SNAP_STATE_FETCHING) {
            display.setTextDatum(middle_center);
            display.setTextSize(1);
            display.setTextColor(0x07FF, 0x0821);
            display.drawString("FETCHING LIVE CAMERA...", cx, cy);
        } else {
            display.fillTriangle(cx - 8, cy - 12, cx - 8, cy + 8, cx + 10, cy - 2, btn_col);
            display.setTextDatum(middle_center);
            display.setTextSize(1);
            display.setTextColor(btn_col, 0x0000);
            display.drawString("TAP TO PLAY FULLSCREEN", cx, cy + 24);
        }
    } else {
        // === 官方离线典藏卡模式 (诚实规范，绝不伪造假图) ===
        display.setTextDatum(top_left);
        display.setTextSize(1);
        display.setTextColor(0xFDC0, 0x0821);
        display.drawString("* ARCHIVE DOSSIER", PREVIEW_X + 8, PREVIEW_Y + 7);

        display.setTextDatum(top_right);
        display.setTextColor(0x4EEB, 0x0821);
        display.drawString("VERIFIED", PREVIEW_X + PREVIEW_W - 8, PREVIEW_Y + 7);

        // 科技准心图标
        display.drawCircle(cx, cy - 10, 14, 0x2354);
        display.drawCircle(cx, cy - 10, 4, place->theme_color);
        display.drawLine(cx - 20, cy - 10, cx + 20, cy - 10, 0x2354);
        display.drawLine(cx, cy - 30, cx, cy + 10, 0x2354);

        display.setTextDatum(middle_center);
        display.setTextSize(1);
        display.setTextColor(0xFFFF, 0x0821);
        display.drawString("STREAM AUDIT IN PROGRESS", cx, cy + 16);

        char src_buf[48];
        snprintf(src_buf, sizeof(src_buf), "SRC: %.24s", place->cam_source);
        display.setTextColor(0x7BEF, 0x0821);
        display.drawString(src_buf, cx, cy + 30);
    }

    // 底部摄像机官方名称 (纯白/浅灰)
    display.setTextDatum(bottom_left);
    display.setTextSize(1);
    display.setTextColor(has_stream ? 0xFFFF : 0xD69A, 0x0000);
    display.drawString(place->cam_name, PREVIEW_X + 8, PREVIEW_Y + PREVIEW_H - 7);
}

void PlaceDetailView::drawFooterGuidance(M5GFX& display) {
    display.setTextDatum(middle_center);
    display.setTextSize(1);

    // 底部控制提示 (位于避让地球仪下方的弧形表圈边缘，清晰高对比度)
    display.setTextColor(0xFFFF, COLOR_BG);
    display.drawString("< SWIPE: SWITCH | BTNA: BACK >", SCREEN_CENTER_X, 442);
}

void PlaceDetailView::render(M5GFX& display, const PlaceLocation* place) {
    if (!place) return;

    if (needs_full_redraw || place != last_rendered_place) {
        // 清理上半区，下半区完全交给全宽 466 的避让地球仪画布与自然圆屏边缘
        display.fillRect(0, 0, SCREEN_WIDTH, 260, COLOR_BG);

        drawTopInfo(display, place);
        drawLeftPanel(display, place);
        drawRightPanel(display, place);
        drawMediumPreviewWindow(display, place);
        drawFooterGuidance(display);

        needs_full_redraw = false;
        last_rendered_place = place;
    } else {
        // 动态帧：只微刷新中央视频窗口中的扫描线与呼吸提示
        drawMediumPreviewWindow(display, place);
    }
}
