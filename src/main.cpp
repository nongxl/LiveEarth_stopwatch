#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <time.h>
#include "config.h"
#include "generated_data.h"
#include "globe.h"
#include "sun_calc.h"
#include "place_marker.h"

// -------------------------------------------------------------
// Global Rendering & Engine State
// -------------------------------------------------------------
static M5Canvas canvas(&M5.Display);
static GlobeEngine globe;
static SunCalculator sun_calc;
static PlaceMarkerManager place_mgr;

struct TouchGestureState {
    int16_t last_x = -1;
    int16_t last_y = -1;
    int16_t start_x = -1;
    int16_t start_y = -1;
    bool is_dragging = false;
    bool was_pressed = false;
    uint32_t touch_start_ms = 0;
} touch_state;

struct PerformanceMetrics {
    uint32_t frame_count = 0;
    uint32_t last_fps_update = 0;
    uint32_t last_frame_time = 0;
    uint32_t last_sun_update = 0;
    uint32_t last_hud_update = 0;
    float current_fps = 0.0f;
    size_t free_heap_sram = 0;
    size_t free_psram = 0;
} perf;

static uint32_t btn_click_count = 0;

// 默认基准时间：2026年9月23日 09:30:00 UTC (1790155800)
static uint32_t simulated_utc_time = 1790155800;

// -------------------------------------------------------------
// Helper: Print System & Memory Diagnostics
// -------------------------------------------------------------
static void print_system_diagnostics() {
    Serial.println("\n==================================================");
    Serial.println("  LiveEarth High-Performance Viewport Engine");
    Serial.println("  Device: M5Stack Stopwatch (CO5300 AMOLED 466x466)");
    Serial.printf("  Canvas Size: %dx%d (Offset: %d, %d)\n", 
                  GLOBE_CANVAS_SIZE, GLOBE_CANVAS_SIZE, 
                  GLOBE_CANVAS_OFFSET_X, GLOBE_CANVAS_OFFSET_Y);
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
    Serial.printf("Interactive Markers: %u loaded\n", (uint32_t)place_mgr.getCount());
    Serial.println("==================================================\n");
}

// -------------------------------------------------------------
// Round HUD Overlays (Rendered directly on M5.Display)
// -------------------------------------------------------------
static void draw_static_hud_markers() {
    // 四方微标 (N / S / W / E)
    M5.Display.setTextColor(COLOR_TEXT_MUTED, COLOR_BG);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(1);
    M5.Display.drawString("N", SCREEN_CENTER_X, SCREEN_CENTER_Y - SAFE_UI_RADIUS + 14);
    M5.Display.drawString("S", SCREEN_CENTER_X, SCREEN_CENTER_Y + SAFE_UI_RADIUS - 14);
    M5.Display.drawString("W", SCREEN_CENTER_X - SAFE_UI_RADIUS + 14, SCREEN_CENTER_Y);
    M5.Display.drawString("E", SCREEN_CENTER_X + SAFE_UI_RADIUS - 14, SCREEN_CENTER_Y);

    // 顶部主标题 (上移至 Y: 28，留出充足空间)
    M5.Display.setTextColor(COLOR_ACCENT_CYAN, COLOR_BG);
    M5.Display.setTextSize(2);
    M5.Display.drawString("LIVE EARTH", SCREEN_CENTER_X, 28);
}

static void update_dynamic_hud() {
    // 1. 顶部视界经纬度信息 (上移至 Y: 46)
    float center_lat = globe.getPitch() * RAD_TO_DEG;
    float center_lon = globe.getYaw() * RAD_TO_DEG;
    char view_buf[48];
    snprintf(view_buf, sizeof(view_buf), "VIEW: %4.1f%c, %5.1f%c",
             fabsf(center_lat), (center_lat >= 0 ? 'N' : 'S'),
             fabsf(center_lon), (center_lon >= 0 ? 'E' : 'W'));

    M5.Display.setTextColor(0xD69A, COLOR_BG);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(1);
    M5.Display.drawString(view_buf, SCREEN_CENTER_X, 46);

    // 2. 太阳直射位置 (上移至 Y: 58，与 Y:73 视口边缘完全隔离，彻底杜绝闪烁与遮挡)
    char sun_buf[48];
    float slat = globe.getSubsolarLat();
    float slon = globe.getSubsolarLon();
    snprintf(sun_buf, sizeof(sun_buf), "SUN:  %4.1f%c, %5.1f%c",
             fabsf(slat), (slat >= 0 ? 'N' : 'S'),
             fabsf(slon), (slon >= 0 ? 'E' : 'W'));
    M5.Display.setTextColor(0xFD64, COLOR_BG); // 暖金
    M5.Display.drawString(sun_buf, SCREEN_CENTER_X, 58);

    // 3. 底部状态栏 HUD: 当前聚焦/选中地点胶囊信息 (位于 Y: 398, 412, 426，在视口 393 之下)
    const PlaceLocation* sel_place = place_mgr.getSelectedPlace();
    if (sel_place) {
        char place_title[64];
        snprintf(place_title, sizeof(place_title), "%s (%s)", sel_place->name, sel_place->country);
        M5.Display.setTextColor(sel_place->theme_color, COLOR_BG);
        M5.Display.drawString(place_title, SCREEN_CENTER_X, 398);

        char place_detail[64];
        snprintf(place_detail, sizeof(place_detail), "%4.1f%c %5.1f%c | %s",
                 fabsf(sel_place->latitude), (sel_place->latitude >= 0 ? 'N' : 'S'),
                 fabsf(sel_place->longitude), (sel_place->longitude >= 0 ? 'E' : 'W'),
                 sel_place->category);
        M5.Display.setTextColor(0xA554, COLOR_BG);
        M5.Display.drawString(place_detail, SCREEN_CENTER_X, 412);
    }

    char perf_buf[64];
    snprintf(perf_buf, sizeof(perf_buf), "%4.1f FPS | DRAG ROTATE | TAP MARKER", perf.current_fps);
    M5.Display.setTextColor(COLOR_TEXT_MUTED, COLOR_BG);
    M5.Display.drawString(perf_buf, SCREEN_CENTER_X, 426);
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

    // 初始化地理地点管理器
    place_mgr.init();

    print_system_diagnostics();

    // 屏幕与背光设置
    M5.Display.setBrightness(180);
    M5.Display.setRotation(0);
    M5.Display.fillScreen(COLOR_BG);

    draw_static_hud_markers();

    // 在 PSRAM 中分配精简 320x320 视口画布 (节约 53% 显存与总线搬运开销)
    canvas.setColorDepth(16);
    canvas.setPsram(true);
    if (!canvas.createSprite(GLOBE_CANVAS_SIZE, GLOBE_CANVAS_SIZE)) {
        Serial.println("[ERROR] Failed to allocate 320x320 Canvas in PSRAM!");
    } else {
        Serial.println("[OK] 320x320 High-Performance Canvas created in PSRAM.");
    }

    // 初始化 3D 地球引擎 (以 320x320 画布为中心 160, 160)
    globe.init(GLOBE_CANVAS_CENTER, GLOBE_CANVAS_CENTER, GLOBE_DEFAULT_RADIUS);

    // 计算初次太阳直射点
    SunPosition init_sun = sun_calc.calculate(simulated_utc_time);
    globe.setSunPosition(init_sun.subsolar_lat, init_sun.subsolar_lon);

    perf.last_fps_update = millis();
    perf.last_frame_time = millis();
    perf.last_sun_update = millis();
    perf.last_hud_update = millis();
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

    // 1. 每 2 秒微调时间并更新太阳直射点
    if (now - perf.last_sun_update >= 2000) {
        perf.last_sun_update = now;
        simulated_utc_time += 2;
        SunPosition cur_sun = sun_calc.calculate(simulated_utc_time);
        globe.setSunPosition(cur_sun.subsolar_lat, cur_sun.subsolar_lon);
    }

    // 2. 物理按键处理：短按切换下一个地点，长按复位地球视角
    if (M5.BtnA.wasClicked()) {
        btn_click_count++;
        place_mgr.nextPlace();
        const PlaceLocation* p = place_mgr.getSelectedPlace();
        if (p) {
            Serial.printf("[BtnA Click] Switched to place: %s (%s)\n", p->name, p->country);
        }
    } else if (M5.BtnA.wasHold()) {
        Serial.println("[BtnA Hold] Resetting globe orientation to default.");
        globe.resetOrientation();
    }

    // 3. 触控手势处理 (拖拽旋转地球 + 点击轻触命中发光 Marker)
    auto detail = M5.Touch.getDetail();
    if (detail.isPressed()) {
        int16_t cur_x = detail.x;
        int16_t cur_y = detail.y;

        if (!touch_state.was_pressed) {
            touch_state.was_pressed = true;
            touch_state.is_dragging = false;
            touch_state.start_x = cur_x;
            touch_state.start_y = cur_y;
            touch_state.last_x = cur_x;
            touch_state.last_y = cur_y;
            touch_state.touch_start_ms = now;
        } else {
            float dx = (float)(cur_x - touch_state.last_x);
            float dy = (float)(cur_y - touch_state.last_y);

            // 位移累计判断是否进入持续拖拽
            float total_dx = (float)(cur_x - touch_state.start_x);
            float total_dy = (float)(cur_y - touch_state.start_y);
            if (!touch_state.is_dragging && (total_dx * total_dx + total_dy * total_dy > 36.0f)) {
                touch_state.is_dragging = true;
            }

            if (touch_state.is_dragging) {
                // 驱动 3D 地球旋转
                globe.handleDrag(dx, dy);
            }

            touch_state.last_x = cur_x;
            touch_state.last_y = cur_y;
        }
    } else {
        if (touch_state.was_pressed) {
            // 手指抬起，若为短促且位移轻微的点击，判定为 Tap 触控命中检测
            uint32_t press_duration = now - touch_state.touch_start_ms;
            float total_dx = (float)(touch_state.last_x - touch_state.start_x);
            float total_dy = (float)(touch_state.last_y - touch_state.start_y);
            float move_dist_sq = total_dx * total_dx + total_dy * total_dy;

            if (press_duration < 350 && move_dist_sq < 64.0f) {
                int hit = place_mgr.hitTest(touch_state.start_x, touch_state.start_y, globe, 26.0f);
                if (hit >= 0) {
                    place_mgr.setSelectedIndex(hit);
                    const PlaceLocation* hit_place = place_mgr.getSelectedPlace();
                    if (hit_place) {
                        Serial.printf("[Touch Hit] Selected: %s, %s (Lat: %.2f, Lon: %.2f)\n",
                                      hit_place->name, hit_place->country,
                                      hit_place->latitude, hit_place->longitude);
                    }
                }
            }

            touch_state.was_pressed = false;
            touch_state.is_dragging = false;
            touch_state.last_x = -1;
            touch_state.last_y = -1;
        }
    }

    // 4. 物理惯性与地点动画更新
    globe.update(dt_sec);
    place_mgr.update(now);

    // 5. 统计帧率与内存
    perf.frame_count++;
    if (now - perf.last_fps_update >= 500) {
        perf.current_fps = (perf.frame_count * 1000.0f) / (float)(now - perf.last_fps_update);
        perf.frame_count = 0;
        perf.last_fps_update = now;
        perf.free_heap_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        perf.free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    }

    // 6. 高速渲染管线 (锁定总线事务传输)
    M5.Display.startWrite();

    // 6.1 在 320x320 视口画布中渲染 3D 地球底图与 0度经纬锚点
    canvas.fillScreen(COLOR_BG);
    globe.render(canvas);

    // 6.2 渲染 Phase 3 & Phase 4: 地理发光 Markers 呼吸动效与瞄准框
    place_mgr.renderMarkers(canvas, globe);

    // 6.3 高速推送到屏幕中央 (73, 73)
    canvas.pushSprite(GLOBE_CANVAS_OFFSET_X, GLOBE_CANVAS_OFFSET_Y);

    // 6.4 定时刷新动态 HUD 文字 (每 80ms 刷新一次，零拖慢主渲染管线)
    if (now - perf.last_hud_update >= 80) {
        perf.last_hud_update = now;
        update_dynamic_hud();
    }

    M5.Display.endWrite();
}

