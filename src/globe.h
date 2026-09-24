#pragma once

#include <M5Unified.h>
#include "config.h"
#include "sun_calc.h"

// -------------------------------------------------------------
// LiveEarth 3D Globe Rendering Engine with Day/Night Physics
// -------------------------------------------------------------

class GlobeEngine {
public:
    GlobeEngine();

    void init(int16_t center_x, int16_t center_y, float radius);

    // 设置与更新太阳直射点
    void setSunPosition(float subsolar_lat, float subsolar_lon);

    // 状态更新与惯性衰减
    void update(float dt_sec);

    // 3D 地球全量渲染（绘制到 466x466 PSRAM 画布）
    void render(M5Canvas& canvas);

    // 手势交互接口
    void handleDrag(float delta_x, float delta_y);
    void handlePinch(float zoom_delta);
    void resetOrientation(float target_lat_deg = 32.0f, float target_lon_deg = 115.0f);

    // 经纬度投影到屏幕
    bool projectLatLng(float lat_deg, float lon_deg, int16_t& out_screen_x, int16_t& out_screen_y, float& out_depth) const;

    // 动态调整视口中心与基准半径 (用于下移避让)
    void setCenterAndRadius(int16_t center_x, int16_t center_y, float radius);
    // 平滑飞行飞掠动画接口 (自然平滑旋转并聚焦)
    void flyToLatLng(float lat_deg, float lon_deg, bool is_avoidance_mode = false, uint32_t duration_ms = 650, float target_zoom = 1.0f);
    bool isAnimating() const { return anim_active; }

    float getRadius() const { return base_radius * current_zoom; }
    float getZoom() const { return current_zoom; }
    void setZoom(float z) { current_zoom = (z < 0.65f) ? 0.65f : ((z > 2.2f) ? 2.2f : z); }
    float getYaw() const { return current_yaw; }
    float getPitch() const { return current_pitch; }
    float getSubsolarLat() const { return sun_pos.subsolar_lat; }
    float getSubsolarLon() const { return sun_pos.subsolar_lon; }

private:
    int16_t cx;
    int16_t cy;
    float base_radius;
    float current_zoom;

    float current_pitch; // 纬度俯仰 [-78°, +78°] 弧度
    float current_yaw;   // 经度自转 弧度

    float vel_pitch;     // 惯性俯仰角速度
    float vel_yaw;       // 惯性自转角速度

    // 平滑过渡飞行动画状态
    bool anim_active;
    uint32_t anim_start_ms;
    uint32_t anim_duration_ms;
    float anim_start_pitch, anim_target_pitch;
    float anim_start_yaw, anim_target_yaw;
    float anim_start_radius, anim_target_radius;
    float anim_start_zoom, anim_target_zoom;
    int16_t anim_start_cy, anim_target_cy;

    SunPosition sun_pos;
    bool has_sun_data;

    // 渲染分步
    void renderOceanGlobe(M5Canvas& canvas, float r);
    void renderTerminatorLine(M5Canvas& canvas, float r);
    void renderCoordinateGrid(M5Canvas& canvas, float r);
    void renderAtmosphereGlow(M5Canvas& canvas, float r);
    void renderHighPrecisionContinents(M5Canvas& canvas, float r);
    void renderCityNightLights(M5Canvas& canvas, float r);
    void renderSunIndicator(M5Canvas& canvas, float r);
    void renderPoles(M5Canvas& canvas, float r);
};
