#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include "place_marker.h"

#include "camera_player.h"

typedef CameraPlayerState SnapshotState;
#define SNAP_STATE_IDLE     CAM_STATE_IDLE
#define SNAP_STATE_FETCHING CAM_STATE_FETCHING
#define SNAP_STATE_READY    CAM_STATE_READY
#define SNAP_STATE_ERROR    CAM_STATE_ERROR

class SnapshotPlayer : public CameraPlayer {
public:
    SnapshotPlayer();
    virtual ~SnapshotPlayer();

    virtual void init() override;
    virtual void update(uint32_t now_ms) override;

    // 请求加载指定地点的实时快照
    virtual void loadPlace(const PlaceLocation* place, bool force_refresh = false) override;

    // 绘制到中等预览窗口 (240x135)
    virtual void renderPreview(M5GFX& display, int16_t x, int16_t y, int16_t w, int16_t h) override;

    // 绘制到全屏 16:9 播放器 (406x228)
    virtual void renderFullscreen(M5GFX& display, int16_t x, int16_t y, int16_t w, int16_t h) override;

    virtual CameraPlayerState getState() const override { return current_state; }
    virtual bool hasValidFrame() const override { return current_state == CAM_STATE_READY && buffer_size > 0; }

    // 视图层消费新帧标记：当且仅当新帧成功解码进 Canvas 后返回 true
    virtual bool hasNewFrame() override {
        if (frame_ready) {
            frame_ready = false;
            return true;
        }
        return false;
    }

    virtual uint32_t getFrameSeq() const override { return frame_seq; }
    virtual CameraPlayerType getPlayerType() const override { return CAM_TYPE_SNAPSHOT; }
    uint32_t getLastFetchTime() const { return last_fetch_ms; }

    // 停止并清理缓冲
    virtual void stop() override;

private:
    SnapshotState current_state;
    volatile bool need_decode;     // 后台下载完成，等待主线程解码
    volatile bool frame_ready;     // 主线程解码完成，等待视图推送重绘
    bool has_decoded_frame;
    uint32_t frame_seq;

    uint8_t* frame_buffer;
    size_t buffer_size;
    size_t buffer_capacity;

    M5Canvas canvas_preview;
    M5Canvas canvas_fullscreen;

    char current_url[256];
    const PlaceLocation* active_place;

    uint32_t last_fetch_ms;
    uint32_t fetch_start_ms;
    bool task_running;
    volatile bool stop_requested;
    SemaphoreHandle_t buffer_mutex;

    // 异步下载实现
    void startAsyncFetch(const char* url);
    static void fetchTaskStatic(void* param);
    void fetchTaskWorker();

    // 帧预解码并缓存至 PSRAM Canvas（一次解码，60FPS 零卡顿重绘）
    void decodeAndCacheFrame(M5GFX& display);
};

extern SnapshotPlayer snapshot_player;
