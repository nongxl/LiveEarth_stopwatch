#include <M5Unified.h>
#include <esp_heap_caps.h>
#include "config.h"
#include "generated_data.h"

// -------------------------------------------------------------
// Global Rendering & System State
// -------------------------------------------------------------
static M5Canvas canvas(&M5.Display);

struct TouchVisualState {
    int16_t x = -1;
    int16_t y = -1;
    bool is_pressed = false;
    uint32_t press_start_ms = 0;
    float ripple_radius = 0.0f;
} touch_state;

struct PerformanceMetrics {
    uint32_t frame_count = 0;
    uint32_t last_fps_update = 0;
    float current_fps = 0.0f;
    size_t free_heap_sram = 0;
    size_t free_psram = 0;
} perf;

static uint32_t btn_click_count = 0;
static float breath_phase = 0.0f;

// -------------------------------------------------------------
// Helper: Print System & Memory Diagnostics
// -------------------------------------------------------------
static void print_system_diagnostics() {
    Serial.println("\n==================================================");
    Serial.println("  LiveEarth Firmware - Hardware Self-Test");
    Serial.println("  Device: M5Stack Stopwatch (CO5300 AMOLED 466x466)");
    Serial.println("==================================================");

    Serial.printf("Chip Model:        %s (Rev %d)\n", ESP.getChipModel(), ESP.getChipRevision());
    Serial.printf("CPU Frequency:     %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("Flash Size:        %u MB (Speed: %u MHz)\n", 
                  ESP.getFlashChipSize() / (1024 * 1024), 
                  ESP.getFlashChipSpeed() / 1000000);

    size_t sram_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t sram_free  = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    size_t psram_free  = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    Serial.printf("Internal SRAM:     Total: %u KB, Free: %u KB, Min Free: %u KB\n",
                  (uint32_t)(sram_total / 1024),
                  (uint32_t)(sram_free / 1024),
                  (uint32_t)(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024));

    Serial.printf("External PSRAM:    Total: %u KB, Free: %u KB, Min Free: %u KB\n",
                  (uint32_t)(psram_total / 1024),
                  (uint32_t)(psram_free / 1024),
                  (uint32_t)(heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM) / 1024));

    auto board = M5.getBoard();
    Serial.printf("Detected Board ID: %d\n", (int)board);
    Serial.printf("Display Width:     %d, Height: %d\n", M5.Display.width(), M5.Display.height());
    Serial.printf("Offline Places:    %u places ready in ROM\n", (uint32_t)OFFLINE_PLACES_COUNT);
    Serial.println("==================================================\n");
}

// -------------------------------------------------------------
// Setup
// -------------------------------------------------------------
void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(300);

    auto cfg = M5.config();
    cfg.internal_imu = true;
    cfg.internal_rtc = true;
    M5.begin(cfg);

    print_system_diagnostics();

    // 屏幕与背光设置
    M5.Display.setBrightness(180);
    M5.Display.setRotation(0);

    // 为 466x466 双缓冲分配 PSRAM 画布 (16位色: 466 * 466 * 2 = 434 KB)
    canvas.setColorDepth(16);
    canvas.setPsram(true);
    if (!canvas.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT)) {
        Serial.println("[ERROR] Failed to allocate 466x466 Canvas in PSRAM! Trying display fallback.");
    } else {
        Serial.println("[OK] 466x466 Canvas successfully created in PSRAM.");
    }

    perf.last_fps_update = millis();
}

// -------------------------------------------------------------
// Render Helpers for Round Display Aesthetics
// -------------------------------------------------------------
static void draw_circular_hud() {
    // 1. 深色背景清屏
    canvas.fillScreen(COLOR_BG);

    // 2. 外部安全圆形辅助线（微妙极暗圆环）
    canvas.drawCircle(SCREEN_CENTER_X, SCREEN_CENTER_Y, SAFE_UI_RADIUS, 0x18E3);
    canvas.drawCircle(SCREEN_CENTER_X, SCREEN_CENTER_Y, SAFE_UI_RADIUS - 6, 0x10C2);

    // 3. 四方刻度 (N / S / W / E)
    canvas.setTextColor(COLOR_TEXT_MUTED);
    canvas.setTextDatum(middle_center);
    canvas.setTextSize(1);
    canvas.drawString("N", SCREEN_CENTER_X, SCREEN_CENTER_Y - SAFE_UI_RADIUS + 14);
    canvas.drawString("S", SCREEN_CENTER_X, SCREEN_CENTER_Y + SAFE_UI_RADIUS - 14);
    canvas.drawString("W", SCREEN_CENTER_X - SAFE_UI_RADIUS + 14, SCREEN_CENTER_Y);
    canvas.drawString("E", SCREEN_CENTER_X + SAFE_UI_RADIUS - 14, SCREEN_CENTER_Y);

    // 4. 中央 3D 地球底座拟态环（带柔和正弦呼吸动效）
    breath_phase += 0.05f;
    if (breath_phase > 6.28318f) breath_phase -= 6.28318f;
    float breath_scale = sinf(breath_phase);
    int dynamic_r = GLOBE_DEFAULT_RADIUS + (int)(breath_scale * 3.0f);

    // 大气层外发光环
    canvas.drawCircle(SCREEN_CENTER_X, SCREEN_CENTER_Y, dynamic_r + 4, 0x01E6);
    canvas.drawCircle(SCREEN_CENTER_X, SCREEN_CENTER_Y, dynamic_r, COLOR_EARTH_GRID);
    canvas.fillCircle(SCREEN_CENTER_X, SCREEN_CENTER_Y, dynamic_r - 1, COLOR_EARTH_OCEAN);

    // 经纬线网格基线示意
    canvas.drawEllipse(SCREEN_CENTER_X, SCREEN_CENTER_Y, dynamic_r - 2, (dynamic_r - 2) / 3, COLOR_EARTH_GRID);
    canvas.drawLine(SCREEN_CENTER_X, SCREEN_CENTER_Y - dynamic_r + 2, SCREEN_CENTER_X, SCREEN_CENTER_Y + dynamic_r - 2, COLOR_EARTH_GRID);

    // 5. 顶部标题区域
    canvas.setTextColor(COLOR_ACCENT_CYAN);
    canvas.setTextDatum(middle_center);
    canvas.setTextSize(2);
    canvas.drawString("LIVE EARTH", SCREEN_CENTER_X, 85);

    canvas.setTextColor(COLOR_TEXT_MUTED);
    canvas.setTextSize(1);
    canvas.drawString("3D WEBCAM EXPLORER", SCREEN_CENTER_X, 110);

    // 6. 中央信息状态卡片
    canvas.setTextColor(0xFFFF);
    canvas.setTextDatum(middle_center);
    canvas.setTextSize(2);
    canvas.drawString("PHASE 1 FOUNDATION", SCREEN_CENTER_X, SCREEN_CENTER_Y - 15);

    canvas.setTextColor(COLOR_ACCENT_GOLD);
    canvas.setTextSize(1);
    char place_buf[48];
    snprintf(place_buf, sizeof(place_buf), "PLACES INDEXED: %u", (uint32_t)OFFLINE_PLACES_COUNT);
    canvas.drawString(place_buf, SCREEN_CENTER_X, SCREEN_CENTER_Y + 15);

    // 7. 触控微反馈涟漪与光标
    if (touch_state.is_pressed && touch_state.x >= 0) {
        touch_state.ripple_radius += 1.5f;
        if (touch_state.ripple_radius > 35.0f) touch_state.ripple_radius = 5.0f;

        canvas.drawCircle(touch_state.x, touch_state.y, (int32_t)touch_state.ripple_radius, COLOR_ACCENT_CYAN);
        canvas.fillCircle(touch_state.x, touch_state.y, 4, COLOR_ACCENT_GOLD);

        // 提示当前触摸坐标
        char coord_buf[32];
        snprintf(coord_buf, sizeof(coord_buf), "X:%d Y:%d", touch_state.x, touch_state.y);
        canvas.setTextColor(COLOR_ACCENT_CYAN);
        canvas.drawString(coord_buf, touch_state.x, touch_state.y - 20);
    }

    // 8. 底部硬件指标 HUD (FPS / SRAM / PSRAM)
    canvas.setTextColor(COLOR_TEXT_MUTED);
    canvas.setTextDatum(middle_center);
    canvas.setTextSize(1);

    char perf_buf[64];
    snprintf(perf_buf, sizeof(perf_buf), "FPS: %.1f | BTN: %u", perf.current_fps, btn_click_count);
    canvas.drawString(perf_buf, SCREEN_CENTER_X, 355);

    char mem_buf[64];
    snprintf(mem_buf, sizeof(mem_buf), "SRAM: %uKB | PSRAM: %uKB",
             (uint32_t)(perf.free_heap_sram / 1024),
             (uint32_t)(perf.free_psram / 1024));
    canvas.drawString(mem_buf, SCREEN_CENTER_X, 375);

    // 圆形视界边缘掩码抗锯齿光圈
    canvas.drawCircle(SCREEN_CENTER_X, SCREEN_CENTER_Y, SCREEN_RADIUS - 1, 0x0000);
}

// -------------------------------------------------------------
// Loop
// -------------------------------------------------------------
void loop() {
    M5.update();

    // 1. 处理按键事件
    if (M5.BtnA.wasPressed()) {
        btn_click_count++;
        Serial.printf("[Button] BtnA pressed! Total: %u\n", btn_click_count);
        // 振动反馈（如果硬件支持震动马达）
        #if defined(M5UNIFIED_PC)
        #else
        // M5.Power.setVibration(100);
        #endif
    }

    // 2. 处理触摸交互
    auto detail = M5.Touch.getDetail();
    if (detail.isPressed()) {
        touch_state.x = detail.x;
        touch_state.y = detail.y;
        touch_state.is_pressed = true;
    } else {
        touch_state.is_pressed = false;
        touch_state.ripple_radius = 0.0f;
    }

    // 3. 计算 FPS 与内存健康指标
    perf.frame_count++;
    uint32_t now = millis();
    if (now - perf.last_fps_update >= 500) {
        perf.current_fps = (perf.frame_count * 1000.0f) / (float)(now - perf.last_fps_update);
        perf.frame_count = 0;
        perf.last_fps_update = now;
        perf.free_heap_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        perf.free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    }

    // 4. 渲染与屏幕刷新
    draw_circular_hud();
    canvas.pushSprite(0, 0);

    // 控制刷新节拍，维持约 35~40 FPS 稳定帧率以降低发热与功耗
    delay(10);
}
