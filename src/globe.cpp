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
    : cx(SCREEN_CENTER_X), cy(GLOBE_CANVAS_HEIGHT / 2),
      base_radius(GLOBE_DEFAULT_RADIUS), current_zoom(1.0f),
      current_pitch(28.0f * DEG_TO_RAD), current_yaw(115.0f * DEG_TO_RAD),
      vel_pitch(0.0f), vel_yaw(0.001f),
      anim_active(false), anim_start_ms(0), anim_duration_ms(0),
      anim_start_pitch(0), anim_target_pitch(0),
      anim_start_yaw(0), anim_target_yaw(0),
      anim_start_radius(GLOBE_DEFAULT_RADIUS), anim_target_radius(GLOBE_DEFAULT_RADIUS),
      anim_start_cy(GLOBE_CANVAS_HEIGHT / 2), anim_target_cy(GLOBE_CANVAS_HEIGHT / 2),
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
    anim_active = false;
}

void GlobeEngine::setCenterAndRadius(int16_t center_x, int16_t center_y, float radius) {
    cx = center_x;
    cy = center_y;
    base_radius = radius;
    anim_active = false;
}

void GlobeEngine::flyToLatLng(float lat_deg, float lon_deg, bool is_avoidance_mode, uint32_t duration_ms, float target_zoom) {
    // 目标纬度俯仰角：
    // 在全屏居中模式下，目标地点对准地球正中心；
    // 在下移避让模式下，地球整体俯仰向前倾斜 35°，使得目标地点自然提升并稳稳停留在下方拱形地球仪的穹顶黄金焦点！
    float target_lat = is_avoidance_mode ? (lat_deg - 35.0f) : lat_deg;
    float t_pitch = target_lat * DEG_TO_RAD;
    if (t_pitch > PITCH_LIMIT) t_pitch = PITCH_LIMIT;
    if (t_pitch < -PITCH_LIMIT) t_pitch = -PITCH_LIMIT;

    float t_yaw = lon_deg * DEG_TO_RAD;
    while (current_yaw > 3.14159265f) current_yaw -= 6.2831853f;
    while (current_yaw < -3.14159265f) current_yaw += 6.2831853f;
    while (t_yaw > 3.14159265f) t_yaw -= 6.2831853f;
    while (t_yaw < -3.14159265f) t_yaw += 6.2831853f;

    // 选取最短角位移（Shortest Arc），确保沿最近方向自然转动
    float delta_yaw = t_yaw - current_yaw;
    if (delta_yaw > 3.14159265f) {
        delta_yaw -= 6.2831853f;
    } else if (delta_yaw < -3.14159265f) {
        delta_yaw += 6.2831853f;
    }

    if (duration_ms == 0) {
        current_pitch = t_pitch;
        current_yaw = t_yaw;
        current_zoom = target_zoom;
        if (is_avoidance_mode) {
            base_radius = 210.0f;
            cy = 220;
        } else {
            base_radius = GLOBE_DEFAULT_RADIUS;
            cy = GLOBE_CANVAS_HEIGHT / 2;
        }
        anim_active = false;
        vel_pitch = 0.0f;
        vel_yaw = 0.0f;
        return;
    }

    anim_start_pitch = current_pitch;
    anim_target_pitch = t_pitch;

    anim_start_yaw = current_yaw;
    anim_target_yaw = current_yaw + delta_yaw;

    anim_start_radius = base_radius;
    anim_target_radius = is_avoidance_mode ? 210.0f : GLOBE_DEFAULT_RADIUS;

    anim_start_zoom = current_zoom;
    anim_target_zoom = target_zoom;

    anim_start_cy = cy;
    anim_target_cy = is_avoidance_mode ? 220 : (GLOBE_CANVAS_HEIGHT / 2);

    anim_start_ms = millis();
    anim_duration_ms = duration_ms;
    anim_active = true;

    vel_pitch = 0.0f;
    vel_yaw = 0.0f;
}

void GlobeEngine::setSunPosition(float subsolar_lat, float subsolar_lon) {
    sun_pos.subsolar_lat = subsolar_lat;
    sun_pos.subsolar_lon = subsolar_lon;
    has_sun_data = true;
}

void GlobeEngine::handleDrag(float delta_x, float delta_y) {
    if (anim_active) {
        anim_active = false; // 用户介入拖拽，打断飞行并保持当前位置
    }

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
    if (current_zoom < GLOBE_ZOOM_MIN) current_zoom = GLOBE_ZOOM_MIN;
    if (current_zoom > GLOBE_ZOOM_MAX) current_zoom = GLOBE_ZOOM_MAX;
}

void GlobeEngine::resetOrientation(float target_lat_deg, float target_lon_deg) {
    current_pitch = target_lat_deg * DEG_TO_RAD;
    current_yaw = target_lon_deg * DEG_TO_RAD;
    vel_pitch = 0.0f;
    vel_yaw = 0.0012f;
    current_zoom = 1.0f;
    anim_active = false;
}

void GlobeEngine::update(float dt_sec) {
    if (anim_active) {
        uint32_t now = millis();
        uint32_t elapsed = now - anim_start_ms;
        if (elapsed >= anim_duration_ms) {
            anim_active = false;
            current_pitch = anim_target_pitch;
            current_yaw = anim_target_yaw;
            base_radius = anim_target_radius;
            current_zoom = anim_target_zoom;
            cy = anim_target_cy;
            vel_pitch = 0.0f;
            vel_yaw = 0.0f;
        } else {
            float t = (float)elapsed / (float)anim_duration_ms;
            // Ease-out cubic: 1 - (1 - t)^3
            float ease = 1.0f - powf(1.0f - t, 3.0f);
            current_pitch = anim_start_pitch + (anim_target_pitch - anim_start_pitch) * ease;
            current_yaw = anim_start_yaw + (anim_target_yaw - anim_start_yaw) * ease;
            base_radius = anim_start_radius + (anim_target_radius - anim_start_radius) * ease;
            current_zoom = anim_start_zoom + (anim_target_zoom - anim_start_zoom) * ease;
            cy = anim_start_cy + (int16_t)((anim_target_cy - anim_start_cy) * ease);
        }

        while (current_yaw > 3.14159265f) current_yaw -= 6.2831853f;
        while (current_yaw < -3.14159265f) current_yaw += 6.2831853f;
    } else if (fabsf(vel_yaw) > 1e-5f || fabsf(vel_pitch) > 1e-5f) {
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
    if (r < 10.0f) return;

    uint16_t* buf = (uint16_t*)canvas.getBuffer();
    if (!buf || !has_sun_data) {
        canvas.fillCircle(cx, cy, (int32_t)r, 0x08C6);
        return;
    }

    // 1. 计算太阳在当前相机屏幕坐标系下的单位向量 S_c = (Scx, Scy, Scz)
    float cLatRad = current_pitch;
    float cLonRad = current_yaw;
    float subLatR = sun_pos.subsolar_lat * DEG_TO_RAD;
    float subLonR = sun_pos.subsolar_lon * DEG_TO_RAD;

    float sin_cLat = sinf(cLatRad);
    float cos_cLat = cosf(cLatRad);
    float sin_subLat = sinf(subLatR);
    float cos_subLat = cosf(subLatR);

    float dLon_s = subLonR - cLonRad;
    float sin_dLon_s = sinf(dLon_s);
    float cos_dLon_s = cosf(dLon_s);

    // 相机坐标系：X朝右，Y朝上(对应屏幕-dy)，Z朝向观察者(对应正面半球h)
    float Scx = cos_subLat * sin_dLon_s;
    float Scy = cos_cLat * sin_subLat - sin_cLat * cos_subLat * cos_dLon_s;
    float Scz = sin_cLat * sin_subLat + cos_cLat * cos_subLat * cos_dLon_s;

    // 白昼大洋色 (清爽明亮深邃海蓝) & 黑夜大洋色 (极其幽暗深空黑蓝) & 晨昏过渡色
    const uint16_t COL_DAY_OCEAN      = 0x0A91; // 白昼碧海蔚蓝 (RGB ~ 10, 80, 140)
    const uint16_t COL_TWILIGHT_OCEAN = 0x0107; // 晨昏幽蓝 (RGB ~ 5, 32, 60)
    const uint16_t COL_NIGHT_OCEAN    = 0x0043; // 深夜极暗墨黑蓝 (RGB ~ 0, 8, 24)

    int ir = (int)r;
    int r2 = ir * ir;
    float inv_r = 1.0f / r;

    int y_min = std::max(0, cy - ir);
    int y_max = std::min((int)GLOBE_CANVAS_HEIGHT - 1, cy + ir);

    // 局部采样着色求值函数
    auto sample_ocean_col = [&](int dx, float dy_term, float max_dx2_f) -> uint16_t {
        float h2 = max_dx2_f - (float)(dx * dx);
        float h = (h2 > 0.0f) ? sqrtf(h2) : 0.0f;
        float cos_solar = (dx * Scx - dy_term + h * Scz) * inv_r;

        if (cos_solar >= 0.40f) {
            return COL_DAY_OCEAN;
        } else if (cos_solar >= 0.05f) {
            float u = (cos_solar - 0.05f) / 0.35f;
            float t = u * u * (3.0f - 2.0f * u);
            return blendRGB565(COL_TWILIGHT_OCEAN, COL_DAY_OCEAN, t);
        } else if (cos_solar >= -0.30f) {
            float u = (cos_solar + 0.30f) / 0.35f;
            float t = u * u * (3.0f - 2.0f * u);
            return blendRGB565(COL_NIGHT_OCEAN, COL_TWILIGHT_OCEAN, t);
        } else {
            return COL_NIGHT_OCEAN;
        }
    };

    for (int y = y_min; y <= y_max; ++y) {
        int dy = y - cy;
        int dy2 = dy * dy;
        int max_dx2 = r2 - dy2;
        if (max_dx2 < 0) continue;
        int w = (int)sqrtf((float)max_dx2);

        int x_min = std::max(0, cx - w);
        int x_max = std::min((int)GLOBE_CANVAS_WIDTH - 1, cx + w);
        if (x_min > x_max) continue;

        uint16_t* ptr = buf + y * GLOBE_CANVAS_WIDTH + x_min;
        float dy_term = dy * Scy;
        float max_dx2_f = (float)max_dx2;

        int x = x_min;
        uint16_t c_curr = sample_ocean_col(x - cx, dy_term, max_dx2_f);

        // 4像素跨步批量求值：减少75%数学运算与开平方，纯色大洋区间极速填充
        while (x + 4 <= x_max) {
            uint16_t c_next = sample_ocean_col((x + 4) - cx, dy_term, max_dx2_f);

            if (c_curr == c_next) {
                uint16_t s = __builtin_bswap16(c_curr);
                ptr[0] = s;
                ptr[1] = s;
                ptr[2] = s;
                ptr[3] = s;
            } else {
                ptr[0] = __builtin_bswap16(c_curr);
                ptr[1] = __builtin_bswap16(blendRGB565(c_curr, c_next, 0.25f));
                ptr[2] = __builtin_bswap16(blendRGB565(c_curr, c_next, 0.50f));
                ptr[3] = __builtin_bswap16(blendRGB565(c_curr, c_next, 0.75f));
            }

            ptr += 4;
            x += 4;
            c_curr = c_next;
        }

        // 处理行尾余数
        while (x <= x_max) {
            *ptr++ = __builtin_bswap16(sample_ocean_col(x - cx, dy_term, max_dx2_f));
            x++;
        }
    }
}

void GlobeEngine::renderTerminatorLine(M5Canvas& canvas, float r) {
    // 昼夜过渡与落日霞光已全域融入超宽三次平滑大洋与陆地光照引擎中，
    // 不再叠加刺眼生硬的单像素高对比实线，呈现真实太空视角的柔润晨昏漫反射
}

void GlobeEngine::renderCoordinateGrid(M5Canvas& canvas, float r) {
    // 雅致微光经纬网格色彩：白天清爽淡蓝，黑夜沉浸微暗蓝灰
    const uint16_t COL_GRID_STANDARD = 0x2354; // 普通经纬网白昼色 (RGB ~ 35, 105, 165)
    const uint16_t COL_GRID_PRIME    = 0x4C9B; // 赤道与本初子午线白昼色 (RGB ~ 70, 145, 215)
    const uint16_t COL_GRID_NIGHT    = 0x0905; // 黑夜半球经纬网暗调色 (RGB ~ 8, 18, 42)

    float cLatRad = current_pitch;
    float cLonRad = current_yaw;
    float sin_cLat = sinf(cLatRad);
    float cos_cLat = cosf(cLatRad);
    float sin_cLon = sinf(cLonRad);
    float cos_cLon = cosf(cLonRad);

    float sin_subLat = 0.0f, cos_subLat = 1.0f, sin_subLon = 0.0f, cos_subLon = 1.0f;
    if (has_sun_data) {
        float subLatR = sun_pos.subsolar_lat * DEG_TO_RAD;
        float subLonR = sun_pos.subsolar_lon * DEG_TO_RAD;
        sin_subLat = sinf(subLatR);
        cos_subLat = cosf(subLatR);
        sin_subLon = sinf(subLonR);
        cos_subLon = cosf(subLonR);
    }

    // 1. 绘制纬度圈（Parallels）：-60°, -30°, 0° (赤道), +30°, +60°
    static const float PARALLELS[] = { -60.0f, -30.0f, 0.0f, 30.0f, 60.0f };
    for (float latDeg : PARALLELS) {
        float latRad = latDeg * DEG_TO_RAD;
        float sin_lat = sinf(latRad);
        float cos_lat = cosf(latRad);

        uint16_t base_line_col = (latDeg == 0.0f) ? COL_GRID_PRIME : COL_GRID_STANDARD;

        int prevX = -1, prevY = -1;
        bool prevVis = false;

        // 步长 8 度采样一圈（共 45 段），高速高效
        for (int step = 0; step <= 45; step++) {
            float lonRad = (-180.0f + step * 8.0f) * DEG_TO_RAD;
            float cos_lon = cosf(lonRad);
            float sin_lon = sinf(lonRad);

            float cos_dLon = cos_lon * cos_cLon + sin_lon * sin_cLon;
            float sin_dLon = sin_lon * cos_cLon - cos_lon * sin_cLon;

            float cos_c = sin_cLat * sin_lat + cos_cLat * cos_lat * cos_dLon;
            if (cos_c > 0.02f) {
                int curX = cx + (int)(r * (cos_lat * sin_dLon));
                int curY = cy - (int)(r * (cos_cLat * sin_lat - sin_cLat * cos_lat * cos_dLon));

                uint16_t seg_col = base_line_col;
                if (has_sun_data) {
                    float cos_solar = sin_subLat * sin_lat + cos_subLat * cos_lat * (cos_lon * cos_subLon + sin_lon * sin_subLon);
                    if (cos_solar >= 0.30f) {
                        seg_col = base_line_col;
                    } else if (cos_solar <= -0.30f) {
                        seg_col = COL_GRID_NIGHT;
                    } else {
                        float u = (cos_solar + 0.30f) / 0.60f;
                        float t = u * u * (3.0f - 2.0f * u);
                        seg_col = blendRGB565(COL_GRID_NIGHT, base_line_col, t);
                    }
                }

                if (prevVis) {
                    canvas.drawLine(prevX, prevY, curX, curY, seg_col);
                }
                prevX = curX;
                prevY = curY;
                prevVis = true;
            } else {
                prevVis = false;
            }
        }
    }

    // 2. 绘制经度线（Meridians）：每 30° 一道，共 12 条大圆弧线
    for (int m = 0; m < 12; m++) {
        float lonDeg = m * 30.0f;
        float lonRad = lonDeg * DEG_TO_RAD;
        float cos_lon = cosf(lonRad);
        float sin_lon = sinf(lonRad);

        float cos_dLon = cos_lon * cos_cLon + sin_lon * sin_cLon;
        float sin_dLon = sin_lon * cos_cLon - cos_lon * sin_cLon;

        uint16_t base_line_col = (m == 0) ? COL_GRID_PRIME : COL_GRID_STANDARD;

        int prevX = -1, prevY = -1;
        bool prevVis = false;

        // 纬度跨度 -75° 到 +75°，步长 6° 采样（共 25 段）
        for (int step = 0; step <= 25; step++) {
            float latRad = (-75.0f + step * 6.0f) * DEG_TO_RAD;
            float sin_lat = sinf(latRad);
            float cos_lat = cosf(latRad);

            float cos_c = sin_cLat * sin_lat + cos_cLat * cos_lat * cos_dLon;
            if (cos_c > 0.02f) {
                int curX = cx + (int)(r * (cos_lat * sin_dLon));
                int curY = cy - (int)(r * (cos_cLat * sin_lat - sin_cLat * cos_lat * cos_dLon));

                uint16_t seg_col = base_line_col;
                if (has_sun_data) {
                    float cos_solar = sin_subLat * sin_lat + cos_subLat * cos_lat * (cos_lon * cos_subLon + sin_lon * sin_subLon);
                    if (cos_solar >= 0.30f) {
                        seg_col = base_line_col;
                    } else if (cos_solar <= -0.30f) {
                        seg_col = COL_GRID_NIGHT;
                    } else {
                        float u = (cos_solar + 0.30f) / 0.60f;
                        float t = u * u * (3.0f - 2.0f * u);
                        seg_col = blendRGB565(COL_GRID_NIGHT, base_line_col, t);
                    }
                }

                if (prevVis) {
                    canvas.drawLine(prevX, prevY, curX, curY, seg_col);
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

                            // 连续无级三次平滑渐变：消除生硬的阈值跳变
                            if (cos_solar >= 0.40f) {
                                lineColor = COL_DAY_COAST;
                            } else if (cos_solar >= 0.05f) {
                                float u = (cos_solar - 0.05f) / 0.35f;
                                float t = u * u * (3.0f - 2.0f * u);
                                lineColor = blendRGB565(COL_TWILIGHT_COAST, COL_DAY_COAST, t);
                            } else if (cos_solar >= -0.30f) {
                                float u = (cos_solar + 0.30f) / 0.35f;
                                float t = u * u * (3.0f - 2.0f * u);
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

    // 3. 淡蓝色优雅经纬网格（5条纬线圈 + 12条经线弧）
    renderCoordinateGrid(canvas, r);

    // 4. 高精度多路径陆地轮廓（白昼翡翠亮线 / 晨昏平滑渐变 / 黑夜暗调青灰）
    renderHighPrecisionContinents(canvas, r);

    // 5. 夜间半球城市星光火（NASA VIIRS 夜光数据采样）
    renderCityNightLights(canvas, r);

    // 5. 太阳直射耀斑指示
    renderSunIndicator(canvas, r);

    // 6. 大气层散射外发光边缘
    renderAtmosphereGlow(canvas, r);

    // 7. 地极微标
    renderPoles(canvas, r);
}
