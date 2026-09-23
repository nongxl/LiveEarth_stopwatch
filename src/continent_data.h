#pragma once

#include <Arduino.h>

// -------------------------------------------------------------
// LiveEarth Compact Global Coastlines & Continental Polylines
// Stored in Flash (PROGMEM) to ensure 0-byte SRAM overhead.
// Coordinates are stored as float pairs [lat, lon].
// -------------------------------------------------------------

struct CoastlinePolyline {
    const float (*points)[2];
    uint16_t count;
    bool closed;
};

// 1. 欧亚大陆与非洲主要骨干轮廓 (Eurasia & Africa)
static const float EURASIA_AFRICA_POLY[][2] PROGMEM = {
    // 欧洲大西洋与北海沿岸
    {71.0f, 28.0f}, {62.0f, 5.0f}, {58.0f, 11.0f}, {54.0f, 9.0f}, {50.0f, 1.0f}, 
    {48.0f, -4.5f}, {44.0f, -1.0f}, {43.5f, -9.0f}, {37.0f, -9.0f}, {36.0f, -5.5f},
    // 直布罗陀进入地中海
    {36.0f, -5.3f}, {41.0f, 2.0f}, {43.5f, 7.0f}, {41.0f, 16.0f}, {38.0f, 24.0f},
    {41.0f, 29.0f}, {44.5f, 34.0f}, {41.0f, 41.5f}, {36.0f, 36.0f}, {31.5f, 34.5f},
    // 非洲东北红海沿岸
    {28.0f, 34.0f}, {22.0f, 37.0f}, {12.0f, 43.5f},
    // 索马里半岛
    {11.5f, 51.0f}, {2.0f, 45.0f},
    // 非洲东海岸至好望角
    {-4.0f, 39.5f}, {-15.0f, 40.5f}, {-26.0f, 33.0f}, {-34.5f, 20.0f},
    // 好望角至西非几内亚湾
    {-34.0f, 18.5f}, {-22.5f, 14.5f}, {-5.0f, 12.0f}, {4.5f, 8.5f}, {5.0f, -1.0f},
    // 西非突起部
    {4.5f, -7.5f}, {14.5f, -17.5f}, {21.0f, -17.0f}, {33.0f, -9.0f}, {36.0f, -5.5f},
    // 北非地中海
    {37.0f, 10.0f}, {32.0f, 15.0f}, {31.5f, 25.0f}, {31.0f, 32.0f}
};

// 2. 亚洲南部、东部与北部沿海 (Asia Coastal Rim)
static const float ASIA_RIM_POLY[][2] PROGMEM = {
    {12.0f, 43.5f}, {15.0f, 53.0f}, {24.0f, 58.0f}, {25.0f, 62.0f}, {24.5f, 67.5f},
    // 印度次大陆
    {21.0f, 72.5f}, {13.0f, 74.5f}, {8.0f, 77.5f}, {13.0f, 80.0f}, {21.5f, 87.0f},
    // 东南亚
    {21.0f, 92.0f}, {16.0f, 96.0f}, {10.0f, 98.5f}, {1.3f, 103.8f}, {8.0f, 104.5f},
    {13.0f, 100.5f}, {10.5f, 107.5f}, {16.0f, 108.0f}, {21.5f, 108.5f},
    // 中国沿海
    {22.5f, 114.0f}, {24.5f, 118.5f}, {30.0f, 122.0f}, {35.0f, 119.5f}, {39.0f, 121.5f},
    {38.0f, 125.0f}, {34.5f, 127.0f}, {37.5f, 129.5f}, {40.0f, 128.5f},
    // 俄罗斯远东与西伯利亚北冰洋沿岸
    {43.0f, 132.0f}, {50.0f, 140.0f}, {53.5f, 141.5f}, {59.5f, 150.5f}, {51.0f, 156.5f},
    {56.0f, 163.0f}, {65.0f, 171.0f}, {66.0f, -169.5f}, {70.0f, 179.0f}, {73.0f, 140.0f},
    {77.5f, 105.0f}, {73.0f, 70.0f}, {69.0f, 60.0f}, {71.0f, 28.0f}
};

// 3. 日本列岛主要轮廓 (Japan Archipelago)
static const float JAPAN_POLY[][2] PROGMEM = {
    {31.0f, 130.5f}, {33.5f, 130.0f}, {35.5f, 135.5f}, {35.5f, 140.0f},
    {40.5f, 142.0f}, {45.5f, 142.0f}, {43.0f, 145.5f}, {41.5f, 140.5f},
    {38.0f, 138.5f}, {34.5f, 132.0f}, {31.0f, 130.5f}
};

// 4. 北美洲大陆 (North America)
static const float NORTH_AMERICA_POLY[][2] PROGMEM = {
    {7.0f, -77.5f}, {8.5f, -83.5f}, {15.5f, -88.0f}, {21.0f, -87.0f}, {19.0f, -96.0f},
    {26.0f, -97.0f}, {29.5f, -95.0f}, {29.0f, -89.0f}, {30.0f, -84.0f}, {25.0f, -80.5f},
    {32.0f, -81.0f}, {35.5f, -75.5f}, {41.0f, -72.0f}, {44.5f, -64.0f}, {47.0f, -53.0f},
    {51.5f, -56.0f}, {58.0f, -62.5f}, {62.0f, -75.0f}, {55.0f, -82.0f}, {51.0f, -80.0f},
    {58.0f, -94.0f}, {64.0f, -88.0f}, {70.0f, -95.0f}, {71.0f, -128.0f}, {71.5f, -156.5f},
    {65.5f, -168.0f}, {59.0f, -160.0f}, {55.0f, -163.0f}, {58.5f, -152.0f}, {60.0f, -140.0f},
    {54.0f, -133.0f}, {49.0f, -125.0f}, {38.0f, -123.0f}, {32.5f, -117.0f}, {23.0f, -110.0f},
    {28.0f, -112.0f}, {23.0f, -106.0f}, {16.0f, -98.0f}, {14.0f, -92.0f}, {8.5f, -83.5f},
    {7.0f, -77.5f}
};

// 5. 南美洲大陆 (South America)
static const float SOUTH_AMERICA_POLY[][2] PROGMEM = {
    {12.0f, -72.0f}, {10.5f, -62.0f}, {6.0f, -55.0f}, {-0.5f, -50.0f}, {-5.5f, -35.5f},
    {-13.0f, -38.5f}, {-23.0f, -42.0f}, {-33.0f, -53.0f}, {-40.0f, -62.0f}, {-53.0f, -68.0f},
    {-55.0f, -66.5f}, {-50.0f, -75.0f}, {-41.0f, -73.5f}, {-18.0f, -70.5f}, {-5.0f, -81.0f},
    {2.0f, -79.0f}, {8.5f, -77.0f}, {12.0f, -72.0f}
};

// 6. 澳大利亚大陆 (Australia)
static const float AUSTRALIA_POLY[][2] PROGMEM = {
    {-11.0f, 142.5f}, {-15.0f, 145.0f}, {-25.0f, 153.0f}, {-37.5f, 150.0f},
    {-38.5f, 145.0f}, {-35.5f, 138.5f}, {-32.0f, 132.5f}, {-34.0f, 123.0f},
    {-34.5f, 115.0f}, {-22.0f, 114.0f}, {-16.5f, 123.0f}, {-12.5f, 131.0f},
    {-12.0f, 136.5f}, {-16.5f, 139.0f}, {-11.0f, 142.5f}
};

// 7. 格陵兰岛 (Greenland)
static const float GREENLAND_POLY[][2] PROGMEM = {
    {60.0f, -43.0f}, {65.5f, -53.0f}, {76.0f, -68.0f}, {83.0f, -30.0f},
    {77.0f, -18.0f}, {70.0f, -22.0f}, {65.0f, -37.0f}, {60.0f, -43.0f}
};

// 8. 南极洲主要沿海骨架 (Antarctica)
static const float ANTARCTICA_POLY[][2] PROGMEM = {
    {-63.5f, -57.0f}, {-68.0f, -67.0f}, {-73.0f, -100.0f}, {-77.0f, -150.0f},
    {-71.0f, 170.0f}, {-66.0f, 140.0f}, {-66.0f, 95.0f}, {-69.0f, 70.0f},
    {-69.0f, 40.0f}, {-70.0f, 0.0f}, {-74.0f, -30.0f}, {-63.5f, -57.0f}
};

// 汇总大陆折线列表
static const CoastlinePolyline WORLD_COASTLINES[] = {
    {EURASIA_AFRICA_POLY, sizeof(EURASIA_AFRICA_POLY) / sizeof(EURASIA_AFRICA_POLY[0]), false},
    {ASIA_RIM_POLY, sizeof(ASIA_RIM_POLY) / sizeof(ASIA_RIM_POLY[0]), false},
    {JAPAN_POLY, sizeof(JAPAN_POLY) / sizeof(JAPAN_POLY[0]), true},
    {NORTH_AMERICA_POLY, sizeof(NORTH_AMERICA_POLY) / sizeof(NORTH_AMERICA_POLY[0]), true},
    {SOUTH_AMERICA_POLY, sizeof(SOUTH_AMERICA_POLY) / sizeof(SOUTH_AMERICA_POLY[0]), true},
    {AUSTRALIA_POLY, sizeof(AUSTRALIA_POLY) / sizeof(AUSTRALIA_POLY[0]), true},
    {GREENLAND_POLY, sizeof(GREENLAND_POLY) / sizeof(GREENLAND_POLY[0]), true},
    {ANTARCTICA_POLY, sizeof(ANTARCTICA_POLY) / sizeof(ANTARCTICA_POLY[0]), true}
};

static const size_t WORLD_COASTLINES_COUNT = sizeof(WORLD_COASTLINES) / sizeof(WORLD_COASTLINES[0]);
