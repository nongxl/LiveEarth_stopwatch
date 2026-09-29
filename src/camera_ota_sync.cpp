#include "camera_ota_sync.h"
#include "place_marker.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>

CameraOtaSync camera_ota_sync;

// 优先采用国内加速 CDN 节点，备用官方 GitHub Raw
static const char* SYNC_URL_MIRRORS[] = {
    "https://testingcf.jsdelivr.net/gh/nongxl/LiveEarth_stopwatch@main/data/places_ota.json",
    "https://fastly.jsdelivr.net/gh/nongxl/LiveEarth_stopwatch@main/data/places_ota.json",
    "https://raw.githubusercontent.com/nongxl/LiveEarth_stopwatch/main/data/places_ota.json"
};
static const size_t MIRRORS_COUNT = sizeof(SYNC_URL_MIRRORS) / sizeof(SYNC_URL_MIRRORS[0]);

CameraOtaSync::CameraOtaSync()
    : syncing(false),
      task_spawned(false),
      last_sync_success_ms(0),
      last_attempt_ms(0),
      sync_task_handle(nullptr)
{
}

void CameraOtaSync::startSilentSync() {
    if (task_spawned || syncing) return;
    if (WiFi.status() != WL_CONNECTED) return;

    // 每次尝试后至少冷却 30 分钟 (1,800,000 ms)，开机后仅静默探测一次，绝不高频重复拉取！
    uint32_t now = millis();
    if (last_attempt_ms > 0 && (now - last_attempt_ms < 1800000)) {
        return;
    }
    last_attempt_ms = now;

    task_spawned = true;
    syncing = true;

    // 绑定至 Core 0，优先级 1（最低工作优先级），避免干扰 Core 1 渲染地球仪 60FPS
    xTaskCreatePinnedToCore(
        syncTaskWorker,
        "cam_ota_sync",
        6144,
        this,
        1,
        &sync_task_handle,
        0
    );
}

void CameraOtaSync::syncTaskWorker(void* param) {
    CameraOtaSync* instance = (CameraOtaSync*)param;
    if (instance) {
        instance->doSync();
    }
    vTaskDelete(NULL);
}

void CameraOtaSync::doSync() {
    // 开机后延时 3.5 秒等待前台 UI 完全进入稳态，避免争抢网络握手
    vTaskDelay(pdMS_TO_TICKS(3500));

    if (WiFi.status() != WL_CONNECTED) {
        syncing = false;
        task_spawned = false;
        return;
    }

    Serial.println("[CameraSync] Initiating silent background sync for live cameras...");

    WiFiClientSecure client;
    client.setInsecure(); // 忽略证书校验，加快 TLS 握手速度与适应各类 CDN 证书
    client.setTimeout(4);

    bool sync_ok = false;

    for (size_t m = 0; m < MIRRORS_COUNT; m++) {
        const char* url = SYNC_URL_MIRRORS[m];
        Serial.printf("[CameraSync] Trying mirror %u/%u: %s\n", (unsigned int)(m + 1), (unsigned int)MIRRORS_COUNT, url);

        HTTPClient http;
        http.setReuse(false);
        http.setTimeout(4500); // 4.5 秒严格单镜像超时，快速失败绝不卡顿

        if (!http.begin(client, url)) {
            continue;
        }

        http.addHeader("User-Agent", "LiveEarth-Stopwatch/1.0 (ESP32-S3)");
        http.addHeader("Accept", "application/json");
        http.addHeader("Connection", "close");

        int http_code = http.GET();
        if (http_code == HTTP_CODE_OK) {
            int len = http.getSize();
            WiFiClient* stream = http.getStreamPtr();

            if (len <= 0) len = 64 * 1024; // 若无 Content-Length，按最大 64KB 限制
            if (len > 128 * 1024) len = 128 * 1024;

            char* buf = (char*)heap_caps_malloc(len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (buf) {
                size_t total_read = 0;
                uint32_t read_start = millis();

                while (http.connected() && (len > 0 ? (int)total_read < len : true)) {
                    size_t avail = stream->available();
                    if (avail > 0) {
                        size_t to_read = (avail > 2048) ? 2048 : avail;
                        if (total_read + to_read > (size_t)len) to_read = len - total_read;
                        if (to_read == 0) break;

                        int bytes = stream->readBytes(buf + total_read, to_read);
                        if (bytes > 0) {
                            total_read += bytes;
                            read_start = millis();
                        }
                    } else {
                        if (millis() - read_start > 3000) break;
                        vTaskDelay(pdMS_TO_TICKS(5));
                    }
                }
                buf[total_read] = '\0';
                http.end();

                // 结构完整性快速探测：首字符必须是 '['，尾有效字符必须是 ']'
                bool json_valid = false;
                if (total_read > 200) {
                    char* first_p = buf;
                    while (*first_p && (*first_p == ' ' || *first_p == '\r' || *first_p == '\n' || *first_p == '\t')) first_p++;
                    char* last_p = buf + total_read - 1;
                    while (last_p > first_p && (*last_p == ' ' || *last_p == '\r' || *last_p == '\n' || *last_p == '\t' || *last_p == '\0')) last_p--;

                    if (*first_p == '[' && *last_p == ']') {
                        json_valid = true;
                    }
                }

                if (json_valid) {
                    // 1. 尝试原子加载进 PSRAM 活跃地点池
                    if (place_mgr.loadFromJson(buf, total_read)) {
                        // 2. 持久化写入本地 LittleFS，保证下一次开机即生效且无网络也能加载！
                        File f = LittleFS.open("/places.json", "w");
                        if (f) {
                            f.write((const uint8_t*)buf, total_read);
                            f.close();
                            Serial.printf("[CameraSync] Successfully cached %u bytes to LittleFS /places.json!\n", (unsigned int)total_read);
                        }
                        last_sync_success_ms = millis();
                        sync_ok = true;
                        free(buf);
                        break; // 同步成功，退出镜像循环
                    }
                }

                free(buf);
            }
        } else {
            http.end();
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (sync_ok) {
        Serial.printf("[CameraSync] Sync completed! Active places: %u\n", (unsigned int)GLOBAL_PLACES_COUNT);
    } else {
        Serial.println("[CameraSync] Background sync skipped or unavailable. Gracefully retained local dataset.");
    }

    syncing = false;
    task_spawned = false;
}
