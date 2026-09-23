#include "globe.h"
#include "continent_data.h"

#define PITCH_LIMIT (78.0f * DEG_TO_RAD)
#define DAMPING 0.91f

GlobeEngine::GlobeEngine() 
    : cx(SCREEN_CENTER_X), cy(SCREEN_CENTER_Y), 
      base_radius(155.0f), current_zoom(1.0f),
      current_pitch(25.0f * DEG_TO_RAD), current_yaw(115.0f * DEG_TO_RAD),
      vel_pitch(0.0f), vel_yaw(0.002f) // 初始微弱自转
{
    rot_matrix = Matrix3x3::fromEuler(current_pitch, current_yaw);
}

void GlobeEngine::init(int16_t center_x, int16_t center_y, float radius) {
    cx = center_x;
    cy = center_y;
    base_radius = radius;
    rot_matrix = Matrix3x3::fromEuler(current_pitch, current_yaw);
}

void GlobeEngine::handleDrag(float delta_x, float delta_y) {
    float r = base_radius * current_zoom;
    if (r < 10.0f) return;

    // 拖拽映射：手指往右，经度自转角增加；手指往上，俯仰角增加
    float dyaw = (delta_x / r) * 1.25f;
    float dpitch = -(delta_y / r) * 1.25f;

    current_yaw += dyaw;
    current_pitch += dpitch;

    if (current_pitch > PITCH_LIMIT) current_pitch = PITCH_LIMIT;
    if (current_pitch < -PITCH_LIMIT) current_pitch = -PITCH_LIMIT;

    while (current_yaw > 3.1415926f) current_yaw -= 6.2831853f;
    while (current_yaw < -3.1415926f) current_yaw += 6.2831853f;

    // 注入角动量（用于松手后的惯性滑行）
    vel_yaw = dyaw * 0.4f;
    vel_pitch = dpitch * 0.4f;

    rot_matrix = Matrix3x3::fromEuler(current_pitch, current_yaw);
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
    vel_yaw = 0.0015f; // 轻微保持自转
    current_zoom = 1.0f;
    rot_matrix = Matrix3x3::fromEuler(current_pitch, current_yaw);
}

void GlobeEngine::update(float dt_sec) {
    // 惯性滑动衰减
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

        while (current_yaw > 3.1415926f) current_yaw -= 6.2831853f;
        while (current_yaw < -3.1415926f) current_yaw += 6.2831853f;

        vel_yaw *= DAMPING;
        vel_pitch *= DAMPING;

        rot_matrix = Matrix3x3::fromEuler(current_pitch, current_yaw);
    }
}

bool GlobeEngine::projectLatLng(float lat_deg, float lon_deg, int16_t& out_screen_x, int16_t& out_screen_y, float& out_depth) const {
    Vec3 p0 = Vec3::fromLatLng(lat_deg, lon_deg);
    Vec3 p = rot_matrix.transform(p0);
    out_depth = p.z;

    float r = base_radius * current_zoom;
    out_screen_x = cx + (int16_t)(p.x * r);
    out_screen_y = cy + (int16_t)(p.y * r);

    // 仅正面可见
    return (p.z > 0.01f);
}

// -------------------------------------------------------------
// Rendering Routines
// -------------------------------------------------------------

void GlobeEngine::renderAtmosphere(M5Canvas& canvas, float r) {
    // 深蓝色大洋填充基底
    canvas.fillCircle(cx, cy, (int32_t)r, COLOR_EARTH_OCEAN);

    // 外缘大气层漫反射光圈
    canvas.drawCircle(cx, cy, (int32_t)(r + 1), 0x1A4F);
    canvas.drawCircle(cx, cy, (int32_t)(r + 3), 0x09E9);
    canvas.drawCircle(cx, cy, (int32_t)(r + 5), 0x0185);
}

void GlobeEngine::renderGraticule(M5Canvas& canvas, float r) {
    const uint16_t GRID_COLOR = 0x1188;
    const uint16_t EQUATOR_COLOR = 0x228D;

    // 1. 纬线圈：[-60°, -30°, 0°(赤道), +30°, +60°]
    static const float LAT_STEPS[] = {-60.0f, -30.0f, 0.0f, 30.0f, 60.0f};
    for (float lat : LAT_STEPS) {
        uint16_t col = (lat == 0.0f) ? EQUATOR_COLOR : GRID_COLOR;
        Vec3 prev_p;
        bool prev_vis = false;

        for (int step = 0; step <= 36; step++) {
            float lon = -180.0f + (step * 10.0f);
            Vec3 p = rot_matrix.transform(Vec3::fromLatLng(lat, lon));
            bool vis = (p.z > 0.02f);

            if (step > 0 && prev_vis && vis) {
                int16_t x1 = cx + (int16_t)(prev_p.x * r);
                int16_t y1 = cy + (int16_t)(prev_p.y * r);
                int16_t x2 = cx + (int16_t)(p.x * r);
                int16_t y2 = cy + (int16_t)(p.y * r);
                canvas.drawLine(x1, y1, x2, y2, col);
            }
            prev_p = p;
            prev_vis = vis;
        }
    }

    // 2. 经线圈：每 30° 一条，从 -180° 到 150°
    for (float lon = -180.0f; lon < 180.0f; lon += 30.0f) {
        Vec3 prev_p;
        bool prev_vis = false;

        for (int step = 0; step <= 24; step++) {
            float lat = -80.0f + (step * (160.0f / 24.0f));
            Vec3 p = rot_matrix.transform(Vec3::fromLatLng(lat, lon));
            bool vis = (p.z > 0.02f);

            if (step > 0 && prev_vis && vis) {
                int16_t x1 = cx + (int16_t)(prev_p.x * r);
                int16_t y1 = cy + (int16_t)(prev_p.y * r);
                int16_t x2 = cx + (int16_t)(p.x * r);
                int16_t y2 = cy + (int16_t)(p.y * r);
                canvas.drawLine(x1, y1, x2, y2, GRID_COLOR);
            }
            prev_p = p;
            prev_vis = vis;
        }
    }
}

void GlobeEngine::renderCoastlines(M5Canvas& canvas, float r) {
    const uint16_t COAST_COLOR = 0x4653;      // 科技感青翠绿 (#46ca99)
    const uint16_t COAST_GLOW = 0x22ED;       // 微弱柔和线光晕

    for (size_t c = 0; c < WORLD_COASTLINES_COUNT; c++) {
        const auto& line = WORLD_COASTLINES[c];
        Vec3 prev_p;
        bool prev_vis = false;

        for (uint16_t i = 0; i < line.count; i++) {
            float lat = pgm_read_float(&line.points[i][0]);
            float lon = pgm_read_float(&line.points[i][1]);

            Vec3 p = rot_matrix.transform(Vec3::fromLatLng(lat, lon));
            bool vis = (p.z > 0.01f);

            if (i > 0 && prev_vis && vis) {
                int16_t x1 = cx + (int16_t)(prev_p.x * r);
                int16_t y1 = cy + (int16_t)(prev_p.y * r);
                int16_t x2 = cx + (int16_t)(p.x * r);
                int16_t y2 = cy + (int16_t)(p.y * r);

                canvas.drawLine(x1, y1, x2, y2, COAST_COLOR);
            }
            prev_p = p;
            prev_vis = vis;
        }

        // 闭合折线连回首点
        if (line.closed && line.count > 2) {
            float lat0 = pgm_read_float(&line.points[0][0]);
            float lon0 = pgm_read_float(&line.points[0][1]);
            Vec3 p0 = rot_matrix.transform(Vec3::fromLatLng(lat0, lon0));
            if (prev_vis && (p0.z > 0.01f)) {
                int16_t x1 = cx + (int16_t)(prev_p.x * r);
                int16_t y1 = cy + (int16_t)(prev_p.y * r);
                int16_t x2 = cx + (int16_t)(p0.x * r);
                int16_t y2 = cy + (int16_t)(p0.y * r);
                canvas.drawLine(x1, y1, x2, y2, COAST_COLOR);
            }
        }
    }
}

void GlobeEngine::renderPoles(M5Canvas& canvas, float r) {
    // 北极点
    Vec3 north_pole = rot_matrix.transform(Vec3::fromLatLng(90.0f, 0.0f));
    if (north_pole.z > 0.05f) {
        int16_t px = cx + (int16_t)(north_pole.x * r);
        int16_t py = cy + (int16_t)(north_pole.y * r);
        canvas.drawPixel(px, py, COLOR_ACCENT_CYAN);
        canvas.drawCircle(px, py, 2, COLOR_ACCENT_CYAN);
    }

    // 南极点
    Vec3 south_pole = rot_matrix.transform(Vec3::fromLatLng(-90.0f, 0.0f));
    if (south_pole.z > 0.05f) {
        int16_t px = cx + (int16_t)(south_pole.x * r);
        int16_t py = cy + (int16_t)(south_pole.y * r);
        canvas.drawPixel(px, py, COLOR_ACCENT_CYAN);
        canvas.drawCircle(px, py, 2, COLOR_ACCENT_CYAN);
    }
}

void GlobeEngine::render(M5Canvas& canvas) {
    float r = base_radius * current_zoom;

    // 1. 大气辉光与深海底色
    renderAtmosphere(canvas, r);

    // 2. 3D 经纬网格线
    renderGraticule(canvas, r);

    // 3. 全球海岸线与大陆轮廓
    renderCoastlines(canvas, r);

    // 4. 地极微标
    renderPoles(canvas, r);
}
