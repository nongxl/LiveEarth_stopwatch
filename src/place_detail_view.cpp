#include "place_detail_view.h"
#include "config.h"
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

    // 呼吸灯与扫描线动画
    pulse_phase += dt * 3.5f;
    if (pulse_phase >= 6.2831853f) pulse_phase -= 6.2831853f;

    scanline_y += dt * 50.0f;
    if (scanline_y >= PREVIEW_H) scanline_y -= PREVIEW_H;
}

void PlaceDetailView::drawTopInfo(M5GFX& display, const PlaceLocation* place) {
    // 1. 顶部表盘弧线 (R: 226)
    display.drawArc(SCREEN_CENTER_X, SCREEN_CENTER_Y, 226, 224, 240, 300, 0x1B55);

    // 2. 分类徽标药丸 (Y: 34)
    char cat_str[32];
    snprintf(cat_str, sizeof(cat_str), "[ %s ]", place->category);
    for (int i = 0; cat_str[i]; i++) {
        if (cat_str[i] >= 'a' && cat_str[i] <= 'z') cat_str[i] -= 32;
    }
    display.setTextDatum(middle_center);
    display.setTextSize(1);
    display.setTextColor(place->theme_color, COLOR_BG);
    display.drawString(cat_str, SCREEN_CENTER_X, 34);

    // 3. 国家与大区 (Y: 54)
    char reg_str[64];
    snprintf(reg_str, sizeof(reg_str), "%s  |  %s", place->country, place->region);
    display.setTextColor(0xA554, COLOR_BG);
    display.drawString(reg_str, SCREEN_CENTER_X, 54);

    // 4. 地点大标题 (Y: 76, 字号 2, 纯白高亮)
    display.setTextSize(2);
    display.setTextColor(0xFFFF, COLOR_BG);
    display.drawString(place->name, SCREEN_CENTER_X, 76);

    // 5. 本土语言名称与特色亮点 (Y: 100)
    display.setTextSize(1);
    char sub_str[96];
    snprintf(sub_str, sizeof(sub_str), "%s  *  %s", place->local_name, place->highlight);
    display.setTextColor(place->theme_color, COLOR_BG);
    display.drawString(sub_str, SCREEN_CENTER_X, 102);

    // 6. 微细分割线 (Y: 118)
    display.drawLine(SCREEN_CENTER_X - 120, 118, SCREEN_CENTER_X + 120, 118, 0x2354);
    display.fillCircle(SCREEN_CENTER_X, 118, 2, place->theme_color);
}

void PlaceDetailView::drawLeftPanel(M5GFX& display, const PlaceLocation* place) {
    // 预览窗左侧垂直参数 (X 中心约 68, Y: 140 ~ 250)
    int lx = 64;

    display.setTextDatum(middle_center);
    display.setTextSize(1);

    // 1. 纬度
    display.setTextColor(0x7BEF, COLOR_BG);
    display.drawString("LAT", lx, 142);
    char lat_str[16];
    snprintf(lat_str, sizeof(lat_str), "%4.2f%c", fabsf(place->latitude), (place->latitude >= 0 ? 'N' : 'S'));
    display.setTextColor(0xFFFF, COLOR_BG);
    display.drawString(lat_str, lx, 156);

    // 2. 经度
    display.setTextColor(0x7BEF, COLOR_BG);
    display.drawString("LON", lx, 174);
    char lon_str[16];
    snprintf(lon_str, sizeof(lon_str), "%5.2f%c", fabsf(place->longitude), (place->longitude >= 0 ? 'E' : 'W'));
    display.setTextColor(0xFFFF, COLOR_BG);
    display.drawString(lon_str, lx, 188);

    // 3. 海拔/高度
    display.setTextColor(0x7BEF, COLOR_BG);
    display.drawString("ELEV", lx, 206);
    display.setTextColor(0x4EEB, COLOR_BG);
    display.drawString((strcmp(place->category, "mountain") == 0 ? "3,776M" : "SEA LVL"), lx, 220);

    // 4. 时区
    display.setTextColor(0x7BEF, COLOR_BG);
    display.drawString("ZONE", lx, 238);
    display.setTextColor(0xFDC0, COLOR_BG);
    int tz_offset = (int)(place->longitude / 15.0f);
    char tz_str[16];
    snprintf(tz_str, sizeof(tz_str), "UTC%+d", tz_offset);
    display.drawString(tz_str, lx, 252);
}

void PlaceDetailView::drawRightPanel(M5GFX& display, const PlaceLocation* place) {
    // 预览窗右侧垂直参数 (X 中心约 400, Y: 140 ~ 250)
    int rx = 402;

    display.setTextDatum(middle_center);
    display.setTextSize(1);

    // 1. 状态 (🟢 ONLINE)
    display.setTextColor(0x7BEF, COLOR_BG);
    display.drawString("FEED", rx, 142);
    display.setTextColor(0x07E0, COLOR_BG);
    display.drawString("* ONLINE", rx, 156);

    // 2. 来源级别
    display.setTextColor(0x7BEF, COLOR_BG);
    display.drawString("SOURCE", rx, 174);
    display.setTextColor(0xFFFF, COLOR_BG);
    display.drawString("OFFICIAL", rx, 188);

    // 3. 授权合规
    display.setTextColor(0x7BEF, COLOR_BG);
    display.drawString("LICENSE", rx, 206);
    display.setTextColor(0xFDC0, COLOR_BG);
    display.drawString("APPROVED", rx, 220);

    // 4. 传输协议
    display.setTextColor(0x7BEF, COLOR_BG);
    display.drawString("TYPE", rx, 238);
    display.setTextColor(0x4EEB, COLOR_BG);
    display.drawString("SNAPSHOT", rx, 252);
}

void PlaceDetailView::drawMediumPreviewWindow(M5GFX& display, const PlaceLocation* place) {
    // 中等大小预览视窗 (X: 121, Y: 135, W: 224, H: 126, 16:9)
    display.fillRect(PREVIEW_X, PREVIEW_Y, PREVIEW_W, PREVIEW_H, 0x0842);
    display.drawRect(PREVIEW_X, PREVIEW_Y, PREVIEW_W, PREVIEW_H, 0x3457);

    // 四角 HUD 瞄准括号
    int16_t b_len = 6;
    display.drawLine(PREVIEW_X, PREVIEW_Y, PREVIEW_X + b_len, PREVIEW_Y, 0xFFFF);
    display.drawLine(PREVIEW_X, PREVIEW_Y, PREVIEW_X, PREVIEW_Y + b_len, 0xFFFF);

    display.drawLine(PREVIEW_X + PREVIEW_W - 1, PREVIEW_Y, PREVIEW_X + PREVIEW_W - 1 - b_len, PREVIEW_Y, 0xFFFF);
    display.drawLine(PREVIEW_X + PREVIEW_W - 1, PREVIEW_Y, PREVIEW_X + PREVIEW_W - 1, PREVIEW_Y + b_len, 0xFFFF);

    display.drawLine(PREVIEW_X, PREVIEW_Y + PREVIEW_H - 1, PREVIEW_X + b_len, PREVIEW_Y + PREVIEW_H - 1, 0xFFFF);
    display.drawLine(PREVIEW_X, PREVIEW_Y + PREVIEW_H - 1, PREVIEW_X, PREVIEW_Y + PREVIEW_H - 1 - b_len, 0xFFFF);

    display.drawLine(PREVIEW_X + PREVIEW_W - 1, PREVIEW_Y + PREVIEW_H - 1, PREVIEW_X + PREVIEW_W - 1 - b_len, PREVIEW_Y + PREVIEW_H - 1, 0xFFFF);
    display.drawLine(PREVIEW_X + PREVIEW_W - 1, PREVIEW_Y + PREVIEW_H - 1, PREVIEW_X + PREVIEW_W - 1, PREVIEW_Y + PREVIEW_H - 1 - b_len, 0xFFFF);

    // 预览窗顶部信息小条
    display.setTextDatum(top_left);
    display.setTextSize(1);
    display.setTextColor(0x7BEF, 0x0842);
    display.drawString("LIVE CAM PREVIEW", PREVIEW_X + 8, PREVIEW_Y + 6);

    // 动态扫描线 (Scanline)
    int sy = PREVIEW_Y + (int)scanline_y;
    display.drawLine(PREVIEW_X + 2, sy, PREVIEW_X + PREVIEW_W - 3, sy, 0x2475);

    // 中央播放微标与点击放大提示
    int cx = PREVIEW_X + PREVIEW_W / 2;
    int cy = PREVIEW_Y + PREVIEW_H / 2;

    float pulse = 0.5f + 0.5f * sinf(pulse_phase);
    uint16_t btn_col = (pulse > 0.5f ? 0xFFFF : 0x4EEB);

    // 播放三角形
    display.fillTriangle(cx - 6, cy - 10, cx - 6, cy + 6, cx + 8, cy - 2, btn_col);

    display.setTextDatum(middle_center);
    display.setTextColor(btn_col, 0x0842);
    display.drawString("TAP TO ENLARGE", cx, cy + 22);

    // 底部摄像机简名
    display.setTextDatum(bottom_left);
    display.setTextColor(0xA554, 0x0842);
    display.drawString(place->cam_name, PREVIEW_X + 8, PREVIEW_Y + PREVIEW_H - 6);
}

void PlaceDetailView::drawFooterGuidance(M5GFX& display) {
    display.setTextDatum(middle_center);
    display.setTextSize(1);

    // 底部控制提示 (位于避让地球仪下方的弧形表圈边缘)
    display.setTextColor(0x7BEF, COLOR_BG);
    display.drawString("< SWIPE TO SWITCH | BTNA BACK >", SCREEN_CENTER_X, 442);
}

void PlaceDetailView::render(M5GFX& display, const PlaceLocation* place) {
    if (!place) return;

    if (needs_full_redraw || place != last_rendered_place) {
        // 清理上半区和左右侧区域，保留下半区给避让的地球仪
        display.fillRect(0, 0, SCREEN_WIDTH, 268, COLOR_BG);
        display.fillRect(0, 268, 73, 198, COLOR_BG);
        display.fillRect(393, 268, 73, 198, COLOR_BG);

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
