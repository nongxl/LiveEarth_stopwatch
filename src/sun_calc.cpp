#include "sun_calc.h"
#include <math.h>

#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.017453292519943295769236907684886f
#endif
#ifndef RAD_TO_DEG
#define RAD_TO_DEG 57.295779513082320876798154814105f
#endif

SunCalculator::SunCalculator() {}

SunPosition SunCalculator::calculate(uint32_t unix_utc_sec) {
    SunPosition pos;

    // 1. 儒略日与儒略世纪数 (J2000.0 起算)
    double jd = (double)unix_utc_sec / 86400.0 + 2440587.5;
    double jc = (jd - 2451545.0) / 36525.0;

    // 2. 太阳几何平黄经 L0 (度)
    double l0 = fmod(280.46646 + jc * (36000.76983 + jc * 0.0003032), 360.0);
    if (l0 < 0) l0 += 360.0;

    // 3. 太阳平近点角 M (度)
    double m = fmod(357.52911 + jc * (35999.05029 - 0.0001537 * jc), 360.0);
    if (m < 0) m += 360.0;
    double m_rad = m * DEG_TO_RAD;

    // 4. 太阳中心差 C (度)
    double c = sin(m_rad) * (1.914602 - jc * (0.004817 + 0.000014 * jc)) +
               sin(2.0 * m_rad) * (0.019993 - 0.000101 * jc) +
               sin(3.0 * m_rad) * 0.000289;

    // 5. 太阳真黄经 (度)
    double true_lon = l0 + c;

    // 6. 黄赤交角 epsilon (度)
    double eps = 23.439291 - 0.0130042 * jc;
    double eps_rad = eps * DEG_TO_RAD;
    double true_lon_rad = true_lon * DEG_TO_RAD;

    // 7. 太阳赤纬 (Declination = 直射点纬度)
    double sin_dec = sin(eps_rad) * sin(true_lon_rad);
    double dec_rad = asin(sin_dec);
    pos.subsolar_lat = (float)(dec_rad * RAD_TO_DEG);

    // 8. 太阳赤经 RA (度)
    double ra_rad = atan2(cos(eps_rad) * sin(true_lon_rad), cos(true_lon_rad));
    double ra_deg = ra_rad * RAD_TO_DEG;
    if (ra_deg < 0) ra_deg += 360.0;

    // 9. 格林威治平恒星时 GMST (度)
    double gmst = fmod(280.46061837 + 360.98564736629 * (jd - 2451545.0) +
                       0.000387933 * jc * jc, 360.0);
    if (gmst < 0) gmst += 360.0;
    pos.gmst_deg = (float)gmst;

    // 10. 太阳直射点经度 = RA - GMST
    double sub_lon = fmod(ra_deg - gmst, 360.0);
    if (sub_lon > 180.0) sub_lon -= 360.0;
    if (sub_lon < -180.0) sub_lon += 360.0;
    pos.subsolar_lon = (float)sub_lon;

    return pos;
}

float SunCalculator::getCosSolarAngle(float lat_deg, float lon_deg, float sub_lat_deg, float sub_lon_deg) {
    float lat_r = lat_deg * DEG_TO_RAD;
    float lon_r = lon_deg * DEG_TO_RAD;
    float sub_lat_r = sub_lat_deg * DEG_TO_RAD;
    float sub_lon_r = sub_lon_deg * DEG_TO_RAD;

    return sinf(sub_lat_r) * sinf(lat_r) + 
           cosf(sub_lat_r) * cosf(lat_r) * cosf(lon_r - sub_lon_r);
}

float SunCalculator::getIllumination(float cos_dist) {
    // 晨昏平滑区间: [-0.15, +0.15]
    if (cos_dist >= 0.15f) return 1.0f;
    if (cos_dist <= -0.15f) return 0.0f;
    return (cos_dist + 0.15f) / 0.30f;
}
