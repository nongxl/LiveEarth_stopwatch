#include <M5Unified.h>
#include <esp_heap_caps.h>
#include "config.h"
#include "generated_data.h"
#include "globe.h"

// -------------------------------------------------------------
// Global Rendering & Engine State
// -------------------------------------------------------------
static M5Canvas canvas(&M5.Display);
static GlobeEngine globe;

struct TouchGestureState {
    int16_t last_x = -1;
    int16_t last_y = -1;
    int16_t start_x = -1;
    int16_t start_y = -1;
    bool is_dragging = false;
    uint32_t touch_start_ms = 0;
} touch_state;

struct PerformanceMetrics {
    uint32_t frame_count = 0;
    uint32_t last_fps_update = 0;
    uint32_t last_frame_time = 0;
    float current_fps = 0.0f;
    size_t free_heap_sram = 0;
    size_t free_psram = 0;
} perf;

static uint32_t btn_click_count = 0;

// -------------------------------------------------------------
// Helper: Print System & Memory Diagnostics
// -------------------------------------------------------------
static void print_system_diagnostics() {
    Serial.println("\n==================================================");
    Serial.println("  LiveEarth Firmware - Hardware Diagnostics");
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
    Serial.printf("Display Res:       %dx%d\n", M5.Display.width(), M5.Display.height());
    Serial.printf("Offline Places:    %u in ROM\n", (uint32_t)OFFLINE_PLACES_COUNT);
    Serial.println("==================================================\n");
}

// -------------------------------------------------------------
// Setup
// -------------------------------------------------------------
void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(200);

    auto cfg = M5.config();
    cfg.internal_imu = true;
    cfg.internal_rtc = true;
    M5.begin(cfg);

    print_system_diagnostics();

    // 屏幕与背光设置
    M5.Display.setBrightness(180);
    M5.Display.setRotation(0);

    // 在 8MB PSRAM 中分配 466x466 16位全屏画布
    canvas.setColorDepth(16);
    canvas.setPsram(true);
    if (!canvas.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT)) {
        Serial.println("[ERROR] Failed to allocate Canvas in PSRAM!");
    } else {
        Serial.println("[OK] 466x466 Canvas successfully created in PSRAM.");
    }

    // 初始化 3D 地球引擎
    globe.init(SCREEN_CENTER_X, SCREEN_CENTER_Y, 155.0f);

    perf.last_fps_update = millis();
    perf.last_frame_time = millis();
}

// -------------------------------------------------------------
// Round HUD Overlays
// -------------------------------------------------------------
static void draw_circular_overlays() {
    // 1. 外部刻度与安全视界圆环
    canvas.drawCircle(SCREEN_CENTER_X, SCREEN_CENTER_Y, SAFE_UI_RADIUS, 0x18E3);
    canvas.drawCircle(SCREEN_CENTER_X, SCREEN_CENTER_Y, SAFE_UI_RADIUS - 6, 0x10C2);

    // 2. 四方微标 (N / S / W / E)
    canvas.setTextColor(COLOR_TEXT_MUTED);
    canvas.setTextDatum(middle_center);
    canvas.setTextSize(1);
    canvas.drawString("N", SCREEN_CENTER_X, SCREEN_CENTER_Y - SAFE_UI_RADIUS + 14);
    canvas.drawString("S", SCREEN_CENTER_X, SCREEN_CENTER_Y + SAFE_UI_RADIUS - 14);
    canvas.drawString("W", SCREEN_CENTER_X - SAFE_UI_RADIUS + 14, SCREEN_CENTER_Y);
    canvas.drawString("E", SCREEN_CENTER_X + SAFE_UI_RADIUS - 14, SCREEN_CENTER_Y);

    // 3. 顶部信息：标题与中心经纬度视点
    canvas.setTextColor(COLOR_ACCENT_CYAN);
    canvas.setTextDatum(middle_center);
    canvas.setTextSize(2);
    canvas.drawString("LIVE EARTH", SCREEN_CENTER_X, 60);

    float center_lat = globe.getPitch() * RAD_TO_DEG;
    float center_lon = globe.getYaw() * RAD_TO_DEG;
    char view_buf[48];
    snprintf(view_buf, sizeof(view_buf), "VIEW: %.1f%c, %.1f%c",
             fabsf(center_lat), (center_lat >= 0 ? 'N' : 'S'),
             fabsf(center_lon), (center_lon >= 0 ? 'E' : 'W'));

    canvas.setTextColor(0xD69A);
    canvas.setTextSize(1);
    canvas.drawString(view_buf, SCREEN_CENTER_X, 85);

    // 4. 底部状态栏 HUD (FPS / 内存指标 / 拖拽指示)
    char perf_buf[64];
    snprintf(perf_buf, sizeof(perf_buf), "FPS: %.1f | DRAG TO ROTATE", perf.current_fps);
    canvas.setTextColor(COLOR_TEXT_MUTED);
    canvas.drawString(perf_buf, SCREEN_CENTER_X, 395);

    char mem_buf[64];
    snprintf(mem_buf, sizeof(mem_buf), "SRAM: %uKB | PSRAM: %uKB",
             (uint32_t)(perf.free_heap_sram / 1024),
             (uint32_t)(perf.free_psram / 1024));
    canvas.drawString(mem_buf, SCREEN_CENTER_X, 415);
}

// -------------------------------------------------------------
// Loop
// -------------------------------------------------------------
void loop() {
    M5.update();

    uint32_t now = millis();
    float dt_sec = (now - perf.last_frame_time) * 0.001f;
    if (dt_sec > 0.1f) dt_sec = 0.1f;
    perf.last_frame_time = now;

    // 1. 物理按键处理：一键复位视角
    if (M5.BtnA.wasPressed()) {
        btn_click_count++;
        Serial.printf("[BtnA] Resetting globe orientation to default.\n");
        globe.resetOrientation();
    }

    // 2. 触控手势处理 (拖拽旋转地球)
    auto detail = M5.Touch.getDetail();
    if (detail.isPressed()) {
        int16_t cur_x = detail.x;
        int16_t cur_y = detail.y;

        if (!touch_state.is_dragging) {
            touch_state.is_dragging = true;
            touch_state.start_x = cur_x;
            touch_state.start_y = cur_y;
            touch_state.last_x = cur_x;
            touch_state.last_y = cur_y;
            touch_state.touch_start_ms = now;
        } else {
            float dx = (float)(cur_x - touch_state.last_x);
            float dy = (float)(cur_y - touch_state.last_y);

            // 驱动 3D 地球旋转
            globe.handleDrag(dx, dy);

            touch_state.last_x = cur_x;
            touch_state.last_y = cur_y;
        }
    } else {
        touch_state.is_dragging = false;
        touch_state.last_x = -1;
        touch_state.last_y = -1;
    }

    // 3. 物理惯性更新
    globe.update(dt_sec);

    // 4. 统计帧率与内存
    perf.frame_count++;
    if (now - perf.last_fps_update >= 500) {
        perf.current_fps = (perf.frame_count * 1000.0f) / (float)(now - perf.last_fps_update);
        perf.frame_count = 0;
        perf.last_fps_update = now;
        perf.free_heap_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        perf.free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    }

    // 5. 渲染画面
    canvas.fillScreen(COLOR_BG);
    globe.render(canvas);
    draw_circular_overlays();

    // 6. 推送至物理 AMOLED 屏幕
    canvas.pushSprite(0, 0);

    delay(2);
}
