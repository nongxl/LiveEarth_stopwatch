#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include "place_marker.h"

enum SnapshotState {
    SNAP_STATE_IDLE = 0,
    SNAP_STATE_FETCHING,
    SNAP_STATE_READY,
    SNAP_STATE_ERROR
};

class SnapshotPlayer {
public:
    SnapshotPlayer();
    ~SnapshotPlayer();

    void init();
    void update(uint32_t now_ms);

    // 请求加载指定地点的实时快照
    void loadPlace(const PlaceLocation* place, bool force_refresh = false);

    // 绘制到中等预览窗口 (居中适应 maxWidth x maxHeight)
    void renderPreview(M5GFX& display, int16_t x, int16_t y, int16_t w, int16_t h);

    // 绘制到全屏 16:9 播放器 (406x228 居中)
    void renderFullscreen(M5GFX& display, int16_t x, int16_t y, int16_t w, int16_t h);

    SnapshotState getState() const { return current_state; }
    bool hasValidFrame() const { return current_state == SNAP_STATE_READY && buffer_size > 0; }
    uint32_t getLastFetchTime() const { return last_fetch_ms; }

    // 停止并清理缓冲
    void stop();

private:
    SnapshotState current_state;
    uint8_t* frame_buffer;
    size_t buffer_size;
    size_t buffer_capacity;

    char current_url[256];
    const PlaceLocation* active_place;

    uint32_t last_fetch_ms;
    uint32_t fetch_start_ms;
    bool task_running;
    SemaphoreHandle_t buffer_mutex;

    // 异步下载实现
    void startAsyncFetch(const char* url);
    static void fetchTaskStatic(void* param);
    void fetchTaskWorker();
};

extern SnapshotPlayer snapshot_player;
