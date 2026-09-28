#pragma once

#include <M5GFX.h>
#include "network_manager.h"

enum WifiConfigState {
    WIFI_CFG_INACTIVE = 0,    // 未开启配置界面
    WIFI_CFG_BOOT_CHECK,      // 开机检测已保存网络中
    WIFI_CFG_SSID_LIST,       // SSID 扫描选择列表
    WIFI_CFG_KEYBOARD,        // 倒梯形全键盘密码输入
    WIFI_CFG_CONNECTING,      // 正在连接目标网络
    WIFI_CFG_SUCCESS,         // 连接成功提示
    WIFI_CFG_FAILED           // 连接失败提示
};

enum KeyboardMode {
    KB_MODE_LOWER = 0,        // 小写字母
    KB_MODE_UPPER,            // 大写字母 (Shift)
    KB_MODE_SYMBOLS           // 数字与常用符号
};

struct KeyButton {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    char normal_char;
    char shift_char;
    char symbol_char;
    char long_press_char;     // 长按输出字符 (数字1~0、常见符号@ . _ - 等)
    const char* label;        // 特殊按键标签 (如 "CAPS", "DEL", "SPACE", "JOIN"...)
    int action_code;          // 0: 普通字符, 1: Shift, 2: Del, 3: 切换符号, 4: Space, 5: Connect
};

class WifiConfigView {
public:
    WifiConfigView();

    void init();
    void open(bool from_boot = false);
    void close();
    bool isActive() const { return current_state != WIFI_CFG_INACTIVE; }
    WifiConfigState getState() const { return current_state; }

    void update(uint32_t now_ms);
    void render(M5GFX& display);

    // 触控交互处理，返回 true 表示事件已被本配置页面消费
    bool handleTouch(bool is_pressed, int16_t x, int16_t y, uint32_t now_ms);

    // 快捷触发重新配网
    void triggerScan();

private:
    WifiConfigState current_state;
    KeyboardMode kb_mode;

    // 当前操作的目标网络与密码缓冲区
    char selected_ssid[33];
    char input_password[65];
    size_t pass_cursor;
    bool show_password_plain; // 是否显示明文密码
    uint32_t last_char_input_ms;

    // 倒梯形按键阵列定义
    static const size_t MAX_KEYS = 35; // 3 行大按键 + 底部半圆按键
    KeyButton keys[MAX_KEYS];
    size_t key_count;
    int pressed_key_idx;
    uint32_t key_press_feedback_ms;
    uint32_t press_start_ms;
    bool is_long_press_triggered;

    // 输入框右侧 DEL 键交互控制
    bool is_del_pressed;
    uint32_t last_del_repeat_ms;


    // SSID 列表滚动与触控
    float list_scroll_y;
    float target_scroll_y;
    bool is_dragging_list;
    int16_t touch_start_x;
    int16_t touch_start_y;
    int16_t touch_last_y;
    uint32_t touch_start_ms;

    // 状态保持与超时控制
    uint32_t state_enter_ms;
    uint32_t last_scan_req_ms;

    void buildTrapezoidKeyboardLayout();
    void switchState(WifiConfigState new_state);
    void appendChar(char c);
    void deleteChar();

    // 渲染各个子界面
    void renderBootCheck(M5GFX& display);
    void renderSSIDList(M5GFX& display);
    void renderTrapezoidKeyboard(M5GFX& display);
    void renderConnecting(M5GFX& display);
    void renderSuccess(M5GFX& display);
    void renderFailed(M5GFX& display);

    // 触控子分支
    bool handleTouchSSIDList(bool is_pressed, int16_t x, int16_t y, uint32_t now_ms);
    bool handleTouchKeyboard(bool is_pressed, int16_t x, int16_t y, uint32_t now_ms);
    bool handleTouchResult(bool is_pressed, int16_t x, int16_t y, uint32_t now_ms);
};

extern WifiConfigView wifi_config_view;
