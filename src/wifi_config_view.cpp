#include "wifi_config_view.h"
#include "config.h"
#include <algorithm>

WifiConfigView wifi_config_view;

static const int16_t SCREEN_CENTER = SCREEN_WIDTH / 2;

WifiConfigView::WifiConfigView()
    : current_state(WIFI_CFG_INACTIVE),
      kb_mode(KB_MODE_LOWER),
      pass_cursor(0),
      show_password_plain(true),
      last_char_input_ms(0),
      key_count(0),
      pressed_key_idx(-1),
      key_press_feedback_ms(0),
      press_start_ms(0),
      is_long_press_triggered(false),
      list_scroll_y(0.0f),
      target_scroll_y(0.0f),
      is_dragging_list(false),
      touch_start_x(0),
      touch_start_y(0),
      touch_last_y(0),
      touch_start_ms(0),
      state_enter_ms(0),
      last_scan_req_ms(0)
{
    selected_ssid[0] = '\0';
    input_password[0] = '\0';
}

void WifiConfigView::init() {
    buildTrapezoidKeyboardLayout();
}

void WifiConfigView::buildTrapezoidKeyboardLayout() {
    key_count = 0;

    // Row 1: 最宽, 10 键 (q w e r t y u i o p) -> 长按输入 1 2 3 4 5 6 7 8 9 0
    int16_t r1_y = 210;
    int16_t r1_w = 38;
    int16_t r1_h = 46;
    int16_t r1_gap = 4;
    int16_t r1_total = 10 * r1_w + 9 * r1_gap; // 416
    int16_t r1_start_x = (SCREEN_WIDTH - r1_total) / 2;

    const char r1_norm[] = "qwertyuiop";
    const char r1_shft[] = "QWERTYUIOP";
    const char r1_syms[] = "1234567890";
    const char r1_long[] = "1234567890";

    for (int i = 0; i < 10; i++) {
        KeyButton& k = keys[key_count++];
        k.x = r1_start_x + i * (r1_w + r1_gap);
        k.y = r1_y;
        k.w = r1_w;
        k.h = r1_h;
        k.normal_char = r1_norm[i];
        k.shift_char  = r1_shft[i];
        k.symbol_char = r1_syms[i];
        k.long_press_char = r1_long[i];
        k.label = nullptr;
        k.action_code = 0;
    }

    // Row 2: 次宽, 9 键 (a s d f g h j k l) -> 长按输入高频 Wi-Fi 符号 @ _ . - ! # $ * ?
    int16_t r2_y = 260;
    int16_t r2_w = 40;
    int16_t r2_h = 46;
    int16_t r2_gap = 4;
    int16_t r2_total = 9 * r2_w + 8 * r2_gap; // 392
    int16_t r2_start_x = (SCREEN_WIDTH - r2_total) / 2;

    const char r2_norm[] = "asdfghjkl";
    const char r2_shft[] = "ASDFGHJKL";
    const char r2_syms[] = "@#$&*-+()";
    const char r2_long[] = "@_.-!#$*?";

    for (int i = 0; i < 9; i++) {
        KeyButton& k = keys[key_count++];
        k.x = r2_start_x + i * (r2_w + r2_gap);
        k.y = r2_y;
        k.w = r2_w;
        k.h = r2_h;
        k.normal_char = r2_norm[i];
        k.shift_char  = r2_shft[i];
        k.symbol_char = r2_syms[i];
        k.long_press_char = r2_long[i];
        k.label = nullptr;
        k.action_code = 0;
    }

    // Row 3: 较窄 (CAPS + 7 字符 + DEL) -> 长按输入 = / : ; % & ~
    int16_t r3_y = 310;
    int16_t r3_h = 46;
    int16_t r3_fn_w = 50;
    int16_t r3_char_w = 35;
    int16_t r3_gap = 4;
    int16_t r3_total = r3_fn_w + r3_gap + (7 * r3_char_w + 6 * r3_gap) + r3_gap + r3_fn_w; // 377
    int16_t r3_start_x = (SCREEN_WIDTH - r3_total) / 2;

    // Shift Key
    {
        KeyButton& k = keys[key_count++];
        k.x = r3_start_x;
        k.y = r3_y;
        k.w = r3_fn_w;
        k.h = r3_h;
        k.normal_char = 0;
        k.shift_char = 0;
        k.symbol_char = 0;
        k.long_press_char = 0;
        k.label = "CAPS";
        k.action_code = 1; // Shift
    }

    // 7 Characters
    const char r3_norm[] = "zxcvbnm";
    const char r3_shft[] = "ZXCVBNM";
    const char r3_syms[] = "=/;:!%~";
    const char r3_long[] = "=/;:!%~";
    int16_t cur_x = r3_start_x + r3_fn_w + r3_gap;
    for (int i = 0; i < 7; i++) {
        KeyButton& k = keys[key_count++];
        k.x = cur_x + i * (r3_char_w + r3_gap);
        k.y = r3_y;
        k.w = r3_char_w;
        k.h = r3_h;
        k.normal_char = r3_norm[i];
        k.shift_char  = r3_shft[i];
        k.symbol_char = r3_syms[i];
        k.long_press_char = r3_long[i];
        k.label = nullptr;
        k.action_code = 0;
    }

    // Del Key
    {
        KeyButton& k = keys[key_count++];
        k.x = cur_x + 7 * (r3_char_w + r3_gap);
        k.y = r3_y;
        k.w = r3_fn_w;
        k.h = r3_h;
        k.normal_char = 0;
        k.shift_char = 0;
        k.symbol_char = 0;
        k.long_press_char = 0;
        k.label = "DEL";
        k.action_code = 2; // Del
    }

    // Row 4: 底部窄行, 3 个大功能键 (?123/ABC, SPACE, JOIN)
    int16_t r4_y = 362;
    int16_t r4_h = 48;
    int16_t r4_w_mode = 76;
    int16_t r4_w_space = 124;
    int16_t r4_w_conn = 104;
    int16_t r4_gap = 6;
    int16_t r4_total = r4_w_mode + r4_gap + r4_w_space + r4_gap + r4_w_conn; // 316
    int16_t r4_start_x = (SCREEN_WIDTH - r4_total) / 2;

    // Mode Key
    {
        KeyButton& k = keys[key_count++];
        k.x = r4_start_x;
        k.y = r4_y;
        k.w = r4_w_mode;
        k.h = r4_h;
        k.normal_char = 0;
        k.shift_char = 0;
        k.symbol_char = 0;
        k.long_press_char = 0;
        k.label = "?123";
        k.action_code = 3; // Toggle Symbol
    }

    // Space Key
    {
        KeyButton& k = keys[key_count++];
        k.x = r4_start_x + r4_w_mode + r4_gap;
        k.y = r4_y;
        k.w = r4_w_space;
        k.h = r4_h;
        k.normal_char = ' ';
        k.shift_char = ' ';
        k.symbol_char = ' ';
        k.long_press_char = 0;
        k.label = "SPACE";
        k.action_code = 4; // Space
    }

    // Connect Key
    {
        KeyButton& k = keys[key_count++];
        k.x = r4_start_x + r4_w_mode + r4_gap + r4_w_space + r4_gap;
        k.y = r4_y;
        k.w = r4_w_conn;
        k.h = r4_h;
        k.normal_char = 0;
        k.shift_char = 0;
        k.symbol_char = 0;
        k.long_press_char = 0;
        k.label = "JOIN";
        k.action_code = 5; // Connect
    }
    // 已彻底去除 Row 5，键盘高度减少一行，整体重心下移贴合圆屏底部
}


void WifiConfigView::switchState(WifiConfigState new_state) {
    current_state = new_state;
    state_enter_ms = millis();
    pressed_key_idx = -1;

    if (new_state == WIFI_CFG_SSID_LIST) {
        list_scroll_y = 0.0f;
        target_scroll_y = 0.0f;
        is_dragging_list = false;
        triggerScan();
    }
}

void WifiConfigView::open(bool from_boot) {
    if (from_boot) {
        if (network_mgr.hasSavedConfig()) {
            switchState(WIFI_CFG_BOOT_CHECK);
        } else {
            switchState(WIFI_CFG_SSID_LIST);
        }
    } else {
        switchState(WIFI_CFG_SSID_LIST);
    }
}

void WifiConfigView::close() {
    current_state = WIFI_CFG_INACTIVE;
}

void WifiConfigView::triggerScan() {
    network_mgr.startScan(true);
    last_scan_req_ms = millis();
}

void WifiConfigView::appendChar(char c) {
    if (pass_cursor < sizeof(input_password) - 1) {
        input_password[pass_cursor++] = c;
        input_password[pass_cursor] = '\0';
        last_char_input_ms = millis();
    }
}

void WifiConfigView::deleteChar() {
    if (pass_cursor > 0) {
        pass_cursor--;
        input_password[pass_cursor] = '\0';
        last_char_input_ms = millis();
    }
}

void WifiConfigView::update(uint32_t now_ms) {
    if (current_state == WIFI_CFG_INACTIVE) return;

    // 开机检测模式：若已保存的 Wi-Fi 连上了，立即切入成功
    if (current_state == WIFI_CFG_BOOT_CHECK) {
        if (network_mgr.isConnected()) {
            switchState(WIFI_CFG_SUCCESS);
        } else if (network_mgr.isFailed() || (now_ms - state_enter_ms > WIFI_CONNECT_TIMEOUT_MS)) {
            Serial.println("[WifiConfig] Boot auto-connect failed/timed out. Presenting SSID list.");
            switchState(WIFI_CFG_SSID_LIST);
        }
    } 
    // 用户发起连接阶段
    else if (current_state == WIFI_CFG_CONNECTING) {
        if (network_mgr.isConnected()) {
            // 保存到 NVS
            network_mgr.saveConfig(selected_ssid, input_password);
            switchState(WIFI_CFG_SUCCESS);
        } else if (network_mgr.isFailed() || (now_ms - state_enter_ms > WIFI_CONNECT_TIMEOUT_MS)) {
            network_mgr.resetFailState();
            switchState(WIFI_CFG_FAILED);
        }
    }
    // 成功态停留 1.5 秒后自动关闭
    else if (current_state == WIFI_CFG_SUCCESS) {
        if (now_ms - state_enter_ms > 1500) {
            close();
        }
    }

    // SSID 列表平滑滚动弹性更新
    if (current_state == WIFI_CFG_SSID_LIST && !is_dragging_list) {
        list_scroll_y += (target_scroll_y - list_scroll_y) * 0.25f;
    }
}

// -------------------------------------------------------------
// UI 渲染实现 (AMOLED 466x466 圆屏全适配)
// -------------------------------------------------------------
void WifiConfigView::render(M5GFX& display) {
    if (current_state == WIFI_CFG_INACTIVE) return;

    display.startWrite();

    switch (current_state) {
        case WIFI_CFG_BOOT_CHECK:
            renderBootCheck(display);
            break;
        case WIFI_CFG_SSID_LIST:
            renderSSIDList(display);
            break;
        case WIFI_CFG_KEYBOARD:
            renderTrapezoidKeyboard(display);
            break;
        case WIFI_CFG_CONNECTING:
            renderConnecting(display);
            break;
        case WIFI_CFG_SUCCESS:
            renderSuccess(display);
            break;
        case WIFI_CFG_FAILED:
            renderFailed(display);
            break;
        default:
            break;
    }

    display.endWrite();
}

void WifiConfigView::renderBootCheck(M5GFX& display) {
    display.fillScreen(COLOR_BG);

    // 居中科技波纹
    uint32_t now = millis();
    float pulse = 0.5f + 0.5f * sinf(now * 0.005f);
    int r = 80 + (int)(pulse * 30);
    display.drawCircle(SCREEN_CENTER, SCREEN_CENTER, r, 0x1B55);
    display.drawCircle(SCREEN_CENTER, SCREEN_CENTER, 70, COLOR_ACCENT_CYAN);
    display.fillCircle(SCREEN_CENTER, SCREEN_CENTER, 8, COLOR_ACCENT_CYAN);

    display.setTextDatum(middle_center);
    display.setTextSize(1);
    display.setTextColor(COLOR_ACCENT_CYAN, COLOR_BG);
    display.drawString("* LIVE-EARTH SYSTEM *", SCREEN_CENTER, 140);

    display.setTextColor(0xFFFF, COLOR_BG);
    display.drawString("CONNECTING TO SAVED WI-FI...", SCREEN_CENTER, 280);

    char buf[64];
    snprintf(buf, sizeof(buf), "SSID: %s", network_mgr.getSSID());
    display.setTextColor(0xA554, COLOR_BG);
    display.drawString(buf, SCREEN_CENTER, 305);

    // 底部跳过按钮
    display.fillRoundRect(SCREEN_CENTER - 60, 370, 120, 32, 16, 0x10A2);
    display.drawRoundRect(SCREEN_CENTER - 60, 370, 120, 32, 16, 0x39E7);
    display.setTextColor(0xBDD7, 0x10A2);
    display.drawString("SKIP / OFFLINE", SCREEN_CENTER, 386);
}

void WifiConfigView::renderSSIDList(M5GFX& display) {
    display.fillScreen(COLOR_BG);

    // 1. 顶部 HUD (Y: 28 ~ 74)
    display.setTextDatum(middle_center);
    display.setTextSize(1);
    display.setTextColor(COLOR_ACCENT_CYAN, COLOR_BG);
    display.drawString("* SELECT WI-FI NETWORK *", SCREEN_CENTER, 34);

    // 刷新按钮 (左上)
    display.fillRoundRect(64, 48, 70, 24, 12, 0x0842);
    display.drawRoundRect(64, 48, 70, 24, 12, 0x2354);
    display.setTextColor(0x7BEF, 0x0842);
    display.drawString("↻ SCAN", 99, 60);

    // 跳过离线按钮 (右上)
    display.fillRoundRect(SCREEN_WIDTH - 134, 48, 70, 24, 12, 0x0842);
    display.drawRoundRect(SCREEN_WIDTH - 134, 48, 70, 24, 12, 0x2354);
    display.drawString("OFFLINE", SCREEN_WIDTH - 99, 60);

    display.drawFastHLine(40, 76, SCREEN_WIDTH - 80, 0x2354);

    // 2. 列表内容 (Y: 82 ~ 400)
    const auto& list = network_mgr.getScannedList();
    int count = (int)list.size();

    if (count == 0) {
        display.setTextDatum(middle_center);
        display.setTextColor(0x7BEF, COLOR_BG);
        display.drawString("SCANNING SURROUNDING WI-FI...", SCREEN_CENTER, 220);
        display.drawCircle(SCREEN_CENTER, 255, 12, 0x2354);
        return;
    }

    const int ITEM_H = 50;
    const int LIST_TOP = 82;
    const int LIST_BOTTOM = 410;
    const int card_w = 326;
    const int card_x = (SCREEN_WIDTH - card_w) / 2;

    for (int i = 0; i < count; i++) {
        int item_y = LIST_TOP + (int)list_scroll_y + i * (ITEM_H + 4);
        if (item_y + ITEM_H < LIST_TOP || item_y > LIST_BOTTOM) continue;

        const auto& item = list[i];

        // 1. 卡片底板 (326x50, 紧凑黄金比例)
        display.fillRoundRect(card_x, item_y, card_w, ITEM_H, 10, 0x0821);
        display.drawRoundRect(card_x, item_y, card_w, ITEM_H, 10, 0x1B55);

        // 2. 信号指示 (4格科技天线柱)
        int rssi_bar_x = card_x + 12;
        int rssi_bar_y = item_y + 33;
        int bars = 1;
        uint16_t sig_col = 0x7BEF;
        if (item.rssi >= -60) {
            bars = 4;
            sig_col = 0x07E0; // 极佳：纯绿
        } else if (item.rssi >= -70) {
            bars = 3;
            sig_col = 0x27E0; // 良好：鲜绿
        } else if (item.rssi >= -80) {
            bars = 2;
            sig_col = 0xFDC0; // 一般：暖金
        } else {
            bars = 1;
            sig_col = 0xF940; // 较弱：暖橙
        }

        for (int b = 0; b < 4; b++) {
            int bh = (b + 1) * 3 + 3;
            uint16_t b_col = (b < bars) ? sig_col : 0x2124;
            display.fillRect(rssi_bar_x + b * 4, rssi_bar_y - bh, 3, bh, b_col);
        }

        // 3. 信号数值显示 (例如 -52dB, -68dB)
        display.setFont(&fonts::Font0);
        display.setTextDatum(middle_left);
        display.setTextSize(1);
        display.setTextColor(sig_col, 0x0821);
        char rssi_str[16];
        snprintf(rssi_str, sizeof(rssi_str), "%ddB", (int)item.rssi);
        display.drawString(rssi_str, card_x + 32, item_y + ITEM_H / 2);

        // 4. SSID 名称 (大号中文 efontCN_14 字体，清晰支持全中文/特殊符号)
        display.setFont(&fonts::efontCN_14);
        display.setTextDatum(middle_left);
        display.setTextSize(1);
        display.setTextColor(0xFFFF, 0x0821);

        // 右侧留出锁图标区域 (约 45px)
        display.drawString(item.ssid, card_x + 78, item_y + ITEM_H / 2);

        // 5. 加密标识
        display.setFont(&fonts::Font0);
        display.setTextDatum(middle_right);
        if (item.is_open) {
            display.setTextColor(0x07E0, 0x0821);
            display.drawString("OPEN", card_x + card_w - 10, item_y + ITEM_H / 2);
        } else {
            display.setTextColor(0xFDC0, 0x0821);
            display.drawString("[LOCK]", card_x + card_w - 10, item_y + ITEM_H / 2);
        }
    }

}

void WifiConfigView::renderTrapezoidKeyboard(M5GFX& display) {
    display.fillScreen(COLOR_BG);

    // 1. 顶部网络信息与返回按键 (Y: 22 ~ 54)
    display.fillRoundRect(50, 24, 68, 26, 13, 0x0842);
    display.drawRoundRect(50, 24, 68, 26, 13, 0x2354);
    display.setTextDatum(middle_center);
    display.setFont(&fonts::Font0);
    display.setTextSize(1);
    display.setTextColor(0x7BEF, 0x0842);
    display.drawString("< SSID", 84, 37);

    char title_buf[64];
    snprintf(title_buf, sizeof(title_buf), "%s", selected_ssid);
    display.setFont(&fonts::efontCN_14);
    display.setTextDatum(middle_center);
    display.setTextColor(COLOR_ACCENT_CYAN, COLOR_BG);
    display.drawString(title_buf, SCREEN_CENTER + 40, 37);
    display.setFont(&fonts::Font0);

    // 2. 密码输入显示框 (下移优化至 Y: 64 ~ 116, 宽 340, 高 50, 圆角 12)
    int16_t box_w = 340;
    int16_t box_h = 50;
    int16_t box_x = (SCREEN_WIDTH - box_w) / 2;
    int16_t box_y = 64;

    display.fillRoundRect(box_x, box_y, box_w, box_h, 12, 0x0821);
    display.drawRoundRect(box_x, box_y, box_w, box_h, 12, COLOR_ACCENT_CYAN);

    // 密码文本或掩码 (使用 16px Font2 大字体)
    char show_buf[65] = {0};
    uint32_t now = millis();
    if (show_password_plain) {
        strncpy(show_buf, input_password, sizeof(show_buf) - 1);
    } else {
        for (size_t i = 0; i < pass_cursor; i++) {
            if (i == pass_cursor - 1 && (now - last_char_input_ms < 900)) {
                show_buf[i] = input_password[i];
            } else {
                show_buf[i] = '*';
            }
        }
        show_buf[pass_cursor] = '\0';
    }

    display.setTextDatum(middle_left);
    display.setFont(&fonts::Font2);
    display.setTextSize(1);
    display.setTextColor(0xFFFF, 0x0821);
    if (pass_cursor == 0) {
        display.setFont(&fonts::Font0);
        display.setTextColor(0x52AA, 0x0821);
        display.drawString("ENTER PASSWORD...", box_x + 14, box_y + box_h / 2);
    } else {
        display.drawString(show_buf, box_x + 14, box_y + box_h / 2);
        // 闪烁光标
        if ((now / 450) % 2 == 0) {
            int16_t text_w = display.textWidth(show_buf);
            display.drawFastVLine(box_x + 14 + text_w + 2, box_y + 12, box_h - 24, 0x07E0);
        }
    }

    // 密码框右侧动作按钮: 眼睛明暗切换 (Eye) 与 清除 (Clear)
    int16_t eye_x = box_x + box_w - 62;
    display.fillRoundRect(eye_x, box_y + 11, 26, 28, 6, 0x1B55);
    display.setTextDatum(middle_center);
    display.setFont(&fonts::Font0);
    display.setTextColor(show_password_plain ? 0x07E0 : 0x7BEF, 0x1B55);
    display.drawString(show_password_plain ? "EYE" : "***", eye_x + 13, box_y + box_h / 2);

    int16_t clr_x = box_x + box_w - 32;
    display.fillRoundRect(clr_x, box_y + 11, 24, 28, 6, 0x3186);
    display.setTextColor(0xFFFF, 0x3186);
    display.drawString("X", clr_x + 12, box_y + box_h / 2);

    // 3. 提示信息 (Y: 126 ~ 185 区域，高雅充裕)
    display.setTextDatum(middle_center);
    display.setFont(&fonts::Font0);
    display.setTextColor(COLOR_ACCENT_CYAN, COLOR_BG);
    display.drawString("* HOLD KEY FOR NUM & @ . _ - *", SCREEN_CENTER, 134);

    char info_buf[64];
    snprintf(info_buf, sizeof(info_buf), "LEN: %u/64  |  MODE: %s", 
             (unsigned int)pass_cursor,
             kb_mode == KB_MODE_LOWER ? "abc" : (kb_mode == KB_MODE_UPPER ? "CAPS" : "?123"));
    display.setTextColor(0x7BEF, COLOR_BG);
    display.drawString(info_buf, SCREEN_CENTER, 154);

    // 4. 倒梯形键盘按键绘制 (下移至 Y: 210, 4 行大按键)
    for (size_t i = 0; i < key_count; i++) {
        const KeyButton& k = keys[i];
        bool is_pressed = ((int)i == pressed_key_idx && (now - key_press_feedback_ms < 200));

        uint16_t bg_col = 0x0842;
        uint16_t border_col = 0x2354;
        uint16_t text_col = 0xFFFF;

        if (k.action_code == 1) { // Shift
            if (kb_mode == KB_MODE_UPPER) {
                bg_col = 0x1B55;
                border_col = COLOR_ACCENT_CYAN;
                text_col = 0xFFFF;
            } else {
                bg_col = 0x0821;
                border_col = 0x2354;
                text_col = 0x7BEF;
            }
        } else if (k.action_code == 2) { // Del
            bg_col = 0x3986;
            border_col = 0x7186;
            text_col = 0xFFFF;
        } else if (k.action_code == 3) { // Mode
            if (kb_mode == KB_MODE_SYMBOLS) {
                bg_col = 0x1B55;
                border_col = COLOR_ACCENT_CYAN;
            }
        } else if (k.action_code == 5) { // Connect
            bg_col = 0x03E0;
            border_col = 0x07E0;
            text_col = 0xFFFF;
        }

        if (is_pressed) {
            bg_col = is_long_press_triggered ? 0x07E0 : 0x07FF; // 长按时高亮纯绿
            text_col = 0x0000;
        }

        display.fillRoundRect(k.x, k.y, k.w, k.h, 7, bg_col);
        display.drawRoundRect(k.x, k.y, k.w, k.h, 7, border_col);

        if (k.label) {
            display.setFont(&fonts::Font0);
            display.setTextDatum(middle_center);
            display.setTextSize(1);
            display.setTextColor(text_col, bg_col);
            if (k.action_code == 3) {
                display.drawString(kb_mode == KB_MODE_SYMBOLS ? "ABC" : "?123", k.x + k.w / 2, k.y + k.h / 2);
            } else {
                display.drawString(k.label, k.x + k.w / 2, k.y + k.h / 2);
            }
        } else {
            char ch = k.normal_char;
            if (kb_mode == KB_MODE_UPPER) ch = k.shift_char;
            else if (kb_mode == KB_MODE_SYMBOLS) ch = k.symbol_char;

            // 1. 大号居中主字符 (使用 16px Font2 大字体)
            display.setFont(&fonts::Font2);
            display.setTextDatum(middle_center);
            display.setTextSize(1);
            display.setTextColor(text_col, bg_col);
            char ch_str[2] = { ch, '\0' };
            int16_t cy = (kb_mode != KB_MODE_SYMBOLS && k.long_press_char != 0) ? (k.y + k.h / 2 + 2) : (k.y + k.h / 2);
            display.drawString(ch_str, k.x + k.w / 2, cy);

            // 2. 右上角长按快捷角标 (数字 1~0, 常见符号 @ _ . - ! # $ * ? 等)
            if (kb_mode != KB_MODE_SYMBOLS && k.long_press_char != 0) {
                display.setFont(&fonts::Font0);
                display.setTextDatum(top_right);
                display.setTextSize(1);
                uint16_t sub_col = is_pressed ? 0x0000 : 0x07FF;
                display.setTextColor(sub_col, bg_col);
                char sub_str[2] = { k.long_press_char, '\0' };
                display.drawString(sub_str, k.x + k.w - 3, k.y + 3);
            }
        }
    }
}

void WifiConfigView::renderConnecting(M5GFX& display) {
    display.fillScreen(COLOR_BG);

    uint32_t now = millis();
    int angle = (now / 3) % 360;

    display.drawArc(SCREEN_CENTER, SCREEN_CENTER, 80, 74, angle, (angle + 120) % 360, COLOR_ACCENT_CYAN);
    display.drawArc(SCREEN_CENTER, SCREEN_CENTER, 65, 60, (360 - angle), (480 - angle) % 360, 0x07E0);

    display.setTextDatum(middle_center);
    display.setTextSize(1);
    display.setTextColor(COLOR_ACCENT_CYAN, COLOR_BG);
    display.drawString("CONNECTING...", SCREEN_CENTER, 140);

    display.setTextColor(0xFFFF, COLOR_BG);
    char buf[48];
    snprintf(buf, sizeof(buf), "SSID: %s", selected_ssid);
    display.drawString(buf, SCREEN_CENTER, 280);

    display.setTextColor(0x7BEF, COLOR_BG);
    display.drawString("AUTHENTICATING & ACQUIRING IP...", SCREEN_CENTER, 305);
}

void WifiConfigView::renderSuccess(M5GFX& display) {
    display.fillScreen(COLOR_BG);

    // 成功对勾环
    display.drawCircle(SCREEN_CENTER, SCREEN_CENTER - 20, 60, 0x07E0);
    display.fillCircle(SCREEN_CENTER, SCREEN_CENTER - 20, 54, 0x0320);

    // 对勾
    display.drawLine(SCREEN_CENTER - 24, SCREEN_CENTER - 20, SCREEN_CENTER - 8, SCREEN_CENTER - 4, 0xFFFF);
    display.drawLine(SCREEN_CENTER - 24, SCREEN_CENTER - 19, SCREEN_CENTER - 8, SCREEN_CENTER - 3, 0xFFFF);
    display.drawLine(SCREEN_CENTER - 8, SCREEN_CENTER - 4, SCREEN_CENTER + 24, SCREEN_CENTER - 36, 0xFFFF);
    display.drawLine(SCREEN_CENTER - 8, SCREEN_CENTER - 3, SCREEN_CENTER + 24, SCREEN_CENTER - 35, 0xFFFF);

    display.setTextDatum(middle_center);
    display.setTextSize(1);
    display.setTextColor(0x07E0, COLOR_BG);
    display.drawString("CONNECTED TO WI-FI!", SCREEN_CENTER, SCREEN_CENTER + 65);

    char ip_buf[48];
    snprintf(ip_buf, sizeof(ip_buf), "IP: %s", network_mgr.getIP());
    display.setTextColor(0xFFFF, COLOR_BG);
    display.drawString(ip_buf, SCREEN_CENTER, SCREEN_CENTER + 90);

    display.setTextColor(0x7BEF, COLOR_BG);
    display.drawString("SAVED TO NVS. ENTERING 3D GLOBE...", SCREEN_CENTER, SCREEN_CENTER + 115);
}

void WifiConfigView::renderFailed(M5GFX& display) {
    display.fillScreen(COLOR_BG);

    display.drawCircle(SCREEN_CENTER, SCREEN_CENTER - 35, 55, 0xF940);
    display.fillCircle(SCREEN_CENTER, SCREEN_CENTER - 35, 49, 0x4800);

    // 感叹号
    display.fillRect(SCREEN_CENTER - 3, SCREEN_CENTER - 60, 6, 30, 0xFFFF);
    display.fillCircle(SCREEN_CENTER, SCREEN_CENTER - 18, 4, 0xFFFF);

    display.setTextDatum(middle_center);
    display.setTextSize(1);
    display.setTextColor(0xF940, COLOR_BG);
    display.drawString("CONNECTION FAILED!", SCREEN_CENTER, SCREEN_CENTER + 40);

    display.setTextColor(0x7BEF, COLOR_BG);
    display.drawString("CHECK PASSWORD OR AP DISTANCE", SCREEN_CENTER, SCREEN_CENTER + 62);

    // 重试密码按钮
    display.fillRoundRect(SCREEN_CENTER - 140, SCREEN_CENTER + 90, 130, 36, 18, 0x1B55);
    display.drawRoundRect(SCREEN_CENTER - 140, SCREEN_CENTER + 90, 130, 36, 18, COLOR_ACCENT_CYAN);
    display.setTextColor(0xFFFF, 0x1B55);
    display.drawString("RETRY PASS", SCREEN_CENTER - 75, SCREEN_CENTER + 108);

    // 重选网络按钮
    display.fillRoundRect(SCREEN_CENTER + 10, SCREEN_CENTER + 90, 130, 36, 18, 0x0842);
    display.drawRoundRect(SCREEN_CENTER + 10, SCREEN_CENTER + 90, 130, 36, 18, 0x2354);
    display.setTextColor(0xBDD7, 0x0842);
    display.drawString("OTHER SSID", SCREEN_CENTER + 75, SCREEN_CENTER + 108);
}

// -------------------------------------------------------------
// 触控交互分发
// -------------------------------------------------------------
bool WifiConfigView::handleTouch(bool is_pressed, int16_t x, int16_t y, uint32_t now_ms) {
    if (current_state == WIFI_CFG_INACTIVE) return false;

    if (current_state == WIFI_CFG_BOOT_CHECK) {
        if (!is_pressed && touch_start_ms > 0) {
            // 点击 SKIP 离线
            if (abs(x - SCREEN_CENTER) < 70 && abs(y - 386) < 25) {
                close();
            }
            touch_start_ms = 0;
        } else if (is_pressed && touch_start_ms == 0) {
            touch_start_ms = now_ms;
        }
        return true;
    }

    if (current_state == WIFI_CFG_SSID_LIST) {
        return handleTouchSSIDList(is_pressed, x, y, now_ms);
    } else if (current_state == WIFI_CFG_KEYBOARD) {
        return handleTouchKeyboard(is_pressed, x, y, now_ms);
    } else if (current_state == WIFI_CFG_FAILED) {
        return handleTouchResult(is_pressed, x, y, now_ms);
    }

    return true; // 连接中或成功态全屏拦截点击
}

bool WifiConfigView::handleTouchSSIDList(bool is_pressed, int16_t x, int16_t y, uint32_t now_ms) {
    if (is_pressed) {
        if (touch_start_ms == 0) {
            touch_start_ms = now_ms;
            touch_start_x = x;
            touch_start_y = y;
            touch_last_y = y;
            is_dragging_list = false;
        } else {
            int16_t dy = y - touch_last_y;
            touch_last_y = y;
            if (!is_dragging_list && (abs(y - touch_start_y) > 8)) {
                is_dragging_list = true;
            }
            if (is_dragging_list) {
                list_scroll_y += dy;
                target_scroll_y = list_scroll_y;
            }
        }
    } else {
        if (touch_start_ms > 0) {
            uint32_t duration = now_ms - touch_start_ms;
            int16_t total_dx = x - touch_start_x;
            int16_t total_dy = y - touch_start_y;

            // 限制滚动边界
            const auto& list = network_mgr.getScannedList();
            int count = (int)list.size();
            float min_scroll = -(float)max(0, count * 54 - 320);
            if (target_scroll_y > 0.0f) target_scroll_y = 0.0f;
            if (target_scroll_y < min_scroll) target_scroll_y = min_scroll;

            if (!is_dragging_list && duration < 400 && abs(total_dx) < 15 && abs(total_dy) < 15) {
                // 点击顶部刷新按钮
                if (touch_start_y >= 44 && touch_start_y <= 74 && touch_start_x >= 60 && touch_start_x <= 140) {
                    triggerScan();
                    touch_start_ms = 0;
                    return true;
                }
                // 点击顶部跳过/离线按钮
                if (touch_start_y >= 44 && touch_start_y <= 74 && touch_start_x >= SCREEN_WIDTH - 140 && touch_start_x <= SCREEN_WIDTH - 60) {
                    close();
                    touch_start_ms = 0;
                    return true;
                }

                // 点击 SSID 列表项
                const int ITEM_H = 50;
                const int LIST_TOP = 82;
                if (touch_start_y >= LIST_TOP && touch_start_y <= 410) {
                    int clicked_idx = (int)(touch_start_y - LIST_TOP - list_scroll_y) / (ITEM_H + 4);

                    if (clicked_idx >= 0 && clicked_idx < count) {
                        const auto& item = list[clicked_idx];
                        strncpy(selected_ssid, item.ssid, sizeof(selected_ssid) - 1);
                        selected_ssid[sizeof(selected_ssid) - 1] = '\0';

                        if (item.is_open) {
                            // 开放网络免密直连
                            input_password[0] = '\0';
                            pass_cursor = 0;
                            network_mgr.connect(selected_ssid, "");
                            switchState(WIFI_CFG_CONNECTING);
                        } else {
                            // 进入倒梯形键盘密码输入
                            input_password[0] = '\0';
                            pass_cursor = 0;
                            switchState(WIFI_CFG_KEYBOARD);
                        }
                    }
                }
            }
            touch_start_ms = 0;
            is_dragging_list = false;
        }
    }
    return true;
}

bool WifiConfigView::handleTouchKeyboard(bool is_pressed, int16_t x, int16_t y, uint32_t now_ms) {
    if (is_pressed) {
        if (touch_start_ms == 0) {
            touch_start_ms = now_ms;
            touch_start_x = x;
            touch_start_y = y;
            press_start_ms = now_ms;
            is_long_press_triggered = false;

            // 检查倒梯形键盘按键碰撞
            for (size_t i = 0; i < key_count; i++) {
                const KeyButton& k = keys[i];
                if (x >= k.x && x < k.x + k.w && y >= k.y && y < k.y + k.h) {
                    pressed_key_idx = (int)i;
                    key_press_feedback_ms = now_ms;
                    break;
                }
            }
        } else {
            // 持续按住中：判断是否达到长按阈值 (380ms)
            if (!is_long_press_triggered && pressed_key_idx >= 0 && (now_ms - press_start_ms >= 380)) {
                const KeyButton& k = keys[pressed_key_idx];
                if (k.action_code == 0 && k.long_press_char != 0 && kb_mode != KB_MODE_SYMBOLS) {
                    appendChar(k.long_press_char);
                    is_long_press_triggered = true;
                    key_press_feedback_ms = now_ms; // 再次触发高亮
                    Serial.printf("[Keyboard] Long-press input: '%c'\n", k.long_press_char);
                }
            }
        }
    } else {
        if (touch_start_ms > 0) {
            uint32_t duration = now_ms - touch_start_ms;
            int16_t total_dx = x - touch_start_x;
            int16_t total_dy = y - touch_start_y;

            if (duration < 550 && abs(total_dx) < 20 && abs(total_dy) < 20) {
                // 1. 返回 SSID 列表按钮 (X: 45~125, Y: 20~54)
                if (touch_start_x >= 45 && touch_start_x <= 125 && touch_start_y >= 20 && touch_start_y <= 54) {
                    switchState(WIFI_CFG_SSID_LIST);
                    touch_start_ms = 0;
                    return true;
                }

                // 2. 密码框右侧: 眼睛切换 (Eye) 与 清除 (Clear)
                int16_t box_w = 340;
                int16_t box_x = (SCREEN_WIDTH - box_w) / 2;
                int16_t box_y = 64;
                int16_t eye_x = box_x + box_w - 62;
                int16_t clr_x = box_x + box_w - 32;

                if (touch_start_y >= box_y && touch_start_y <= box_y + 50) {
                    if (touch_start_x >= eye_x && touch_start_x <= eye_x + 28) {
                        show_password_plain = !show_password_plain;
                        touch_start_ms = 0;
                        return true;
                    }
                    if (touch_start_x >= clr_x && touch_start_x <= clr_x + 28) {
                        input_password[0] = '\0';
                        pass_cursor = 0;
                        touch_start_ms = 0;
                        return true;
                    }
                }

                // 3. 倒梯形键盘按键短按判定 (若未触发长按)
                if (!is_long_press_triggered) {
                    for (size_t i = 0; i < key_count; i++) {
                        const KeyButton& k = keys[i];
                        if (touch_start_x >= k.x && touch_start_x < k.x + k.w &&
                            touch_start_y >= k.y && touch_start_y < k.y + k.h) 
                        {
                            if (k.action_code == 0) {
                                char ch = k.normal_char;
                                if (kb_mode == KB_MODE_UPPER) ch = k.shift_char;
                                else if (kb_mode == KB_MODE_SYMBOLS) ch = k.symbol_char;
                                appendChar(ch);
                            } else if (k.action_code == 1) { // Shift
                                if (kb_mode == KB_MODE_LOWER) kb_mode = KB_MODE_UPPER;
                                else kb_mode = KB_MODE_LOWER;
                            } else if (k.action_code == 2) { // Del
                                deleteChar();
                            } else if (k.action_code == 3) { // Mode Toggle
                                if (kb_mode == KB_MODE_SYMBOLS) kb_mode = KB_MODE_LOWER;
                                else kb_mode = KB_MODE_SYMBOLS;
                            } else if (k.action_code == 4) { // Space
                                appendChar(' ');
                            } else if (k.action_code == 5) { // Connect
                                if (strlen(selected_ssid) > 0) {
                                    network_mgr.connect(selected_ssid, input_password);
                                    switchState(WIFI_CFG_CONNECTING);
                                }
                            }
                            break;
                        }
                    }
                }
            }
            touch_start_ms = 0;
            is_long_press_triggered = false;
        }
    }
    return true;
}

bool WifiConfigView::handleTouchResult(bool is_pressed, int16_t x, int16_t y, uint32_t now_ms) {
    if (!is_pressed && touch_start_ms > 0) {
        // 重试密码
        if (touch_start_x >= SCREEN_CENTER - 140 && touch_start_x <= SCREEN_CENTER - 10 &&
            touch_start_y >= SCREEN_CENTER + 90 && touch_start_y <= SCREEN_CENTER + 126) 
        {
            switchState(WIFI_CFG_KEYBOARD);
        }
        // 重新选择网络
        else if (touch_start_x >= SCREEN_CENTER + 10 && touch_start_x <= SCREEN_CENTER + 140 &&
                 touch_start_y >= SCREEN_CENTER + 90 && touch_start_y <= SCREEN_CENTER + 126) 
        {
            switchState(WIFI_CFG_SSID_LIST);
        }
        touch_start_ms = 0;
    } else if (is_pressed && touch_start_ms == 0) {
        touch_start_ms = now_ms;
        touch_start_x = x;
        touch_start_y = y;
    }
    return true;
}
