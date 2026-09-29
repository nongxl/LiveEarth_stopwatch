#pragma once

#include <Arduino.h>

class CameraOtaSync {
public:
    CameraOtaSync();

    // 在 Wi-Fi 连通后触发（只启动一次后台异步线程，避免重复拉取）
    void startSilentSync();

    // 当前是否正在同步
    bool isSyncing() const { return syncing; }

    // 上一次同步成功时间戳 (ms)，0 表示未同步或失败
    uint32_t getLastSyncTime() const { return last_sync_success_ms; }

private:
    volatile bool syncing;
    volatile bool task_spawned;
    uint32_t last_sync_success_ms;
    uint32_t last_attempt_ms;
    TaskHandle_t sync_task_handle;

    static void syncTaskWorker(void* param);
    void doSync();
};

extern CameraOtaSync camera_ota_sync;
