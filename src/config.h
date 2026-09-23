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

// Globe Engine Viewport Geometry (320x320 Focused Canvas)
#define GLOBE_CANVAS_SIZE    320
#define GLOBE_CANVAS_OFFSET_X ((SCREEN_WIDTH - GLOBE_CANVAS_SIZE) / 2)  // 73
#define GLOBE_CANVAS_OFFSET_Y ((SCREEN_HEIGHT - GLOBE_CANVAS_SIZE) / 2) // 73
#define GLOBE_CANVAS_CENTER  (GLOBE_CANVAS_SIZE / 2)                   // 160
#define GLOBE_DEFAULT_RADIUS 152

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
