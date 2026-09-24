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

    // 第 1 组: 海拔或高度 (Y: 154)
    display.setTextColor(0x4EEB, COLOR_BG); // 翡翠亮青
    display.drawString((strcmp(place->category, "mountain") == 0 ? "3,776M" : "COASTAL"), lx, 154);
    display.setTextColor(0x7BEF, COLOR_BG); // 灰色标签
    display.drawString("ELEVATION", lx, 170);

    // 第 2 组: 时区信息 (Y: 208)
    display.setTextColor(0xFDC0, COLOR_BG); // 暖金
    int tz_offset = (int)(place->longitude / 15.0f);
    char tz_str[16];
    snprintf(tz_str, sizeof(tz_str), "UTC%+d", tz_offset);
    display.drawString(tz_str, lx, 208);
    display.setTextColor(0x7BEF, COLOR_BG);
    display.drawString("TIMEZONE", lx, 224);
}

void PlaceDetailView::drawRightPanel(M5GFX& display, const PlaceLocation* place) {
    // 预览窗右侧仪表 (X 中心 408, 2 组大字参数，高对比度)
    int rx = 408;

    display.setTextDatum(middle_center);
    display.setTextSize(1);

    // 第 1 组: 实时广播状态 (Y: 154)
    display.setTextColor(0x07E0, COLOR_BG); // 纯绿大字
    display.drawString("* LIVE", rx, 154);
    display.setTextColor(0x7BEF, COLOR_BG);
    display.drawString("ONLINE", rx, 170);

    // 第 2 组: 官方认证 (Y: 208)
    display.setTextColor(0xFFFF, COLOR_BG); // 纯白高亮
    display.drawString("OFFICIAL", rx, 208);
    display.setTextColor(0x7BEF, COLOR_BG);
    display.drawString("VERIFIED", rx, 224);
}

void PlaceDetailView::drawMediumPreviewWindow(M5GFX& display, const PlaceLocation* place) {
    // 中等大小预览视窗 (X: 113, Y: 122, W: 240, H: 135, 16:9 标准比例)
    display.fillRect(PREVIEW_X, PREVIEW_Y, PREVIEW_W, PREVIEW_H, 0x0842);
    display.drawRect(PREVIEW_X, PREVIEW_Y, PREVIEW_W, PREVIEW_H, 0x4EEB);
    display.drawRect(PREVIEW_X - 1, PREVIEW_Y - 1, PREVIEW_W + 2, PREVIEW_H + 2, 0x2354);

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

    // 动态扫描线 (Scanline)
    int sy = PREVIEW_Y + (int)scanline_y;
    display.drawLine(PREVIEW_X + 2, sy, PREVIEW_X + PREVIEW_W - 3, sy, 0x2475);

    // 左上角: 实时流角标
    display.setTextDatum(top_left);
    display.setTextSize(1);
    display.setTextColor(0x07E0, 0x0842);
    display.drawString("* LIVE 1080P", PREVIEW_X + 8, PREVIEW_Y + 7);

    // 右上角: 来源许可
    display.setTextDatum(top_right);
    display.setTextColor(0xFDC0, 0x0842);
    display.drawString("APPROVED", PREVIEW_X + PREVIEW_W - 8, PREVIEW_Y + 7);

    // 中央大号发光播放按钮与高对比度文字 (字号大、极易阅读与点击操作！)
    int cx = PREVIEW_X + PREVIEW_W / 2;
    int cy = PREVIEW_Y + PREVIEW_H / 2;

    float pulse = 0.5f + 0.5f * sinf(pulse_phase);
    uint16_t btn_col = (pulse > 0.4f ? 0xFFFF : 0x4EEB);

    // 绘制显眼的大播放三角形
    display.fillTriangle(cx - 8, cy - 12, cx - 8, cy + 8, cx + 10, cy - 2, btn_col);

    display.setTextDatum(middle_center);
    display.setTextSize(1);
    display.setTextColor(btn_col, 0x0842);
    display.drawString("TAP TO PLAY FULLSCREEN", cx, cy + 24);

    // 底部摄像机官方名称 (纯白粗体)
    display.setTextDatum(bottom_left);
    display.setTextColor(0xFFFF, 0x0842);
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
