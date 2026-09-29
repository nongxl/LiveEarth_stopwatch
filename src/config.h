#pragma once

#include <Arduino.h>

// -------------------------------------------------------------
// LiveEarth Hardware & UI Configuration
// Target Hardware: M5Stack Stopwatch (ESP32-S3 + 466x466 Round AMOLED)
// -------------------------------------------------------------

// Display Geometry (1.75" Round AMOLED CO5300)
#define SCREEN_WIDTH         466
#define SCREEN_HEIGHT        466
#define SCREEN_CENTER_X      (SCREEN_WIDTH / 2)   // 233
#define SCREEN_CENTER_Y      (SCREEN_HEIGHT / 2)  // 233
#define SCREEN_RADIUS        233
#define SAFE_UI_RADIUS       215                  // 圆形安全可视边界内径
#define DEFAULT_DISPLAY_BRIGHTNESS 144           // 默认屏幕亮度 (0~255)，原 180 降低 20% 为 144

// Globe Engine Viewport Geometry (466x320 Full-Width Precision Canvas)
#define GLOBE_CANVAS_WIDTH   466
#define GLOBE_CANVAS_HEIGHT  320
#define GLOBE_CANVAS_OFFSET_X 0
#define GLOBE_CANVAS_OFFSET_Y 73
#define GLOBE_DEFAULT_RADIUS 152
#define GLOBE_ZOOM_MIN       0.60f
#define GLOBE_ZOOM_MAX       4.00f   // 支持超高放大倍率，密集摄像头区域轻松点选

// Color Palette (Dark Mode Aesthetic)
#define COLOR_BG             0x0821   // 深邃星空黑 (#080811)
#define COLOR_EARTH_OCEAN    0x0989   // 幽深大洋蓝 (#0b192e)
#define COLOR_EARTH_GRID     0x1A4F   // 细微经纬网格 (#1a334d)
#define COLOR_ACCENT_CYAN    0x07FF   // 赛博电光青 (#00ffff)
#define COLOR_ACCENT_GOLD    0xFEA0   // 暖金发光标记 (#ffcc00)
#define COLOR_TEXT_MUTED     0x7BEF   // 次要文本灰 (#7b7b7b)
#define COLOR_CARD_BG        0x10A2   // 半透明卡片底色
#define COLOR_SUCCESS        0x2668   // 状态正常绿

// System Monitor Settings
#define SERIAL_BAUD_RATE     115200
#define STATUS_UPDATE_INTERVAL_MS 1000

// Wi-Fi & Network Configuration (Dynamically saved to NVS)
#define WIFI_CONNECT_TIMEOUT_MS 9000
#define WIFI_RETRY_INTERVAL_MS  20000


// Snapshot Stream Engine Settings
#define SNAPSHOT_REFRESH_INTERVAL_MS 15000  // 15秒定时轮询最新现场动态帧（符合各大景区与天文台监控快照真实更新周期，彻底避免被控频封禁）
#define SNAPSHOT_HTTP_TIMEOUT_MS     12000  // 12秒超时保护（适应 SSL 握手与大图下载）
#define SNAPSHOT_MAX_BUF_SIZE        (512 * 1024) // 512KB PSRAM 接收缓冲区，彻底杜绝 300KB+ 高清大图（如富士山）截断解码失败

