#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include "place_marker.h"

// -------------------------------------------------------------
// LiveEarth Camera Player Abstract Interface (Phase 9)
// Decouples playback engines (Snapshot / MJPEG / HLS) from UI views
// -------------------------------------------------------------

enum CameraPlayerState {
    CAM_STATE_IDLE = 0,
    CAM_STATE_FETCHING,
    CAM_STATE_READY,
    CAM_STATE_ERROR
};

enum CameraPlayerType {
    CAM_TYPE_SNAPSHOT = 0,
    CAM_TYPE_MJPEG,
    CAM_TYPE_HLS
};

class CameraPlayer {
public:
    virtual ~CameraPlayer() = default;

    virtual void init() = 0;
    virtual void update(uint32_t now_ms) = 0;

    // 请求加载指定地点的实时流或快照
    virtual void loadPlace(const PlaceLocation* place, bool force_refresh = false) = 0;

    // 绘制到中等预览窗口 (240x135)
    virtual void renderPreview(M5GFX& display, int16_t x, int16_t y, int16_t w, int16_t h) = 0;

    // 绘制到全屏 16:9 播放器 (406x228)
    virtual void renderFullscreen(M5GFX& display, int16_t x, int16_t y, int16_t w, int16_t h) = 0;

    virtual CameraPlayerState getState() const = 0;
    virtual bool hasValidFrame() const = 0;
    virtual bool hasNewFrame() = 0;
    virtual uint32_t getFrameSeq() const = 0;
    virtual CameraPlayerType getPlayerType() const = 0;

    // 停止并清理缓冲
    virtual void stop() = 0;
};
