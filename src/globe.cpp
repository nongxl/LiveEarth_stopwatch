#pragma GCC optimize ("O3")
#pragma GCC optimize ("fast-math")

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
    : cx(GLOBE_CANVAS_CENTER), cy(GLOBE_CANVAS_CENTER),
      base_radius(GLOBE_DEFAULT_RADIUS), current_zoom(1.0f),
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

    // 拖拽方向：手指往右，球体表面随手指往右移动（视野经度减少）；手指往下，球体表面随手指往下移动（视野纬度增加）
    float dyaw = -(delta_x / r) * 1.35f;
    float dpitch = (delta_y / r) * 1.35f;

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

// -------------------------------------------------------------
// Helper: Fast RGB565 Linear Color Interpolation
// -------------------------------------------------------------
static inline uint16_t blendRGB565(uint16_t c1, uint16_t c2, float t) {
    if (t <= 0.0f) return c1;
    if (t >= 1.0f) return c2;
    uint_fast8_t r1 = (c1 >> 11) & 0x1F;
    uint_fast8_t g1 = (c1 >> 5)  & 0x3F;
    uint_fast8_t b1 = c1 & 0x1F;

    uint_fast8_t r2 = (c2 >> 11) & 0x1F;
    uint_fast8_t g2 = (c2 >> 5)  & 0x3F;
    uint_fast8_t b2 = c2 & 0x1F;

    uint_fast8_t r = r1 + (uint_fast8_t)((r2 - r1) * t);
    uint_fast8_t g = g1 + (uint_fast8_t)((g2 - g1) * t);
    uint_fast8_t b = b1 + (uint_fast8_t)((b2 - b1) * t);

    return (r << 11) | (g << 5) | b;
}

void GlobeEngine::renderOceanGlobe(M5Canvas& canvas, float r) {
    // 纯正球体深邃大洋底色（完美贴合球心 (cx, cy)）
    canvas.fillCircle(cx, cy, (int32_t)r, 0x08C6);
}

void GlobeEngine::renderTerminatorLine(M5Canvas& canvas, float r) {
    if (!has_sun_data) return;

    // 太阳直射方向单位向量 S = (Sx, Sy, Sz)
    float subLatR = sun_pos.subsolar_lat * DEG_TO_RAD;
    float subLonR = sun_pos.subsolar_lon * DEG_TO_RAD;
    float Sx = cosf(subLatR) * cosf(subLonR);
    float Sy = cosf(subLatR) * sinf(subLonR);
    float Sz = sinf(subLatR);

    // 构造垂直于 S 的正交基底 U, V (使得 P(a) = U*cos(a) + V*sin(a) 正好是晨昏圈)
    float Ux = -Sy;
    float Uy = Sx;
    float Uz = 0.0f;
    float ulen = sqrtf(Ux * Ux + Uy * Uy);
    if (ulen > 1e-4f) {
        Ux /= ulen; Uy /= ulen;
    } else {
        Ux = 1.0f; Uy = 0.0f; Uz = 0.0f;
    }

    float Vx = Sy * Uz - Sz * Uy;
    float Vy = Sz * Ux - Sx * Uz;
    float Vz = Sx * Uy - Sy * Ux;

    // 视角中心正弦/余弦
    float cLatRad = current_pitch;
    float cLonRad = current_yaw;
    float sin_cLat = sinf(cLatRad);
    float cos_cLat = cosf(cLatRad);
    float sin_cLon = sinf(cLonRad);
    float cos_cLon = cosf(cLonRad);

    // 晨昏渐变双层落日霞光色（内层霞光金 #F5A623 -> 0xF544，外层柔和暗琥珀 #A05A18 -> 0xA2C3）
    const uint16_t COL_TERMINATOR_CORE = 0xF544;
    const uint16_t COL_TERMINATOR_GLOW = 0xA2C3;

    int prevX = -1, prevY = -1;
    bool prevVis = false;

    // 以 5 度为步长，平滑投影晨昏圈大圆弧线
    for (int step = 0; step <= 72; step++) {
        float rad = (step * 5.0f) * DEG_TO_RAD;
        float ca = cosf(rad);
        float sa = sinf(rad);

        // 晨昏圈球面点 P = (Px, Py, Pz)
        float Px = Ux * ca + Vx * sa;
        float Py = Uy * ca + Vy * sa;
        float Pz = Uz * ca + Vz * sa;

        // 投影到当前视角 (正交投影)
        float term2 = Px * cos_cLon + Py * sin_cLon;
        float cos_c = sin_cLat * Pz + cos_cLat * term2;

        if (cos_c > 0.01f) {
            float proj_x = r * (Py * cos_cLon - Px * sin_cLon);
            float proj_y = r * (cos_cLat * Pz - sin_cLat * term2);

            int curX = cx + (int)proj_x;
            int curY = cy - (int)proj_y;

            if (prevVis) {
                if (abs(curX - prevX) < 80 && abs(curY - prevY) < 80) {
                    // 绘制柔和落日霞光晨昏过渡弧
                    canvas.drawLine(prevX, prevY, curX, curY, COL_TERMINATOR_CORE);
                    // 外发光增强层次
                    canvas.drawPixel(curX, curY + 1, COL_TERMINATOR_GLOW);
                    canvas.drawPixel(curX, curY - 1, COL_TERMINATOR_GLOW);
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

void GlobeEngine::renderZeroReferenceLines(M5Canvas& canvas, float r) {
    // 调高对比度与科技质感：赤道通透青蓝(0x4EEB)，本初子午线雅致金黄(0xFE60)，锚点高亮橙金(0xFDC0)
    const uint16_t COL_EQUATOR  = 0x4EEB; 
    const uint16_t COL_MERIDIAN = 0xFE60; 
    const uint16_t COL_ANCHOR   = 0xFDC0; 
    const uint16_t COL_TICK     = 0x7BEF; 

    float cLatRad = current_pitch;
    float cLonRad = current_yaw;
    float sin_cLat = sinf(cLatRad);
    float cos_cLat = cosf(cLatRad);
    float sin_cLon = sinf(cLonRad);
    float cos_cLon = cosf(cLonRad);

    // 1. 绘制赤道圈 (0° 纬线, lat = 0)
    int prevX = -1, prevY = -1;
    bool prevVis = false;
    for (int step = 0; step <= 72; step++) {
        float lonDeg = -180.0f + step * 5.0f;
        float lonRad = lonDeg * DEG_TO_RAD;
        float cos_lon = cosf(lonRad);
        float sin_lon = sinf(lonRad);

        float cos_dLon = cos_lon * cos_cLon + sin_lon * sin_cLon;
        float sin_dLon = sin_lon * cos_cLon - cos_lon * sin_cLon;

        // lat=0 时: sin_lat = 0, cos_lat = 1
        float cos_c = cos_cLat * cos_dLon;
        if (cos_c > 0.02f) {
            int curX = cx + (int)(r * sin_dLon);
            int curY = cy - (int)(r * (-sin_cLat * cos_dLon));

            if (prevVis) {
                canvas.drawLine(prevX, prevY, curX, curY, COL_EQUATOR);
            }

            // 每隔 30 度在赤道上绘制一个微小刻度节点
            int degInt = (int)(lonDeg < 0 ? lonDeg + 360.0f : lonDeg);
            if (degInt % 30 == 0 && cos_c > 0.2f) {
                canvas.drawCircle(curX, curY, 1, COL_TICK);
            }

            prevX = curX;
            prevY = curY;
            prevVis = true;
        } else {
            prevVis = false;
        }
    }

    // 2. 绘制本初子午线 (0° 经线, lon = 0)
    prevX = -1; prevY = -1;
    prevVis = false;
    for (int step = 0; step <= 36; step++) {
        float latDeg = -85.0f + step * (170.0f / 36.0f);
        float latRad = latDeg * DEG_TO_RAD;
        float sin_lat = sinf(latRad);
        float cos_lat = cosf(latRad);

        float cos_c = sin_cLat * sin_lat + cos_cLat * cos_lat * cos_cLon;
        if (cos_c > 0.02f) {
            int curX = cx + (int)(r * cos_lat * (-sin_cLon));
            int curY = cy - (int)(r * (cos_cLat * sin_lat - sin_cLat * cos_lat * cos_cLon));

            if (prevVis) {
                canvas.drawLine(prevX, prevY, curX, curY, COL_MERIDIAN);
            }

            // 每隔 30 度在子午线上绘制刻度点
            if (abs((int)latDeg) % 30 == 0 && abs((int)latDeg) != 0 && cos_c > 0.2f) {
                canvas.drawCircle(curX, curY, 1, COL_MERIDIAN);
            }

            prevX = curX;
            prevY = curY;
            prevVis = true;
        } else {
            prevVis = false;
        }
    }

    // 3. (0°, 0°) 零度交汇锚点 HUD 定位准心（几内亚湾）
    int16_t ax, ay;
    float depth;
    if (projectLatLng(0.0f, 0.0f, ax, ay, depth) && depth > 0.05f) {
        // 双层同心准星
        canvas.drawCircle(ax, ay, 4, COL_ANCHOR);
        canvas.drawCircle(ax, ay, 2, 0xFFFF);
        canvas.drawLine(ax - 7, ay, ax + 7, ay, COL_ANCHOR);
        canvas.drawLine(ax, ay - 7, ax, ay + 7, COL_ANCHOR);

        // 仅在正对时显示清晰文字 "0°,0°"
        if (depth > 0.35f) {
            canvas.setTextColor(COL_ANCHOR);
            canvas.setTextDatum(top_left);
            canvas.setTextSize(1);
            canvas.drawString("0,0", ax + 6, ay + 3);
        }
    }

    // 4. 在赤道与本初子午线各选一个特征点添加文字微标 "0° MERIDIAN" 和 "EQUATOR"
    int16_t mx, my;
    if (projectLatLng(25.0f, 0.0f, mx, my, depth) && depth > 0.45f) {
        canvas.setTextColor(COL_MERIDIAN);
        canvas.setTextDatum(middle_left);
        canvas.setTextSize(1);
        canvas.drawString("0 E/W", mx + 4, my);
    }

    int16_t eqx, eqy;
    if (projectLatLng(0.0f, 45.0f, eqx, eqy, depth) && depth > 0.45f) {
        canvas.setTextColor(COL_EQUATOR);
        canvas.setTextDatum(bottom_center);
        canvas.setTextSize(1);
        canvas.drawString("0 LAT", eqx, eqy - 3);
    }
}

void GlobeEngine::renderAtmosphereGlow(M5Canvas& canvas, float r) {
    // 柔和微光大气层边缘光晕
    canvas.drawCircle(cx, cy, (int32_t)(r + 1), 0x22AD); // 浅钴蓝边界
    canvas.drawCircle(cx, cy, (int32_t)(r + 2), 0x1188); // 柔和渐变
}

void GlobeEngine::renderHighPrecisionContinents(M5Canvas& canvas, float r) {
    // 基础三态色彩
    const uint16_t COL_DAY_COAST      = 0x4EEB; // 白昼浅翡翠白金绿 (#4adca8)
    const uint16_t COL_TWILIGHT_COAST = 0xF544; // 晨昏落日琥珀金 (#f5a623)
    const uint16_t COL_NIGHT_COAST    = 0x19EA; // 黑夜深石板青蓝 (#1c3a5e)

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
                        uint16_t lineColor = COL_DAY_COAST;
                        if (has_sun_data) {
                            float cos_lon_sub = cos_lon * cos_subLon + sin_lon * sin_subLon;
                            float cos_solar = sin_subLat * sin_lat + cos_subLat * cos_lat * cos_lon_sub;

                            // 连续无级平滑渐变：消除生硬的阈值跳变
                            if (cos_solar >= 0.18f) {
                                lineColor = COL_DAY_COAST;
                            } else if (cos_solar >= 0.0f) {
                                float t = cos_solar / 0.18f;
                                lineColor = blendRGB565(COL_TWILIGHT_COAST, COL_DAY_COAST, t);
                            } else if (cos_solar >= -0.18f) {
                                float t = (cos_solar + 0.18f) / 0.18f;
                                lineColor = blendRGB565(COL_NIGHT_COAST, COL_TWILIGHT_COAST, t);
                            } else {
                                lineColor = COL_NIGHT_COAST;
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

    // 针对 466x466 屏幕，精选步长采样（step = 4 遍历 3000 点，兼顾极高密度与 40+ FPS）
    const int step = 4;
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

    // 2. 晨昏分界过渡光弧（大圆日落霞光渐变）
    renderTerminatorLine(canvas, r);

    // 3. 高精度多路径陆地轮廓（白昼翡翠亮线 / 晨昏平滑渐变 / 黑夜暗调青灰）
    renderHighPrecisionContinents(canvas, r);

    // 4. 0度经纬参考基准线与 (0,0) 十字锚点
    renderZeroReferenceLines(canvas, r);

    // 5. 夜间半球城市星光火（NASA VIIRS 夜光数据采样）
    renderCityNightLights(canvas, r);

    // 5. 太阳直射耀斑指示
    renderSunIndicator(canvas, r);

    // 6. 大气层散射外发光边缘
    renderAtmosphereGlow(canvas, r);

    // 7. 地极微标
    renderPoles(canvas, r);
}
