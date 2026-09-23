#pragma once

#include <math.h>
#include <Arduino.h>

// -------------------------------------------------------------
// LiveEarth 3D Math & Spherical Projection Library
// -------------------------------------------------------------

#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.017453292519943295769236907684886f
#endif
#ifndef RAD_TO_DEG
#define RAD_TO_DEG 57.295779513082320876798154814105f
#endif

struct Vec3 {
    float x;
    float y;
    float z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    Vec3 operator+(const Vec3& b) const { return Vec3(x + b.x, y + b.y, z + b.z); }
    Vec3 operator-(const Vec3& b) const { return Vec3(x - b.x, y - b.y, z - b.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }

    float dot(const Vec3& b) const { return x * b.x + y * b.y + z * b.z; }

    Vec3 cross(const Vec3& b) const {
        return Vec3(
            y * b.z - z * b.y,
            z * b.x - x * b.z,
            x * b.y - y * b.x
        );
    }

    float lengthSq() const { return x * x + y * y + z * z; }
    float length() const { return sqrtf(lengthSq()); }

    Vec3 normalized() const {
        float len = length();
        if (len > 1e-6f) {
            float inv = 1.0f / len;
            return Vec3(x * inv, y * inv, z * inv);
        }
        return Vec3(0, 0, 1.0f);
    }

    // 地理经纬度转三维单位球面坐标
    // lat: [-90, +90] 度 (北为正)
    // lon: [-180, +180] 度 (东为正)
    // 采用屏幕适配坐标系：+Y 朝下(南)，-Y 朝上(北)，+Z 朝向观察者(正面)，+X 朝右(东)
    static Vec3 fromLatLng(float lat_deg, float lon_deg) {
        float lat_rad = lat_deg * DEG_TO_RAD;
        float lon_rad = lon_deg * DEG_TO_RAD;
        float cos_lat = cosf(lat_rad);
        return Vec3(
            cos_lat * sinf(lon_rad),
            -sinf(lat_rad),
            cos_lat * cosf(lon_rad)
        );
    }
};

struct Matrix3x3 {
    float m[3][3];

    Matrix3x3() {
        identity();
    }

    void identity() {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                m[i][j] = (i == j) ? 1.0f : 0.0f;
            }
        }
    }

    // 绕 X 轴旋转（俯仰 Pitch）与绕 Y 轴旋转（偏航 Yaw）的组合变换矩阵
    // pitch_rad: 上下俯仰角
    // yaw_rad: 水平经度自转角
    static Matrix3x3 fromEuler(float pitch_rad, float yaw_rad) {
        Matrix3x3 res;
        float cp = cosf(pitch_rad);
        float sp = sinf(pitch_rad);
        float cy = cosf(yaw_rad);
        float sy = sinf(yaw_rad);

        // R = R_x(pitch) * R_y(yaw)
        res.m[0][0] = cy;
        res.m[0][1] = 0.0f;
        res.m[0][2] = sy;

        res.m[1][0] = sp * sy;
        res.m[1][1] = cp;
        res.m[1][2] = -sp * cy;

        res.m[2][0] = -cp * sy;
        res.m[2][1] = sp;
        res.m[2][2] = cp * cy;

        return res;
    }

    // 向量变换
    Vec3 transform(const Vec3& v) const {
        return Vec3(
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
        );
    }
};
