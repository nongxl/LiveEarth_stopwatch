#pragma once

#include <M5Unified.h>
#include "config.h"
#include "math3d.h"

// -------------------------------------------------------------
// LiveEarth 3D Globe Rendering & Gesture Engine
// -------------------------------------------------------------

class GlobeEngine {
public:
    GlobeEngine();

    void init(int16_t center_x, int16_t center_y, float radius);

    // 状态更新与物理惯性衰减
    void update(float dt_sec);

    // 3D 地球全量渲染（绘制到指定 PSRAM Canvas）
    void render(M5Canvas& canvas);

    // 手势交互接口
    void handleDrag(float delta_x, float delta_y);
    void handlePinch(float zoom_delta);
    void resetOrientation(float target_lat_deg = 32.0f, float target_lon_deg = 115.0f);

    // 坐标与视界接口
    bool projectLatLng(float lat_deg, float lon_deg, int16_t& out_screen_x, int16_t& out_screen_y, float& out_depth) const;

    float getRadius() const { return base_radius * current_zoom; }
    float getYaw() const { return current_yaw; }
    float getPitch() const { return current_pitch; }

private:
    int16_t cx;
    int16_t cy;
    float base_radius;
    float current_zoom;

    float current_pitch; // 纬度俯仰 [-80°, +80°]
    float current_yaw;   // 经度偏航自转

    float vel_pitch;     // 惯性俯仰角速度
    float vel_yaw;       // 惯性自转角速度

    Matrix3x3 rot_matrix;

    // 内部绘制分步例程
    void renderAtmosphere(M5Canvas& canvas, float r);
    void renderGraticule(M5Canvas& canvas, float r);
    void renderCoastlines(M5Canvas& canvas, float r);
    void renderPoles(M5Canvas& canvas, float r);
};
