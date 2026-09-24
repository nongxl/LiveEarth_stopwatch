#include "place_detail_view.h"
#include "config.h"
#include <cmath>

#pragma GCC optimize ("O3,fast-math")

PlaceDetailView::PlaceDetailView()
    : anim_start_ms(0), pulse_phase(0.0f), needs_full_redraw(true), last_rendered_place(nullptr) {
}

void PlaceDetailView::init() {
    anim_start_ms = millis();
    pulse_phase = 0.0f;
    needs_full_redraw = true;
    last_rendered_place = nullptr;
}

void PlaceDetailView::update(uint32_t now_ms) {
    if (anim_start_ms == 0) anim_start_ms = now_ms;
    float dt = (now_ms - anim_start_ms) * 0.001f;
    anim_start_ms = now_ms;
    if (dt > 0.1f) dt = 0.1f;

    // 呼吸灯动画周期约 1.5 秒
    pulse_phase += dt * 4.0f;
    if (pulse_phase >= 6.2831853f) {
        pulse_phase -= 6.2831853f;
    }
}

void PlaceDetailView::drawHeader(M5GFX& display, const PlaceLocation* place) {
    // 1. 顶部表盘微光弧
    display.drawArc(SCREEN_CENTER_X, SCREEN_CENTER_Y, 226, 224, 230, 310, 0x1B55);

    // 2. 分类微标药丸 Badge (Y: 45)
    char cat_str[32];
    snprintf(cat_str, sizeof(cat_str), "[ %s ]", place->category);
    // 转大写
    for (int i = 0; cat_str[i]; i++) {
        if (cat_str[i] >= 'a' && cat_str[i] <= 'z') cat_str[i] -= 32;
    }

    display.setTextColor(place->theme_color, COLOR_BG);
    display.setTextDatum(middle_center);
    display.setTextSize(1);
    display.drawString(cat_str, SCREEN_CENTER_X, 48);

    // 3. 国家与大区 (Y: 66)
    char region_str[64];
    snprintf(region_str, sizeof(region_str), "%s  |  %s", place->country, place->region);
    display.setTextColor(0xA554, COLOR_BG);
    display.drawString(region_str, SCREEN_CENTER_X, 68);
}

void PlaceDetailView::drawCoreInfo(M5GFX& display, const PlaceLocation* place) {
    // 1. 地名英文大标题 (Y: 96)
    display.setTextColor(0xFFFF, COLOR_BG);
    display.setTextDatum(middle_center);
    display.setTextSize(2);
    display.drawString(place->name, SCREEN_CENTER_X, 98);

    // 2. 当地母语名称 (Y: 122)
    display.setTextSize(1);
    display.setTextColor(place->theme_color, COLOR_BG);
    display.drawString(place->local_name, SCREEN_CENTER_X, 124);

    // 3. 经纬度微标 (Y: 142)
    char coord_str[64];
    snprintf(coord_str, sizeof(coord_str), "GEO: %4.2f%c, %5.2f%c",
             fabsf(place->latitude), (place->latitude >= 0 ? 'N' : 'S'),
             fabsf(place->longitude), (place->longitude >= 0 ? 'E' : 'W'));
    display.setTextColor(0x7BEF, COLOR_BG);
    display.drawString(coord_str, SCREEN_CENTER_X, 144);

    // 4. 精致横向装饰分割线与中央菱形 (Y: 158)
    display.drawLine(110, 158, 220, 158, 0x2354);
    display.drawLine(246, 158, 356, 158, 0x2354);
    display.fillCircle(SCREEN_CENTER_X, 158, 2, place->theme_color);
}

void PlaceDetailView::drawDescription(M5GFX& display, const PlaceLocation* place) {
    display.setTextDatum(middle_center);
    display.setTextSize(1);
    display.setTextColor(0xDEFB, COLOR_BG);

    // 自动折行简短介绍 (针对圆形屏幕优化排版，Y: 178, 196)
    const char* desc = place->description;
    size_t len = strlen(desc);

    if (len <= 48) {
        display.drawString(desc, SCREEN_CENTER_X, 186);
    } else {
        // 寻找中间空格分割成两行
        int split_idx = 42;
        while (split_idx > 20 && desc[split_idx] != ' ') {
            split_idx--;
        }
        char line1[64] = {0};
        char line2[64] = {0};
        strncpy(line1, desc, split_idx);
        strncpy(line2, desc + split_idx + 1, sizeof(line2) - 1);

        display.drawString(line1, SCREEN_CENTER_X, 178);
        display.drawString(line2, SCREEN_CENTER_X, 194);
    }

    // 亮点标签 (Y: 218)
    char hl_buf[96];
    snprintf(hl_buf, sizeof(hl_buf), "* %s", place->highlight);
    display.setTextColor(0xFDC0, COLOR_BG); // 暖金微光
    display.drawString(hl_buf, SCREEN_CENTER_X, 218);
}

void PlaceDetailView::drawCameraCard(M5GFX& display, const PlaceLocation* place) {
    int card_x = 73;
    int card_y = 242;
    int card_w = 320;
    int card_h = 118;
    int r = 10;

    // 卡片背景与科技边框
    display.fillRoundRect(card_x, card_y, card_w, card_h, r, 0x0862);
    display.drawRoundRect(card_x, card_y, card_w, card_h, r, 0x2354);

    // 动态呼吸状态指示灯 (🟢 LIVE ONLINE)
    float pulse = 0.5f + 0.5f * sinf(pulse_phase);
    int light_x = card_x + 20;
    int light_y = card_y + 20;

    display.fillCircle(light_x, light_y, 4, 0x07E0); // 实体翡翠绿
    int pulse_r = 4 + (int)(3.0f * pulse);
    display.drawCircle(light_x, light_y, pulse_r, 0x47E0);

    display.setTextDatum(middle_left);
    display.setTextSize(1);
    display.setTextColor(0x07E0, 0x0862);
    display.drawString("LIVE WEBCAM ONLINE", light_x + 14, light_y);

    // 摄像头官方全称
    display.setTextColor(0xFFFF, 0x0862);
    display.drawString(place->cam_name, card_x + 20, card_y + 46);

    // 来源与审核状态
    char src_buf[64];
    snprintf(src_buf, sizeof(src_buf), "Source: %s", place->cam_source);
    display.setTextColor(0x9CF3, 0x0862);
    display.drawString(src_buf, card_x + 20, card_y + 70);

    // 合法公开背书微标
    display.setTextColor(0xFDC0, 0x0862);
    display.drawString("[ APPROVED OFFICIAL FEED ]", card_x + 20, card_y + 94);
}

void PlaceDetailView::drawFooterControls(M5GFX& display, const PlaceLocation* place) {
    display.setTextDatum(middle_center);
    display.setTextSize(1);

    // 滑动提示 (Y: 388)
    display.setTextColor(0x7BEF, COLOR_BG);
    display.drawString("< SWIPE LEFT / RIGHT TO EXPLORE >", SCREEN_CENTER_X, 388);

    // 物理按键与退出提示 (Y: 410)
    display.setTextColor(COLOR_ACCENT_CYAN, COLOR_BG);
    display.drawString("[BtnA] BACK TO GLOBE", SCREEN_CENTER_X, 410);

    // 底部表盘微光弧
    display.drawArc(SCREEN_CENTER_X, SCREEN_CENTER_Y, 226, 224, 50, 130, 0x1B55);
}

void PlaceDetailView::render(M5GFX& display, const PlaceLocation* place) {
    if (!place) return;

    // 当地点改变或首次进入时，全量重绘底色和所有静态文字
    if (needs_full_redraw || place != last_rendered_place) {
        display.fillScreen(COLOR_BG);

        drawHeader(display, place);
        drawCoreInfo(display, place);
        drawDescription(display, place);
        drawCameraCard(display, place);
        drawFooterControls(display, place);

        needs_full_redraw = false;
        last_rendered_place = place;
    } else {
        // 后续帧只微刷新摄像头的动态呼吸灯，零重绘闪烁，极高刷新率！
        int card_x = 73;
        int card_y = 242;
        int light_x = card_x + 20;
        int light_y = card_y + 20;

        // 局部擦除呼吸灯外圈
        display.fillCircle(light_x, light_y, 9, 0x0862);
        display.fillCircle(light_x, light_y, 4, 0x07E0);

        float pulse = 0.5f + 0.5f * sinf(pulse_phase);
        int pulse_r = 4 + (int)(3.0f * pulse);
        display.drawCircle(light_x, light_y, pulse_r, 0x47E0);
    }
}
