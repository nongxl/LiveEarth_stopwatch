#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <time.h>
#include "config.h"
#include "generated_data.h"
#include "globe.h"
#include "sun_calc.h"
#include "place_marker.h"
#include "place_detail_view.h"
#include "video_player_view.h"
#include "network_manager.h"
#include "snapshot_player.h"
#include "camera_drawer_view.h"
#include "wifi_config_view.h"
#include <LittleFS.h>
#include "camera_ota_sync.h"


// -------------------------------------------------------------
// Global Rendering & Engine State
// -------------------------------------------------------------
enum AppViewState {
    VIEW_GLOBE = 0,         // 1. 3D 地球全屏居中探索模式
    VIEW_PLACE_PREVIEW = 1, // 2. 地球下移避让 + 中央预览窗 + 上左右多维信息环绕
    VIEW_VIDEO_FULL = 2     // 3. 最大内接矩形全屏放大播放器 (406x228, 对角线 466px)
};

static AppViewState current_view_state = VIEW_GLOBE;
static M5Canvas canvas(&M5.Display);
static GlobeEngine globe;
static SunCalculator sun_calc;
// place_mgr is exported globally by place_marker.h
static PlaceDetailView preview_view;
static VideoPlayerView video_player;

struct TouchGestureState {
    int16_t last_x = -1;
    int16_t last_y = -1;
    int16_t start_x = -1;
    int16_t start_y = -1;
    bool is_dragging = false;
    bool was_pressed = false;
    uint32_t touch_start_ms = 0;
    uint32_t last_tap_ms = 0;
    int last_hit_idx = -1;

    // 缩放手势控制 (适配 CST816S 单点触控硬件)
    bool is_edge_zooming = false;
    bool is_double_tap_drag = false;
    uint32_t last_zoom_ui_ms = 0;

    // 双指多点触控（若硬件/仿真器支持）
    bool is_pinching = false;
    float last_pinch_dist = 0.0f;
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

// 开机仪式感全息渐进构建动画状态
static bool is_boot_anim = true;
static uint32_t boot_start_ms = 0;
static constexpr uint32_t BOOT_ANIM_DURATION = 2200; // 2.2 秒优雅渐进构建展开
static uint32_t wifi_connect_done_ms = 0;
static bool was_wifi_connected = false;

// 默认基准时间：2026年9月28日 UTC (若 NTP 同步失败时的后备值)
static uint32_t simulated_utc_time = 1727480000;
// 地理定位缓存
static float geo_lat = 0.0f;
static float geo_lon = 0.0f;
static bool geo_loc_fetched = false;

// -------------------------------------------------------------
// Helper: Print System & Memory Diagnostics
// -------------------------------------------------------------
static void print_system_diagnostics() {
    Serial.println("\n==================================================");
    Serial.println("  LiveEarth High-Performance Viewport Engine");
    Serial.println("  Device: M5Stack Stopwatch (CO5300 AMOLED 466x466)");
    Serial.printf("  Canvas Size: %dx%d (Offset: %d, %d)\n", 
                  GLOBE_CANVAS_WIDTH, GLOBE_CANVAS_HEIGHT, 
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
    M5.Display.setFont(&fonts::Font0);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(1);

    // 1. 顶部视界经纬度信息 (上移至 Y: 46)
    float center_lat = globe.getPitch() * RAD_TO_DEG;
    float center_lon = globe.getYaw() * RAD_TO_DEG;
    char view_buf[48];
    snprintf(view_buf, sizeof(view_buf), "VIEW: %4.1f%c, %5.1f%c",
             fabsf(center_lat), (center_lat >= 0 ? 'N' : 'S'),
             fabsf(center_lon), (center_lon >= 0 ? 'E' : 'W'));

    M5.Display.setTextColor(0xD69A, COLOR_BG);
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

    // 3. 底部状态栏 HUD: 先局部清屏彻底杜绝字串变短时的旧字重叠与残影
    M5.Display.fillRect(0, 393, SCREEN_WIDTH, SCREEN_HEIGHT - 393, COLOR_BG);

    const PlaceLocation* sel_place = place_mgr.getSelectedPlace();
    if (sel_place) {
        char place_title[64];
        snprintf(place_title, sizeof(place_title), "%s (%s)", sel_place->name, sel_place->country);
        M5.Display.setTextColor(sel_place->theme_color, COLOR_BG);
        M5.Display.drawString(place_title, SCREEN_CENTER_X, 400);

        char place_detail[64];
        snprintf(place_detail, sizeof(place_detail), "%4.1f%c %5.1f%c | %s",
                 fabsf(sel_place->latitude), (sel_place->latitude >= 0 ? 'N' : 'S'),
                 fabsf(sel_place->longitude), (sel_place->longitude >= 0 ? 'E' : 'W'),
                 sel_place->category);
        M5.Display.setTextColor(0xA554, COLOR_BG);
        M5.Display.drawString(place_detail, SCREEN_CENTER_X, 416);
    }

    // 4. 底部网络连接与系统状态 HUD (动态、炫酷、与赛博极客风格完美融合，单行紧凑自适应圆屏底部)
    uint32_t now = millis();
    bool is_net_ok = network_mgr.isConnected();
    if (is_net_ok && !was_wifi_connected) {
        was_wifi_connected = true;
        wifi_connect_done_ms = now; // 记录刚连上的时间点
    } else if (!is_net_ok) {
        was_wifi_connected = false;
    }

    if (network_mgr.getState() == WIFI_STATE_CONNECTING) {
        // [动态连接中] 居中流光能量脉冲
        int16_t bar_w = 110;
        int16_t bar_h = 2;
        int16_t bar_x = SCREEN_CENTER_X - bar_w / 2;
        int16_t bar_y = 430;
        M5.Display.fillRect(bar_x, bar_y, bar_w, bar_h, 0x18E3);

        float sweep = 0.5f + 0.5f * sinf(now * 0.009f);
        int16_t dot_x = bar_x + (int16_t)(sweep * (bar_w - 18));
        M5.Display.fillRect(dot_x, bar_y - 1, 18, bar_h + 2, COLOR_ACCENT_CYAN);

        const char* dots[] = {".  ", ".. ", "...", " .."};
        int d_idx = (now / 200) % 4;
        char net_buf[64];
        snprintf(net_buf, sizeof(net_buf), "LINKING: %s %s", network_mgr.getSSID(), dots[d_idx]);
        M5.Display.setTextColor(COLOR_ACCENT_CYAN, COLOR_BG);
        M5.Display.drawString(net_buf, SCREEN_CENTER_X, 442);

    } else if (is_net_ok && (now - wifi_connect_done_ms < 3000)) {
        // [连接成功建立瞬间高亮]
        float flash = 0.5f + 0.5f * sinf(now * 0.015f);
        uint16_t ok_col = (flash > 0.3f) ? 0x07E0 : 0xFFFF;
        char ok_buf[48];
        snprintf(ok_buf, sizeof(ok_buf), "* ONLINE // IP: %s *", network_mgr.getIP());
        M5.Display.setTextColor(ok_col, COLOR_BG);
        M5.Display.drawString(ok_buf, SCREEN_CENTER_X, 434);

    } else if (is_net_ok) {
        // [常规稳定在线状态] 紧凑合并单行，给圆弧边缘预留安全边距
        char status_buf[64];
        snprintf(status_buf, sizeof(status_buf), "WIFI %ddBm | %4.1f FPS", network_mgr.getRSSI(), perf.current_fps);
        M5.Display.setTextColor(COLOR_ACCENT_CYAN, COLOR_BG);
        M5.Display.drawString(status_buf, SCREEN_CENTER_X, 434);

    } else {
        // [未连接 / 离线存档模式]
        M5.Display.setTextColor(0xFDC0, COLOR_BG); // 暖金
        M5.Display.drawString("OFFLINE // TAP TO SETUP WI-FI", SCREEN_CENTER_X, 434);
    }
}

static void draw_zoom_edge_ui(M5GFX& display, uint32_t now) {
    if (current_view_state != VIEW_GLOBE) return;

    bool is_active = touch_state.is_edge_zooming || touch_state.is_double_tap_drag || (now - touch_state.last_zoom_ui_ms < 1200);

    if (!is_active) {
        // 静止态：右侧边缘低调科技拉手把柄 (X: 454~466, Y: 213~253)，稳态暗金属色，去除闪烁呼吸避免抢占视觉
        uint16_t h_col = 0x2354;

        display.fillRoundRect(SCREEN_WIDTH - 12, 213, 12, 40, 5, 0x0842);
        display.drawRoundRect(SCREEN_WIDTH - 12, 213, 12, 40, 5, h_col);

        display.setFont(&fonts::Font0);
        display.setTextDatum(middle_center);
        display.setTextSize(1);
        display.setTextColor(0xFFFF, 0x0842);
        display.drawString("+", SCREEN_WIDTH - 6, 224);
        display.drawString("-", SCREEN_WIDTH - 6, 240);
        return;
    }

    // 激活态：无级动态光轨 (底轨 330° ~ 30°，跨度整整 60 度)
    // 1. 底轨 (深邃科技背景)
    display.drawArc(SCREEN_CENTER_X, SCREEN_CENTER_Y, 226, 222, 330, 30, 0x1B55);

    // 两端极限刻度线：顶端 (-30° 即 330°) 与 底端 (+30°)
    int16_t t_kx1 = SCREEN_CENTER_X + (int16_t)(cosf(-30.0f * DEG_TO_RAD) * 220.0f);
    int16_t t_ky1 = SCREEN_CENTER_Y + (int16_t)(sinf(-30.0f * DEG_TO_RAD) * 220.0f);
    int16_t t_kx2 = SCREEN_CENTER_X + (int16_t)(cosf(-30.0f * DEG_TO_RAD) * 228.0f);
    int16_t t_ky2 = SCREEN_CENTER_Y + (int16_t)(sinf(-30.0f * DEG_TO_RAD) * 228.0f);
    display.drawLine(t_kx1, t_ky1, t_kx2, t_ky2, 0x7BEF);

    int16_t b_kx1 = SCREEN_CENTER_X + (int16_t)(cosf(30.0f * DEG_TO_RAD) * 220.0f);
    int16_t b_ky1 = SCREEN_CENTER_Y + (int16_t)(sinf(30.0f * DEG_TO_RAD) * 220.0f);
    int16_t b_kx2 = SCREEN_CENTER_X + (int16_t)(cosf(30.0f * DEG_TO_RAD) * 228.0f);
    int16_t b_ky2 = SCREEN_CENTER_Y + (int16_t)(sinf(30.0f * DEG_TO_RAD) * 228.0f);
    display.drawLine(b_kx1, b_ky1, b_kx2, b_ky2, 0x7BEF);

    // 2. 映射当前 zoom (GLOBE_ZOOM_MIN ~ GLOBE_ZOOM_MAX) 到精确角度
    float cur_z = globe.getZoom();
    float norm = (cur_z - GLOBE_ZOOM_MIN) / (GLOBE_ZOOM_MAX - GLOBE_ZOOM_MIN);
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;

    // 角度精确线性映射：norm=0 时 angle=+30°(底端)；norm=1 时 angle=-30°(顶端尽头)
    float angle_deg = 30.0f - norm * 60.0f;

    // 3. 动态发光填充弧线 (从底端 30° 向上饱满填充至当前 angle_deg)
    int32_t start_a = (int32_t)roundf(angle_deg < 0 ? angle_deg + 360.0f : angle_deg);
    if (start_a >= 360) start_a -= 360;
    display.drawArc(SCREEN_CENTER_X, SCREEN_CENTER_Y, 226, 222, start_a, 30, COLOR_ACCENT_CYAN);

    // 4. 当前游标星核 (到达极限时精确停靠在端点小刻度线上)
    float rad = angle_deg * DEG_TO_RAD;
    int16_t kx = SCREEN_CENTER_X + (int16_t)(cosf(rad) * 224.0f);
    int16_t ky = SCREEN_CENTER_Y + (int16_t)(sinf(rad) * 224.0f);

    display.fillCircle(kx, ky, 5, COLOR_ACCENT_CYAN);
    display.fillCircle(kx, ky, 2, 0xFFFF);
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

    // 挂载 LittleFS 本地存储并加载地点配置 (离线优先)
    if (!LittleFS.begin(true)) {
        Serial.println("[FS] Failed to mount LittleFS!");
    } else {
        Serial.println("[FS] LittleFS mounted successfully.");
    }

    // 初始化地理地点管理器、预览视图、全屏播放器与网络快照引擎
    place_mgr.init();
    if (!place_mgr.loadFromLittleFS("/places.json")) {
        Serial.printf("[Init] Using default ROM places (%u total).\n", (uint32_t)place_mgr.getCount());
    } else {
        Serial.printf("[Init] Successfully loaded %u dynamic places from LittleFS.\n", (uint32_t)place_mgr.getCount());
    }

    preview_view.init();
    video_player.init();
    snapshot_player.init();
    network_mgr.init();
    camera_drawer.init();

    print_system_diagnostics();

    // 屏幕与背光设置 (原 180 降低 20% 为 144)
    M5.Display.setBrightness(DEFAULT_DISPLAY_BRIGHTNESS);
    M5.Display.setRotation(0);
    M5.Display.setClipRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    M5.Display.fillScreen(COLOR_BG);

    draw_static_hud_markers();

    // 在 PSRAM 中分配 466x320 全宽视口画布 (彻底消除左右两侧垂直切线)
    canvas.setColorDepth(16);
    canvas.setPsram(true);
    if (!canvas.createSprite(GLOBE_CANVAS_WIDTH, GLOBE_CANVAS_HEIGHT)) {
        Serial.println("[ERROR] Failed to allocate 466x320 Canvas in PSRAM!");
    } else {
        Serial.println("[OK] 466x320 Full-Width High-Performance Canvas created in PSRAM.");
    }

    // 初始化 3D 地球引擎 (以 466x320 画布为中心 233, 160)
    globe.init(GLOBE_CANVAS_WIDTH / 2, GLOBE_CANVAS_HEIGHT / 2, GLOBE_DEFAULT_RADIUS);

    // 计算初次太阳直射点
    SunPosition init_sun = sun_calc.calculate(simulated_utc_time);
    globe.setSunPosition(init_sun.subsolar_lat, init_sun.subsolar_lon);

    perf.last_fps_update = millis();
    perf.last_frame_time = millis();
    perf.last_sun_update = millis();
    perf.last_hud_update = millis();

    // 初始化 Wi-Fi 配网与全键盘组件
    wifi_config_view.init();
    if (!network_mgr.hasSavedConfig()) {
        Serial.println("[Boot] No Wi-Fi credentials in NVS. Launching Wi-Fi config view.");
        wifi_config_view.open(false);
    } else {
        Serial.println("[Boot] Found saved Wi-Fi in NVS. Launching Globe View with Cinematic Progressive Boot.");
        // 不再调用 wifi_config_view.open(true) 霸屏！
        // network_mgr.init() 已经在后台静默异步连接，前台直接展示炫酷渐进地球仪与动态底部网络指示器！
    }

    // 立即启动开机全息渐进构建动画
    is_boot_anim = true;
    boot_start_ms = millis();

    // 立即渲染并推送第 0 毫秒全息微光第一帧，彻底杜绝任何上电黑屏！
    M5.Display.startWrite();
    canvas.fillScreen(COLOR_BG);
    globe.setCenterAndRadius(GLOBE_CANVAS_WIDTH / 2, GLOBE_CANVAS_HEIGHT / 2, GLOBE_DEFAULT_RADIUS * 0.35f);
    globe.render(canvas);
    canvas.pushSprite(GLOBE_CANVAS_OFFSET_X, GLOBE_CANVAS_OFFSET_Y);
    update_dynamic_hud();
    M5.Display.endWrite();
}


// -------------------------------------------------------------
// Helper: 状态转场函数
// -------------------------------------------------------------
static void transition_to_view(AppViewState new_state) {
    if (current_view_state == new_state) return;

    // 状态切换时首先安全重置显示裁剪区为整屏 (0, 0, 466, 466)，消除任何局部残留
    M5.Display.setClipRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    if (new_state == VIEW_GLOBE) {
        // 返回地球仪居中模式：停止快照拉取以节省功耗与带宽，恢复地球仪居中
        snapshot_player.stop();
        const PlaceLocation* p = place_mgr.getSelectedPlace();
        if (p) {
            globe.flyToLatLng(p->latitude, p->longitude, false, 500);
        } else {
            globe.setCenterAndRadius(GLOBE_CANVAS_WIDTH / 2, GLOBE_CANVAS_HEIGHT / 2, GLOBE_DEFAULT_RADIUS);
        }
        current_view_state = VIEW_GLOBE;

        M5.Display.startWrite();
        M5.Display.fillScreen(COLOR_BG);
        draw_static_hud_markers();
        M5.Display.endWrite();

        perf.last_hud_update = 0;
        Serial.println("[State] -> VIEW_GLOBE (Centered 3D Earth)");
    } else if (new_state == VIEW_PLACE_PREVIEW) {
        // 进入预览模式：启动平滑自转飞掠动画，自然旋转并触发实时快照拉取
        const PlaceLocation* p = place_mgr.getSelectedPlace();
        if (p) {
            globe.flyToLatLng(p->latitude, p->longitude, true, 650);
            snapshot_player.loadPlace(p);
        } else {
            globe.setCenterAndRadius(GLOBE_CANVAS_WIDTH / 2, 220, 210.0f);
        }
        current_view_state = VIEW_PLACE_PREVIEW;

        M5.Display.startWrite();
        M5.Display.fillScreen(COLOR_BG);
        M5.Display.endWrite();

        preview_view.requestRedraw();
        Serial.println("[State] -> VIEW_PLACE_PREVIEW (Smooth Fly-to & Real-time Snapshot Feed)");
    } else if (new_state == VIEW_VIDEO_FULL) {
        // 进入对角线466黄金内接放大播放模式
        current_view_state = VIEW_VIDEO_FULL;
        const PlaceLocation* p = place_mgr.getSelectedPlace();
        if (p) {
            snapshot_player.loadPlace(p);
        }

        M5.Display.startWrite();
        M5.Display.fillScreen(COLOR_BG);
        M5.Display.endWrite();

        video_player.requestRedraw();
        Serial.println("[State] -> VIEW_VIDEO_FULL (16:9 Max-Inscribed 406x228, Diagonal=466)");
    }
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

    // 网络状态机与快照轮询更新 (非阻塞后台维持)
    network_mgr.update(now);
    snapshot_player.update(now);

    // Wi-Fi 配网界面生命周期管理
    wifi_config_view.update(now);
    static bool was_wifi_cfg_active = false;
    if (wifi_config_view.isActive()) {
        was_wifi_cfg_active = true;
        auto detail = M5.Touch.getDetail(0);
        wifi_config_view.handleTouch(detail.isPressed(), detail.x, detail.y, now);
        
        static uint32_t last_cfg_render_ms = 0;
        if (now - last_cfg_render_ms >= 33) { // 30 FPS 平滑渲染
            last_cfg_render_ms = now;
            wifi_config_view.render(M5.Display);
        }
        delay(6);
        return;
    } else if (was_wifi_cfg_active) {

        // 刚从配网页面关闭进入地球仪，清屏并重绘 HUD 确保无任何残留
        was_wifi_cfg_active = false;
        M5.Display.fillScreen(COLOR_BG);
        draw_static_hud_markers();
        perf.last_hud_update = 0;
    }


    // 1. 每 2 秒更新太阳直射点
    if (now - perf.last_sun_update >= 2000) {
        perf.last_sun_update = now;

        if (network_mgr.isTimeSynced()) {
            // NTP 已同步：直接从系统 RTC 读取真实 UTC 时间戳
            time_t real_utc = 0;
            time(&real_utc);
            simulated_utc_time = (uint32_t)real_utc;
        } else {
            // 未同步时继续用模拟时间推进
            simulated_utc_time += 2;
        }

        SunPosition cur_sun = sun_calc.calculate(simulated_utc_time);
        globe.setSunPosition(cur_sun.subsolar_lat, cur_sun.subsolar_lon);
    }

    // 1b. 联网后一次性获取 IP 地理位置（用于定位时区与大致坐标）
    if (network_mgr.isConnected() && !geo_loc_fetched) {
        geo_loc_fetched = true;
        network_mgr.fetchGeoLocation(&geo_lat, &geo_lon, nullptr);
    }

    // 1c. 联网后触发后台静默机位 OTA 差分同步（国内加速优先，失败优雅降级）
    if (network_mgr.isConnected()) {
        camera_ota_sync.startSilentSync();
    }

    // =============================================================
    // 2. 物理实体按键交互矩阵 (Dragon Ball Radar Style)
    // KeyA: GPIO 2 (屏幕左上角)  |  KeyB: GPIO 1 (机身右侧)
    // =============================================================

    // 规则 1: 全局长按 KeyA 呼出 / 关闭摄像头列表
    if (M5.BtnA.wasHold()) {
        if (current_view_state != VIEW_GLOBE) {
            // 若当前处于预览或全屏模式，返回地球仪并呼出抽屉
            transition_to_view(VIEW_GLOBE);
            camera_drawer.open();
            camera_drawer.scrollToSelected(place_mgr.getSelectedIndex());
            Serial.println("[KeyA Hold] Exited to VIEW_GLOBE & opened camera drawer.");
        } else {
            camera_drawer.toggle();
            if (camera_drawer.isOpen() || camera_drawer.isVisible()) {
                camera_drawer.scrollToSelected(place_mgr.getSelectedIndex());
            }
            Serial.printf("[KeyA Hold] Camera drawer toggled: %s\n", camera_drawer.isOpen() ? "OPEN" : "CLOSED");
        }
    }
    // 规则 2: 在摄像头抽屉列表中且完全展开 (camera_drawer.isOpen())
    else if (camera_drawer.isOpen()) {
        // 2.1 短按 KeyA: 向下选择列表项 (循环往复，平滑滚动居中，地球仪同步旋转对准)
        if (M5.BtnA.wasClicked()) {
            int next_idx = camera_drawer.selectNextItem(place_mgr.getSelectedIndex());
            if (next_idx >= 0) {
                place_mgr.setSelectedIndex(next_idx);
                const PlaceLocation* p = place_mgr.getSelectedPlace();
                if (p) {
                    globe.flyToLatLng(p->latitude, p->longitude, false, 450);
                    Serial.printf("[KeyA Step] Selected list item #%d: %s (%s)\n", next_idx + 1, p->name, p->country);
                }
            }
        }
        // 2.2 长按 KeyB: 切换 全部摄像头 <-> LIVE 摄像头列表
        else if (M5.BtnB.wasHold()) {
            camera_drawer.toggleFilter();
            camera_drawer.scrollToSelected(place_mgr.getSelectedIndex());
            Serial.printf("[KeyB Hold] Drawer filter toggled -> %s\n", camera_drawer.isFilterLiveOnly() ? "LIVE ONLY" : "ALL");
        }
        // 2.3 短按 KeyB: 确定选择并聚焦此摄像头，打开预览窗口
        else if (M5.BtnB.wasClicked()) {
            camera_drawer.close();
            const PlaceLocation* p = place_mgr.getSelectedPlace();
            if (p) {
                transition_to_view(VIEW_PLACE_PREVIEW);
                Serial.printf("[KeyB Click] Confirmed camera -> VIEW_PLACE_PREVIEW: %s\n", p->name);
            }
        }
    }
    // 规则 3: 在摄像头聚焦预览界面 (VIEW_PLACE_PREVIEW)
    else if (current_view_state == VIEW_PLACE_PREVIEW) {
        // 3.1 短按 KeyA: 直接切换下一个 LIVE 摄像头（底部保留半个地球仪，自转飞跃无缝衔接）
        if (M5.BtnA.wasClicked()) {
            int cur_idx = place_mgr.getSelectedIndex();
            int next_live_idx = -1;
            for (size_t step = 1; step <= GLOBAL_PLACES_COUNT; step++) {
                int candidate = (cur_idx + step) % GLOBAL_PLACES_COUNT;
                if (GLOBAL_PLACES[candidate].is_online) {
                    next_live_idx = candidate;
                    break;
                }
            }
            if (next_live_idx >= 0) {
                place_mgr.setSelectedIndex(next_live_idx);
                const PlaceLocation* p = place_mgr.getSelectedPlace();
                if (p) {
                    globe.flyToLatLng(p->latitude, p->longitude, true, 550);
                    snapshot_player.loadPlace(p);
                    preview_view.requestRedraw();
                    Serial.printf("[KeyA Click] Preview Switch Next LIVE #%d: %s (%s)\n", next_live_idx + 1, p->name, p->country);
                }
            }
        }
        // 3.2 按下 KeyB: 全屏放大画面
        else if (M5.BtnB.wasClicked()) {
            transition_to_view(VIEW_VIDEO_FULL);
            Serial.println("[KeyB Click] Preview -> VIEW_VIDEO_FULL");
        }
    }
    // 规则 4: 在全屏放大画面 (VIEW_VIDEO_FULL)
    else if (current_view_state == VIEW_VIDEO_FULL) {
        // 4.1 短按 KeyA: 返回上一级 (摄像头预览窗口)
        if (M5.BtnA.wasClicked()) {
            transition_to_view(VIEW_PLACE_PREVIEW);
            Serial.println("[KeyA Click] Fullscreen Back -> VIEW_PLACE_PREVIEW");
        }
        // 4.2 按下 KeyB: 亦可返回预览
        else if (M5.BtnB.wasClicked()) {
            transition_to_view(VIEW_PLACE_PREVIEW);
            Serial.println("[KeyB Click] Fullscreen -> VIEW_PLACE_PREVIEW");
        }
    }
    // 规则 5: 在地球仪界面 (VIEW_GLOBE 且抽屉未展开)
    else if (current_view_state == VIEW_GLOBE) {
        // 5.1 短按 KeyA: 按顺序切换地球仪上有 LIVE 标识的摄像头 (龙珠雷达锁定感！)
        if (M5.BtnA.wasClicked()) {
            int cur_idx = place_mgr.getSelectedIndex();
            int next_live_idx = -1;
            for (size_t step = 1; step <= GLOBAL_PLACES_COUNT; step++) {
                int candidate = (cur_idx + step) % GLOBAL_PLACES_COUNT;
                if (GLOBAL_PLACES[candidate].is_online) {
                    next_live_idx = candidate;
                    break;
                }
            }
            if (next_live_idx >= 0) {
                place_mgr.setSelectedIndex(next_live_idx);
                const PlaceLocation* p = place_mgr.getSelectedPlace();
                if (p) {
                    globe.flyToLatLng(p->latitude, p->longitude, false, 550);
                    Serial.printf("[KeyA Click] Radar Lock Next LIVE #%d: %s (%s)\n", next_live_idx + 1, p->name, p->country);
                }
            }
        }
        // 5.2 短按 KeyB: 确定进入预览窗口
        else if (M5.BtnB.wasClicked()) {
            const PlaceLocation* p = place_mgr.getSelectedPlace();
            if (p) {
                globe.flyToLatLng(p->latitude, p->longitude, true, 650);
                snapshot_player.loadPlace(p);
                transition_to_view(VIEW_PLACE_PREVIEW);
                Serial.printf("[KeyB Click] Globe Selected Confirmed -> VIEW_PLACE_PREVIEW: %s\n", p->name);
            }
        }
    }

    // 3. 触控手势处理 (双指缩放 + 侧滑抽屉 + 旋转拖拽 + 点击联动)
    uint8_t touch_count = M5.Touch.getCount();
    if (touch_count >= 2) {
        // --- 双指多点触控：实时 Pinch-to-Zoom 平滑缩放地球仪 ---
        auto d0 = M5.Touch.getDetail(0);
        auto d1 = M5.Touch.getDetail(1);
        float pdx = (float)(d0.x - d1.x);
        float pdy = (float)(d0.y - d1.y);
        float pdist = sqrtf(pdx * pdx + pdy * pdy);

        if (!touch_state.is_pinching) {
            touch_state.is_pinching = true;
            touch_state.last_pinch_dist = pdist;
            touch_state.is_dragging = false;
        } else {
            float delta_dist = pdist - touch_state.last_pinch_dist;
            if (fabsf(delta_dist) > 0.8f) {
                globe.handlePinch(delta_dist * 0.0055f);
                touch_state.last_pinch_dist = pdist;
            }
        }
        touch_state.was_pressed = false; // 双指缩放时抑制单指点击
    } else {
        if (touch_state.is_pinching) {
            touch_state.is_pinching = false;
        }

        auto detail = M5.Touch.getDetail(0);
        int16_t cur_x = detail.x;
        int16_t cur_y = detail.y;

        // 在普通地球仪全屏模式下，优先交由摄像头抽屉组件处理侧滑手势或点击
        if (current_view_state == VIEW_GLOBE) {
            int drawer_hit = camera_drawer.handleTouch(detail.isPressed(), cur_x, cur_y, now);
            if (drawer_hit >= 0) {
                // 用户在抽屉列表中选定了一个地点！
                place_mgr.setSelectedIndex(drawer_hit);
                const PlaceLocation* p = place_mgr.getSelectedPlace();
                if (p) {
                    // 飞越旋转至所选经纬度，并平滑放大成焦点 (target_zoom = 1.35f)！
                    globe.flyToLatLng(p->latitude, p->longitude, false, 750, 1.35f);
                    snapshot_player.loadPlace(p);
                    // 用户在左侧列表选中后，直接自动打开该摄像头的实时预览大卡片！
                    transition_to_view(VIEW_PLACE_PREVIEW);
                    Serial.printf("[Drawer Selected] Focus, Zoom & Open Preview: %s (%s)\n", p->name, p->country);
                }
                touch_state.was_pressed = false;
                touch_state.is_dragging = false;
            }

            // 若抽屉正在展示或过渡，不响应地球仪底层的单指旋转拖拽
            if (camera_drawer.isVisible()) {
                touch_state.was_pressed = false;
                touch_state.is_dragging = false;
                goto skip_normal_touch;
            }
        }

        if (detail.isPressed()) {
            if (!touch_state.was_pressed) {
                touch_state.was_pressed = true;
                touch_state.is_dragging = false;
                touch_state.is_edge_zooming = false;
                touch_state.is_double_tap_drag = false;
                touch_state.start_x = cur_x;
                touch_state.start_y = cur_y;
                touch_state.last_x = cur_x;
                touch_state.last_y = cur_y;
                touch_state.touch_start_ms = now;

                if (current_view_state == VIEW_GLOBE) {
                    if (is_boot_anim) {
                        // 用户触控屏幕：瞬间跳过开机动画，立即恢复常规地球仪并点亮静态 HUD
                        is_boot_anim = false;
                        globe.setCenterAndRadius(GLOBE_CANVAS_WIDTH / 2, GLOBE_CANVAS_HEIGHT / 2, GLOBE_DEFAULT_RADIUS);
                        draw_static_hud_markers();
                    }

                    if (cur_x > 395) {
                        // 1. 命中右侧边缘区域：进入边缘滑轨缩放模式
                        touch_state.is_edge_zooming = true;
                        touch_state.last_zoom_ui_ms = now;
                    } else if (now - touch_state.last_tap_ms < 380) {
                        // 2. 快速双击后按住拖动 (Double-tap & Drag)：单手平滑缩放
                        touch_state.is_double_tap_drag = true;
                        touch_state.last_zoom_ui_ms = now;
                    }
                }
            } else {
                float dx = (float)(cur_x - touch_state.last_x);
                float dy = (float)(cur_y - touch_state.last_y);
                float total_dx = (float)(cur_x - touch_state.start_x);
                float total_dy = (float)(cur_y - touch_state.start_y);

                if (touch_state.is_edge_zooming || touch_state.is_double_tap_drag) {
                    // 垂直滑动连续无级缩放地球仪 (向上推放大，向下拉缩小，匹配 4.0x 超高无级范围)
                    if (fabsf(dy) > 0.6f) {
                        globe.handlePinch(-dy * 0.0135f);
                        touch_state.last_zoom_ui_ms = now;
                    }
                } else if (current_view_state == VIEW_GLOBE || (current_view_state == VIEW_PLACE_PREVIEW && cur_y > 270)) {
                    // 在地球仪模式或预览模式下的下半区地球仪拖拽旋转
                    if (!touch_state.is_dragging && (total_dx * total_dx + total_dy * total_dy > 36.0f)) {
                        touch_state.is_dragging = true;
                    }
                    if (touch_state.is_dragging) {
                        globe.handleDrag(dx, dy);
                    }
                }

                touch_state.last_x = cur_x;
                touch_state.last_y = cur_y;
            }
        } else {
            if (touch_state.was_pressed) {
                uint32_t press_duration = now - touch_state.touch_start_ms;
                float total_dx = (float)(touch_state.last_x - touch_state.start_x);
                float total_dy = (float)(touch_state.last_y - touch_state.start_y);
                float move_dist_sq = total_dx * total_dx + total_dy * total_dy;

                bool was_zooming = touch_state.is_edge_zooming || touch_state.is_double_tap_drag;
                touch_state.is_edge_zooming = false;
                touch_state.is_double_tap_drag = false;

                if (!was_zooming) {
                    if (current_view_state == VIEW_GLOBE) {
                        // 1. 地球居中模式：点击 Marker
                        if (press_duration < 350 && move_dist_sq < 64.0f) {
                            int hit = place_mgr.hitTest(touch_state.start_x, touch_state.start_y, globe, 28.0f);
                            uint32_t time_since_last_tap = now - touch_state.last_tap_ms;
                            touch_state.last_tap_ms = now;

                            if (hit >= 0) {
                                if (hit == place_mgr.getSelectedIndex() || (time_since_last_tap < 450 && hit == touch_state.last_hit_idx)) {
                                    // 第二次点击该地点：地球仪向下方避让，进入 VIEW_PLACE_PREVIEW！
                                    transition_to_view(VIEW_PLACE_PREVIEW);
                                } else {
                                    place_mgr.setSelectedIndex(hit);
                                    touch_state.last_hit_idx = hit;
                                    const PlaceLocation* p = place_mgr.getSelectedPlace();
                                    if (p) {
                                        Serial.printf("[Touch Hit] Focus: %s (%s)\n", p->name, p->country);
                                    }
                                }
                            } else if (touch_state.start_y >= 418 && abs(touch_state.start_x - SCREEN_CENTER_X) < 140) {
                                // 点击底部 Wi-Fi 状态栏：直接进入 Wi-Fi 配置界面
                                wifi_config_view.open(false);
                            } else if (touch_state.start_y >= 390 && abs(touch_state.start_x - SCREEN_CENTER_X) < 140) {
                                // 点击底部卡片亦可进入预览模式
                                transition_to_view(VIEW_PLACE_PREVIEW);

                            } else if (time_since_last_tap < 380) {
                                // 双击地球仪空白处：快速在 1.0x 全景与 2.5x 超清大洲特写之间平滑切换！
                                float current_z = globe.getZoom();
                                float target_z = (current_z > 1.6f) ? 1.0f : 2.5f;
                                globe.setZoom(target_z);
                                touch_state.last_zoom_ui_ms = now;
                                Serial.printf("[Double Tap Blank] Quick Zoom toggle -> %.2fx\n", target_z);
                            }
                        }
                    } else if (current_view_state == VIEW_PLACE_PREVIEW) {
                // 2. 预览模式：点击中等窗口放大播放，或滑动切地点，或点击地球返回
                if (fabsf(total_dx) > 40.0f && fabsf(total_dx) > fabsf(total_dy) * 1.4f) {
                    // 水平滑动切换地点 (Swipe Left: 下一处, Swipe Right: 上一处)
                    if (total_dx < 0) {
                        place_mgr.nextPlace();
                    } else {
                        place_mgr.prevPlace();
                    }
                    const PlaceLocation* np = place_mgr.getSelectedPlace();
                    if (np) {
                        globe.flyToLatLng(np->latitude, np->longitude, true, 550);
                        snapshot_player.loadPlace(np);
                    }
                    preview_view.requestRedraw();
                    Serial.printf("[Preview Swipe] Now at: %s\n", place_mgr.getSelectedPlace()->name);
                } else if (press_duration < 350 && move_dist_sq < 64.0f) {
                    // 点击判定
                    if (PlaceDetailView::isInsidePreviewWindow(touch_state.start_x, touch_state.start_y)) {
                        // 第三次点击：中等视频窗口点击放大播放 (VIEW_VIDEO_FULL)！
                        transition_to_view(VIEW_VIDEO_FULL);
                    } else if (touch_state.start_y > 275 || touch_state.start_y <= 60) {
                        // 点击下方避让地球仪区域或顶部微标：返回居中全景地球
                        transition_to_view(VIEW_GLOBE);
                    }
                }
            } else if (current_view_state == VIEW_VIDEO_FULL) {
                // 3. 放大播放模式：点击任意位置缩回预览模式
                if (press_duration < 350 && move_dist_sq < 64.0f) {
                    transition_to_view(VIEW_PLACE_PREVIEW);
                }
            }
            }

            touch_state.was_pressed = false;
            touch_state.is_dragging = false;
            touch_state.last_x = -1;
            touch_state.last_y = -1;
        }
    }
    skip_normal_touch: ;
    }

    // 4. 视图分支渲染管线
    if (current_view_state == VIEW_GLOBE) {
        // -------------------------------------------------------------
        // 4.1 VIEW_GLOBE: 3D 地球全屏居中探索模式
        // -------------------------------------------------------------
        globe.update(dt_sec);
        place_mgr.update(now);

        perf.frame_count++;
        if (now - perf.last_fps_update >= 500) {
            perf.current_fps = (perf.frame_count * 1000.0f) / (float)(now - perf.last_fps_update);
            perf.frame_count = 0;
            perf.last_fps_update = now;
            perf.free_heap_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
            perf.free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        }

        // 开机全息渐进构建动画状态驱动
        if (is_boot_anim) {
            uint32_t elapsed = now - boot_start_ms;
            if (elapsed >= BOOT_ANIM_DURATION) {
                is_boot_anim = false;
                globe.setCenterAndRadius(GLOBE_CANVAS_WIDTH / 2, GLOBE_CANVAS_HEIGHT / 2, GLOBE_DEFAULT_RADIUS);
                // 开机结束瞬间全面彻底擦除屏幕顶部和底部所有开机弧线与能量环残留！
                M5.Display.fillRect(0, 0, SCREEN_WIDTH, GLOBE_CANVAS_OFFSET_Y, COLOR_BG);
                M5.Display.fillRect(0, GLOBE_CANVAS_OFFSET_Y + GLOBE_CANVAS_HEIGHT,
                                    SCREEN_WIDTH, SCREEN_HEIGHT - (GLOBE_CANVAS_OFFSET_Y + GLOBE_CANVAS_HEIGHT), COLOR_BG);
                draw_static_hud_markers();
                update_dynamic_hud();
            } else {
                float boot_t = (float)elapsed / (float)BOOT_ANIM_DURATION;
                // 1. 地球仪平滑弹性展开 (从 0.35x 放大至 1.0x)
                float ease = 1.0f - powf(1.0f - boot_t, 3.0f);
                float cur_r = GLOBE_DEFAULT_RADIUS * (0.35f + 0.65f * ease);
                globe.setCenterAndRadius(GLOBE_CANVAS_WIDTH / 2, GLOBE_CANVAS_HEIGHT / 2, cur_r);
            }
        }

        M5.Display.startWrite();
        canvas.fillScreen(COLOR_BG);
        globe.render(canvas);
        place_mgr.renderMarkers(canvas, globe);
        canvas.pushSprite(GLOBE_CANVAS_OFFSET_X, GLOBE_CANVAS_OFFSET_Y);

        // 若在开机动画期间，叠加绘制动态外圈展开弧线与能量微光环
        if (is_boot_anim) {
            uint32_t elapsed = now - boot_start_ms;
            float boot_t = (float)elapsed / (float)BOOT_ANIM_DURATION;
            if (boot_t > 1.0f) boot_t = 1.0f;

            // 1. 双向雷达扫掠弧线：前 65% 展开，后 35% 平滑收拢回顶端消失，不留任何视觉负担
            int sweep_deg = 0;
            if (boot_t <= 0.65f) {
                float open_p = boot_t / 0.65f;
                sweep_deg = (int)(open_p * 180.0f);
            } else {
                float close_p = (boot_t - 0.65f) / 0.35f;
                sweep_deg = (int)((1.0f - close_p) * 180.0f);
                // 收起时以背景色清除非视口区外弧轨迹
                M5.Display.drawArc(SCREEN_CENTER_X, SCREEN_CENTER_Y, 228, 222, 0, 360, COLOR_BG);
            }
            if (sweep_deg > 0) {
                int start_a = 270 - sweep_deg;
                if (start_a < 0) start_a += 360;
                int end_a = (270 + sweep_deg) % 360;
                M5.Display.drawArc(SCREEN_CENTER_X, SCREEN_CENTER_Y, 226, 224, start_a, end_a, COLOR_ACCENT_CYAN);
            }

            // 2. 向外扩散的微光全息引力能量环
            if (boot_t > 0.15f && boot_t < 0.65f) {
                float ring_p = (boot_t - 0.15f) / 0.50f;
                float cur_r = globe.getRadius();
                int ring_r = (int)(cur_r * (0.9f + ring_p * 0.65f));
                uint16_t ring_col = (ring_p < 0.6f) ? 0x1B55 : 0x0842;
                M5.Display.drawCircle(SCREEN_CENTER_X, SCREEN_CENTER_Y, ring_r, ring_col);
            }
        }

        if (now - perf.last_hud_update >= 80) {
            perf.last_hud_update = now;
            update_dynamic_hud();
        }

        // 渲染屏幕左侧滑出式摄像头抽屉列表或左边缘呼出把柄
        bool was_drawer_visible = camera_drawer.isVisible();
        camera_drawer.update(now);
        if (camera_drawer.isVisible()) {
            camera_drawer.render(M5.Display, place_mgr);
        } else {
            if (was_drawer_visible) {
                // 抽屉刚刚完全闭合收起：全量清除顶部和底部非视口区域的所有残影，恢复常规地球仪完整静态 HUD！
                M5.Display.fillRect(0, 0, SCREEN_WIDTH, GLOBE_CANVAS_OFFSET_Y, COLOR_BG);
                M5.Display.fillRect(0, GLOBE_CANVAS_OFFSET_Y + GLOBE_CANVAS_HEIGHT,
                                    SCREEN_WIDTH, SCREEN_HEIGHT - (GLOBE_CANVAS_OFFSET_Y + GLOBE_CANVAS_HEIGHT), COLOR_BG);
                draw_static_hud_markers();
                update_dynamic_hud();
            }
            camera_drawer.renderEdgeHandle(M5.Display);
        }

        // 渲染屏幕右侧边缘滑轨缩放反馈与把柄 (适配 CST816S 单点触控硬件)
        draw_zoom_edge_ui(M5.Display, now);

        M5.Display.endWrite();

    } else if (current_view_state == VIEW_PLACE_PREVIEW) {
        // -------------------------------------------------------------
        // 4.2 VIEW_PLACE_PREVIEW: 地球仪下移避让 + 中央预览窗 + 上左右多维信息
        // -------------------------------------------------------------
        globe.update(dt_sec);
        place_mgr.update(now);
        preview_view.update(now);

        M5.Display.startWrite();

        // 1. 渲染上、左、右多维信息与中央中等视频窗口
        preview_view.render(M5.Display, place_mgr.getSelectedPlace());

        // 2. 渲染下方全宽 466 的大幅避让与放大的 3D 地球仪 (推送到 X: 0, Y: 260)
        // 关键防护：将硬件绘制剪裁区严格限定在 Y: 260 ~ 466 (高度 206) 内，彻底杜绝 320 高度画布打爆 AMOLED 显存！
        M5.Display.setClipRect(0, 260, SCREEN_WIDTH, SCREEN_HEIGHT - 260);
        canvas.fillScreen(COLOR_BG);
        globe.render(canvas);
        place_mgr.renderMarkers(canvas, globe);
        canvas.pushSprite(0, 260);
        M5.Display.setClipRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT); // 恢复全屏裁剪区

        M5.Display.endWrite();

    } else if (current_view_state == VIEW_VIDEO_FULL) {
        // -------------------------------------------------------------
        // 4.3 VIEW_VIDEO_FULL: 16:9 最大内接矩形全屏放大播放器 (406x228, 对角线 466px)
        // -------------------------------------------------------------
        video_player.update(now);

        M5.Display.setClipRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        M5.Display.startWrite();
        video_player.render(M5.Display, place_mgr.getSelectedPlace());
        M5.Display.endWrite();
        M5.Display.setAddrWindow(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    }
}

