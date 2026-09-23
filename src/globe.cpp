#include "globe.h"
#include "earth_data.h"
#include "light_points_data.h"
#include <math.h>

#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.017453292519943295769236907684886f
#endif
#ifndef RAD_TO_DEG
#define RAD_TO_DEG 57.295779513082320876798154814105f
#endif

#define PITCH_LIMIT (78.0f * DEG_TO_RAD)
#define DAMPING 0.92f

GlobeEngine::GlobeEngine()
    : cx(SCREEN_CENTER_X), cy(SCREEN_CENTER_Y),
      base_radius(155.0f), current_zoom(1.0f),
      current_pitch(28.0f * DEG_TO_RAD), current_yaw(115.0f * DEG_TO_RAD),
      vel_pitch(0.0f), vel_yaw(0.001f),
      has_sun_data(true)
{
    // 默认初始太阳位置（春分点附近直射赤道偏南/经度0）
    sun_pos.subsolar_lat = 0.0f;
    sun_pos.subsolar_lon = 0.0f;
    sun_pos.gmst_deg = 0.0f;
}

void GlobeEngine::init(int16_t center_x, int16_t center_y, float radius) {
    cx = center_x;
    cy = center_y;
    base_radius = radius;
}

void GlobeEngine::setSunPosition(float subsolar_lat, float subsolar_lon) {
    sun_pos.subsolar_lat = subsolar_lat;
    sun_pos.subsolar_lon = subsolar_lon;
    has_sun_data = true;
}

void GlobeEngine::handleDrag(float delta_x, float delta_y) {
    float r = base_radius * current_zoom;
    if (r < 10.0f) return;

    float dyaw = (delta_x / r) * 1.35f;
    float dpitch = -(delta_y / r) * 1.35f;

    current_yaw += dyaw;
    current_pitch += dpitch;

    if (current_pitch > PITCH_LIMIT) current_pitch = PITCH_LIMIT;
    if (current_pitch < -PITCH_LIMIT) current_pitch = -PITCH_LIMIT;

    while (current_yaw > 3.14159265f) current_yaw -= 6.2831853f;
    while (current_yaw < -3.14159265f) current_yaw += 6.2831853f;

    vel_yaw = dyaw * 0.35f;
    vel_pitch = dpitch * 0.35f;
}

void GlobeEngine::handlePinch(float zoom_delta) {
    current_zoom += zoom_delta;
    if (current_zoom < 0.75f) current_zoom = 0.75f;
    if (current_zoom > 1.45f) current_zoom = 1.45f;
}

void GlobeEngine::resetOrientation(float target_lat_deg, float target_lon_deg) {
    current_pitch = target_lat_deg * DEG_TO_RAD;
    current_yaw = target_lon_deg * DEG_TO_RAD;
    vel_pitch = 0.0f;
    vel_yaw = 0.0012f;
    current_zoom = 1.0f;
}

void GlobeEngine::update(float dt_sec) {
    if (fabsf(vel_yaw) > 1e-5f || fabsf(vel_pitch) > 1e-5f) {
        current_yaw += vel_yaw;
        current_pitch += vel_pitch;

        if (current_pitch > PITCH_LIMIT) {
            current_pitch = PITCH_LIMIT;
            vel_pitch = 0.0f;
        } else if (current_pitch < -PITCH_LIMIT) {
            current_pitch = -PITCH_LIMIT;
            vel_pitch = 0.0f;
        }

        while (current_yaw > 3.14159265f) current_yaw -= 6.2831853f;
        while (current_yaw < -3.14159265f) current_yaw += 6.2831853f;

        vel_yaw *= DAMPING;
        vel_pitch *= DAMPING;
    }
}

bool GlobeEngine::projectLatLng(float lat_deg, float lon_deg, int16_t& out_screen_x, int16_t& out_screen_y, float& out_depth) const {
    float latRad = lat_deg * DEG_TO_RAD;
    float lonRad = lon_deg * DEG_TO_RAD;
    float cLatRad = current_pitch;
    float cLonRad = current_yaw;

    float sin_cLat = sinf(cLatRad);
    float cos_cLat = cosf(cLatRad);
    float sin_lat = sinf(latRad);
    float cos_lat = cosf(latRad);
    float dLon = lonRad - cLonRad;

    float cos_dLon = cosf(dLon);
    float sin_dLon = sinf(dLon);

    float cos_c = sin_cLat * sin_lat + cos_cLat * cos_lat * cos_dLon;
    out_depth = cos_c;

    if (cos_c <= 0.0f) return false;

    float r = base_radius * current_zoom;
    float x = r * cos_lat * sin_dLon;
    float y = r * (cos_cLat * sin_lat - sin_cLat * cos_lat * cos_dLon);

    out_screen_x = cx + (int16_t)x;
    out_screen_y = cy - (int16_t)y;

    return true;
}

// -------------------------------------------------------------
// Rendering Modules
// -------------------------------------------------------------

void GlobeEngine::renderOceanGlobe(M5Canvas& canvas, float r) {
    // 1. 先整体填充夜间深渊绀青黑底色 (Midnight Abyss #080516)
    canvas.fillCircle(cx, cy, (int32_t)r, 0x0823);

    // 2. 如果存在太阳直射数据，计算太阳在当前视角的投影与受光面漫反射
    if (has_sun_data) {
        float subLatR = sun_pos.subsolar_lat * DEG_TO_RAD;
        float subLonR = sun_pos.subsolar_lon * DEG_TO_RAD;

        float sin_cLat = sinf(current_pitch);
        float cos_cLat = cosf(current_pitch);
        float sin_subLat = sinf(subLatR);
        float cos_subLat = cosf(subLatR);
        float dLon = subLonR - current_yaw;

        float cos_c = sin_cLat * sin_subLat + cos_cLat * cos_subLat * cosf(dLon);

        // 如果太阳在正面半球 (cos_c > -0.15)，在受光侧绘制深海日照蔚蓝漫射圆弧
        float sun_x = r * cos_subLat * sinf(dLon);
        float sun_y = r * (cos_cLat * sin_subLat - sin_cLat * cos_subLat * cosf(dLon));
        int16_t sx = cx + (int16_t)(sun_x * 0.45f);
        int16_t sy = cy - (int16_t)(sun_y * 0.45f);

        // 向阳侧深钴大洋蓝 (#0a254d -> 0x092B)
        int32_t day_radius = (int32_t)(r * 0.88f);
        if (cos_c > 0.1f) {
            canvas.fillCircle(sx, sy, day_radius, 0x094C);
            canvas.fillCircle(sx, sy, (int32_t)(day_radius * 0.7f), 0x0A0F);
        } else if (cos_c > -0.2f) {
            canvas.fillCircle(sx, sy, (int32_t)(day_radius * 0.5f), 0x08CA);
        }
    }
}

void GlobeEngine::renderAtmosphereGlow(M5Canvas& canvas, float r) {
    // 多重微光大气光晕（向阳侧增强，背光侧隐约可见）
    canvas.drawCircle(cx, cy, (int32_t)(r + 1), 0x22AD); // 浅钴蓝边界
    canvas.drawCircle(cx, cy, (int32_t)(r + 2), 0x1188); // 柔和渐变
    canvas.drawCircle(cx, cy, (int32_t)(r + 4), 0x0905); // 深层弥散
}

void GlobeEngine::renderGraticule(M5Canvas& canvas, float r) {
    const uint16_t GRID_DAY_COLOR = 0x19E8;   // 白昼微弱网格
    const uint16_t GRID_NIGHT_COLOR = 0x08E3; // 夜间极淡网格
    const uint16_t EQUATOR_COLOR = 0x2BAE;    // 赤道发光线

    float cLatRad = current_pitch;
    float cLonRad = current_yaw;
    float sin_cLat = sinf(cLatRad);
    float cos_cLat = cosf(cLatRad);
    float cos_cLon = cosf(cLonRad);
    float sin_cLon = sinf(cLonRad);

    // 纬线圈：[-60°, -30°, 0°(赤道), +30°, +60°]
    static const float LAT_STEPS[] = {-60.0f, -30.0f, 0.0f, 30.0f, 60.0f};
    for (float lat : LAT_STEPS) {
        float latRad = lat * DEG_TO_RAD;
        float sin_lat = sinf(latRad);
        float cos_lat = cosf(latRad);
        bool is_equator = (lat == 0.0f);

        int prevX = -1, prevY = -1;
        bool prevVis = false;

        for (int step = 0; step <= 36; step++) {
            float lonRad = (-180.0f + step * 10.0f) * DEG_TO_RAD;
            float cos_lon = cosf(lonRad);
            float sin_lon = sinf(lonRad);

            float cos_dLon = cos_lon * cos_cLon + sin_lon * sin_cLon;
            float sin_dLon = sin_lon * cos_cLon - cos_lon * sin_cLon;

            float cos_c = sin_cLat * sin_lat + cos_cLat * cos_lat * cos_dLon;
            if (cos_c > 0.02f) {
                int curX = cx + (int)(r * cos_lat * sin_dLon);
                int curY = cy - (int)(r * (cos_cLat * sin_lat - sin_cLat * cos_lat * cos_dLon));

                if (prevVis) {
                    canvas.drawLine(prevX, prevY, curX, curY, is_equator ? EQUATOR_COLOR : GRID_DAY_COLOR);
                }
                prevX = curX;
                prevY = curY;
                prevVis = true;
            } else {
                prevVis = false;
            }
        }
    }

    // 经线圈：每 30° 一条
    for (float lon = -180.0f; lon < 180.0f; lon += 30.0f) {
        float lonRad = lon * DEG_TO_RAD;
        float cos_lon = cosf(lonRad);
        float sin_lon = sinf(lonRad);

        float cos_dLon = cos_lon * cos_cLon + sin_lon * sin_cLon;
        float sin_dLon = sin_lon * cos_cLon - cos_lon * sin_cLon;

        int prevX = -1, prevY = -1;
        bool prevVis = false;

        for (int step = 0; step <= 24; step++) {
            float latRad = (-80.0f + step * (160.0f / 24.0f)) * DEG_TO_RAD;
            float sin_lat = sinf(latRad);
            float cos_lat = cosf(latRad);

            float cos_c = sin_cLat * sin_lat + cos_cLat * cos_lat * cos_dLon;
            if (cos_c > 0.02f) {
                int curX = cx + (int)(r * cos_lat * sin_dLon);
                int curY = cy - (int)(r * (cos_cLat * sin_lat - sin_cLat * cos_lat * cos_dLon));

                if (prevVis) {
                    canvas.drawLine(prevX, prevY, curX, curY, GRID_DAY_COLOR);
                }
                prevX = curX;
                prevY = curY;
                prevVis = true;
            } else {
                prevVis = false;
            }
        }
    }
}

void GlobeEngine::renderHighPrecisionContinents(M5Canvas& canvas, float r) {
    // 视觉差异化调色板：
    // 白昼向阳：浅翡翠白金绿 (#4adca8 -> 0x4EEB)
    // 晨昏过渡带：落日琥珀金 (#e69b22 -> 0xE4C4)
    // 黑夜背光：深石板青蓝 (#1c3a5e -> 0x19EA)
    const uint16_t COL_DAY_COAST     = 0x4EEB; 
    const uint16_t COL_TWILIGHT_COAST= 0xE4C4;
    const uint16_t COL_NIGHT_COAST   = 0x19EA;

    float cLatRad = current_pitch;
    float cLonRad = current_yaw;
    float sin_cLat = sinf(cLatRad);
    float cos_cLat = cosf(cLatRad);
    float sin_cLon = sinf(cLonRad);
    float cos_cLon = cosf(cLonRad);

    float subLatR = sun_pos.subsolar_lat * DEG_TO_RAD;
    float subLonR = sun_pos.subsolar_lon * DEG_TO_RAD;
    float sin_subLat = sinf(subLatR);
    float cos_subLat = cosf(subLatR);
    float sin_subLon = sinf(subLonR);
    float cos_subLon = cosf(subLonR);

    for (int i = 0; i < world_map_count; i++) {
        const MapPath& path = world_map[i];
        int prevX = -1, prevY = -1;
        bool prevVis = false;

        for (int j = 0; j < path.length; j++) {
            const MapPoint& pt = path.points[j];
            float sin_lat = pt.sinLat;
            float cos_lat = pt.cosLat;
            float sin_lon = pt.sinLon;
            float cos_lon = pt.cosLon;

            float cos_dLon = cos_lon * cos_cLon + sin_lon * sin_cLon;
            float sin_dLon = sin_lon * cos_cLon - cos_lon * sin_cLon;

            float cos_c = sin_cLat * sin_lat + cos_cLat * cos_lat * cos_dLon;

            if (cos_c >= 0.01f) {
                int curX = cx + (int)(r * cos_lat * sin_dLon);
                int curY = cy - (int)(r * (cos_cLat * sin_lat - sin_cLat * cos_lat * cos_dLon));

                if (prevVis) {
                    if (abs(curX - prevX) < 120 && abs(curY - prevY) < 120) {
                        // 计算该地点的昼夜光照分界
                        uint16_t lineColor = COL_DAY_COAST;
                        if (has_sun_data) {
                            float cos_lon_sub = cos_lon * cos_subLon + sin_lon * sin_subLon;
                            float cos_solar = sin_subLat * sin_lat + cos_subLat * cos_lat * cos_lon_sub;

                            if (cos_solar > 0.12f) {
                                lineColor = COL_DAY_COAST;      // 白昼
                            } else if (cos_solar >= -0.12f) {
                                lineColor = COL_TWILIGHT_COAST; // 晨昏交界微金线
                            } else {
                                lineColor = COL_NIGHT_COAST;    // 深夜暗线
                            }
                        }
                        canvas.drawLine(prevX, prevY, curX, curY, lineColor);
                    }
                }
                prevX = curX;
                prevY = curY;
                prevVis = true;
            } else {
                prevVis = false;
            }
        }
    }
}

void GlobeEngine::renderCityNightLights(M5Canvas& canvas, float r) {
    if (!has_sun_data) return;

    float cLatRad = current_pitch;
    float cLonRad = current_yaw;
    float sin_cLat = sinf(cLatRad);
    float cos_cLat = cosf(cLatRad);
    float sin_cLon = sinf(cLonRad);
    float cos_cLon = cosf(cLonRad);

    float subLatR = sun_pos.subsolar_lat * DEG_TO_RAD;
    float subLonR = sun_pos.subsolar_lon * DEG_TO_RAD;
    float sin_subLat = sinf(subLatR);
    float cos_subLat = cosf(subLatR);
    float sin_subLon = sinf(subLonR);
    float cos_subLon = cosf(subLonR);

    // 针对 466x466 屏幕，精选步长采样（step = 2 遍历 6000 点，兼顾极高密度与 40+ FPS）
    const int step = 2;
    for (int i = 0; i < light_points_count; i += step) {
        float sin_lat = light_points[i].sinLat;
        float cos_lat = light_points[i].cosLat;
        float sin_lon = light_points[i].sinLon;
        float cos_lon = light_points[i].cosLon;

        // 1. 仅在黑夜半球 (cos_solar < -0.06f) 才显现城市灯火
        float cos_lon_sub = cos_lon * cos_subLon + sin_lon * sin_subLon;
        float cos_solar = sin_subLat * sin_lat + cos_subLat * cos_lat * cos_lon_sub;

        if (cos_solar < -0.06f) {
            // 2. 检查是否在正面可视半球 (cos_c > 0)
            float cos_dLon = cos_lon * cos_cLon + sin_lon * sin_cLon;
            float cos_c = sin_cLat * sin_lat + cos_cLat * cos_lat * cos_dLon;

            if (cos_c > 0.05f) {
                float sin_dLon = sin_lon * cos_cLon - cos_lon * sin_cLon;
                int px = cx + (int)(r * cos_lat * sin_dLon);
                int py = cy - (int)(r * (cos_cLat * sin_lat - sin_cLat * cos_lat * cos_dLon));

                // 深夜核心城市呈现暖金琥珀星火 (#ffe57f -> 0xFF2E)
                // 偶发主要都会中心点画双像素高亮
                if (cos_solar < -0.25f && (i % 6 == 0)) {
                    canvas.drawPixel(px, py, 0xFFE0); // 暖金
                    canvas.drawPixel(px + 1, py, 0xFFFF); // 核心晶白
                } else {
                    canvas.drawPixel(px, py, 0xFDC6); // 琥珀微光
                }
            }
        }
    }
}

void GlobeEngine::renderSunIndicator(M5Canvas& canvas, float r) {
    if (!has_sun_data) return;

    float subLatR = sun_pos.subsolar_lat * DEG_TO_RAD;
    float subLonR = sun_pos.subsolar_lon * DEG_TO_RAD;

    float sin_cLat = sinf(current_pitch);
    float cos_cLat = cosf(current_pitch);
    float sin_subLat = sinf(subLatR);
    float cos_subLat = cosf(subLatR);
    float dLon = subLonR - current_yaw;

    float cos_c = sin_cLat * sin_subLat + cos_cLat * cos_subLat * cosf(dLon);

    // 太阳直射点在正面视野内
    if (cos_c > 0.05f) {
        float x = r * cos_subLat * sinf(dLon);
        float y = r * (cos_cLat * sin_subLat - sin_cLat * cos_subLat * cosf(dLon));
        int16_t sx = cx + (int16_t)x;
        int16_t sy = cy - (int16_t)y;

        // 柔和太阳透镜耀斑 (Sun Flare)
        canvas.drawCircle(sx, sy, 14, 0x52E0); // 微金外晕
        canvas.drawCircle(sx, sy, 9, 0xA562);  // 暖光环
        canvas.drawCircle(sx, sy, 4, 0xFFE0);  // 金黄内芯
        canvas.fillCircle(sx, sy, 2, 0xFFFF);  // 纯白炽热日轮

        // 标注直射点 "SUN"
        canvas.setTextColor(0xFFE0);
        canvas.setTextDatum(middle_center);
        canvas.setTextSize(1);
        canvas.drawString("SUN", sx, sy - 18);
    }
}

void GlobeEngine::renderPoles(M5Canvas& canvas, float r) {
    // 北极点
    int16_t px, py;
    float depth;
    if (projectLatLng(90.0f, 0.0f, px, py, depth)) {
        canvas.drawPixel(px, py, COLOR_ACCENT_CYAN);
        canvas.drawCircle(px, py, 2, COLOR_ACCENT_CYAN);
        canvas.setTextColor(COLOR_ACCENT_CYAN);
        canvas.setTextDatum(middle_center);
        canvas.drawString("N", px, py - 8);
    }

    // 南极点
    if (projectLatLng(-90.0f, 0.0f, px, py, depth)) {
        canvas.drawPixel(px, py, 0x633F);
        canvas.drawCircle(px, py, 2, 0x633F);
        canvas.setTextColor(0x633F);
        canvas.setTextDatum(middle_center);
        canvas.drawString("S", px, py + 8);
    }
}

void GlobeEngine::render(M5Canvas& canvas) {
    float r = base_radius * current_zoom;

    // 1. 昼夜大洋球体底色
    renderOceanGlobe(canvas, r);

    // 2. 3D 经纬线网格
    renderGraticule(canvas, r);

    // 3. 高精度多路径陆地轮廓（白昼翡翠亮线 / 晨昏落日微金 / 黑夜暗调青灰）
    renderHighPrecisionContinents(canvas, r);

    // 4. 夜间半球城市星光火（NASA VIIRS 夜光数据采样）
    renderCityNightLights(canvas, r);

    // 5. 太阳直射耀斑指示
    renderSunIndicator(canvas, r);

    // 6. 大气层散射外发光边缘
    renderAtmosphereGlow(canvas, r);

    // 7. 地极微标
    renderPoles(canvas, r);
}
