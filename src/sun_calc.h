#pragma once

#include <stdint.h>
#include <Arduino.h>

// -------------------------------------------------------------
// LiveEarth Astronomical Sun Position & Day/Night Calculator
// Computes subsolar point (lat, lon) and surface illumination
// Based on Jean Meeus Astronomical Algorithms
// -------------------------------------------------------------

struct SunPosition {
    float subsolar_lat;  // 太阳直射点纬度 (赤纬 Declination, 度: [-23.45°, +23.45°])
    float subsolar_lon;  // 太阳直射点经度 (度: [-180°, +180°])
    float gmst_deg;      // 格林威治平恒星时 (度)
};

class SunCalculator {
public:
    SunCalculator();

    // 根据 UTC Unix 时间戳计算当前太阳直射点
    SunPosition calculate(uint32_t unix_utc_sec);

    // 计算指定地理坐标 (lat, lon) 与太阳直射点的夹角余弦值
    // 返回值 > 0.05 为白昼, -0.05 ~ 0.05 为晨昏分界, < -0.05 为黑夜
    static float getCosSolarAngle(float lat_deg, float lon_deg, float sub_lat_deg, float sub_lon_deg);

    // 计算光照系数 (0.0: 深夜, 0.5: 晨昏, 1.0: 正午直射)
    static float getIllumination(float cos_dist);
};
