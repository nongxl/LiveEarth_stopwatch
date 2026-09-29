#include "network_manager.h"
#include "config.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

static const char* NVS_NAMESPACE = "le_wifi";
static const char* NVS_KEY_SSID = "ssid";
static const char* NVS_KEY_PASS = "pass";

NetworkManager network_mgr;

NetworkManager::NetworkManager()
    : current_state(WIFI_STATE_DISCONNECTED),
      connect_start_ms(0),
      last_retry_ms(0),
      retry_count(0),
      is_scanning(false),
      time_synced(false),
      synced_utc_time(0),
      tz_offset_sec(0)
{
    target_ssid[0] = '\0';
    target_password[0] = '\0';
    strcpy(ip_str, "0.0.0.0");
}

bool NetworkManager::hasSavedConfig() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        return false;
    }
    String s = prefs.getString(NVS_KEY_SSID, "");
    prefs.end();
    return s.length() > 0;
}

bool NetworkManager::loadConfig(char* ssid, size_t ssid_len, char* password, size_t pass_len) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        return false;
    }
    String s = prefs.getString(NVS_KEY_SSID, "");
    String p = prefs.getString(NVS_KEY_PASS, "");
    prefs.end();

    if (s.length() == 0) return false;

    if (ssid && ssid_len > 0) {
        strncpy(ssid, s.c_str(), ssid_len - 1);
        ssid[ssid_len - 1] = '\0';
    }
    if (password && pass_len > 0) {
        strncpy(password, p.c_str(), pass_len - 1);
        password[pass_len - 1] = '\0';
    }
    return true;
}

bool NetworkManager::saveConfig(const char* ssid, const char* password) {
    if (!ssid || strlen(ssid) == 0) return false;

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        Serial.println("[NVS] Failed to open NVS for writing!");
        return false;
    }
    prefs.putString(NVS_KEY_SSID, ssid);
    prefs.putString(NVS_KEY_PASS, password ? password : "");
    prefs.end();

    Serial.printf("[NVS] Wi-Fi credentials saved to NVS: SSID='%s'\n", ssid);
    return true;
}

void NetworkManager::clearConfig() {
    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, false)) {
        prefs.clear();
        prefs.end();
        Serial.println("[NVS] Wi-Fi credentials cleared.");
    }
}

bool NetworkManager::connectSaved() {
    char s[33] = {0};
    char p[65] = {0};
    if (loadConfig(s, sizeof(s), p, sizeof(p))) {
        Serial.printf("[Network] Found saved Wi-Fi in NVS: '%s', connecting...\n", s);
        connect(s, p);
        return true;
    }
    Serial.println("[Network] No saved Wi-Fi in NVS.");
    return false;
}

void NetworkManager::init() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(50);

    // 注册系统级 Wi-Fi 扫描完成事件
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (event == ARDUINO_EVENT_WIFI_SCAN_DONE) {
            network_mgr.onScanCompleted(info.wifi_scan_done.number);
        }
    });

    // 优先读取 NVS 中存储的 Wi-Fi
    if (!connectSaved()) {
        Serial.println("[Network] Awaiting user Wi-Fi configuration via GUI.");
    }
}

void NetworkManager::connect(const char* ssid, const char* password) {
    if (!ssid || strlen(ssid) == 0) return;

    strncpy(target_ssid, ssid, sizeof(target_ssid) - 1);
    target_ssid[sizeof(target_ssid) - 1] = '\0';

    if (password) {
        strncpy(target_password, password, sizeof(target_password) - 1);
        target_password[sizeof(target_password) - 1] = '\0';
    } else {
        target_password[0] = '\0';
    }

    Serial.printf("[Network] Initiating connection to SSID: '%s'...\n", target_ssid);
    WiFi.disconnect(false);
    WiFi.begin(target_ssid, strlen(target_password) > 0 ? target_password : nullptr);

    connect_start_ms = millis();
    retry_count = 0;
    handleStateTransition(WIFI_STATE_CONNECTING);
}

void NetworkManager::disconnect() {
    WiFi.disconnect(true);
    handleStateTransition(WIFI_STATE_DISCONNECTED);
    strcpy(ip_str, "0.0.0.0");
}

void NetworkManager::resetFailState() {
    if (current_state == WIFI_STATE_FAILED) {
        handleStateTransition(WIFI_STATE_DISCONNECTED);
    }
}

int8_t NetworkManager::getRSSI() const {
    if (current_state == WIFI_STATE_CONNECTED) {
        return WiFi.RSSI();
    }
    return -100;
}

void NetworkManager::startScan(bool async) {
    Serial.println("[Network] Triggering Wi-Fi scan...");
    is_scanning = true;
    WiFi.scanDelete();
    WiFi.scanNetworks(async, true);
}

void NetworkManager::onScanCompleted(int16_t status) {
    Serial.printf("[Network] Event SCAN_DONE triggered: found %d networks.\n", status);
    if (status <= 0) {
        is_scanning = false;
        return;
    }

    scanned_networks.clear();
    for (int i = 0; i < status; i++) {
        String cur_ssid = WiFi.SSID(i);
        if (cur_ssid.length() == 0) continue; // 过滤无名广播

        int32_t cur_rssi = WiFi.RSSI(i);
        bool cur_open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);

        // 去重合并，保留信号更强的一个
        bool exist = false;
        for (auto& item : scanned_networks) {
            if (strcmp(item.ssid, cur_ssid.c_str()) == 0) {
                exist = true;
                if (cur_rssi > item.rssi) {
                    item.rssi = cur_rssi;
                    item.is_open = cur_open;
                }
                break;
            }
        }

        if (!exist && scanned_networks.size() < 30) {
            ScannedWifiItem item;
            strncpy(item.ssid, cur_ssid.c_str(), sizeof(item.ssid) - 1);
            item.ssid[sizeof(item.ssid) - 1] = '\0';
            item.rssi = cur_rssi;
            item.is_open = cur_open;
            scanned_networks.push_back(item);
        }
    }

    // 按信号强度降序排序
    std::sort(scanned_networks.begin(), scanned_networks.end(), 
              [](const ScannedWifiItem& a, const ScannedWifiItem& b) {
                  return a.rssi > b.rssi;
              });

    Serial.printf("[Network] Successfully collected %u unique Wi-Fi SSIDs.\n", (unsigned int)scanned_networks.size());
    WiFi.scanDelete();
    is_scanning = false;
}

bool NetworkManager::isScanCompleted() {
    if (!is_scanning) return true;
    int16_t status = WiFi.scanComplete();
    if (status >= 0) {
        onScanCompleted(status);
        return true;
    }
    return false;
}

int NetworkManager::getScannedCount() {
    return (int)scanned_networks.size();
}


void NetworkManager::handleStateTransition(WifiState new_state) {
    if (current_state == new_state) return;
    current_state = new_state;

    switch (new_state) {
        case WIFI_STATE_CONNECTED: {
            IPAddress ip = WiFi.localIP();
            snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
            Serial.printf("[Network] [OK] Wi-Fi Connected! IP: %s, RSSI: %d dBm\n", ip_str, WiFi.RSSI());
            retry_count = 0;
            // 联网成功后自动启动 NTP 校时
            syncNTP();
            break;
        }
        case WIFI_STATE_CONNECTING:
            Serial.println("[Network] Connecting to Wi-Fi...");
            break;
        case WIFI_STATE_FAILED:
            Serial.println("[Network] Wi-Fi connection timed out or failed.");
            break;
        case WIFI_STATE_DISCONNECTED:
            Serial.println("[Network] Wi-Fi disconnected.");
            break;
    }
}

bool NetworkManager::syncNTP() {
    Serial.println("[NTP] Synchronizing time with NTP servers...");

    // 使用多个备用 NTP 服务器，时区设为 UTC+0（大阳位置计算不需要本地时区）
    configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");

    // 等待 NTP 同步，最多等 8 秒
    struct tm timeinfo;
    uint32_t wait_start = millis();
    bool got_time = false;
    while (millis() - wait_start < 8000) {
        if (getLocalTime(&timeinfo, 100)) {
            got_time = true;
            break;
        }
        delay(100);
    }

    if (got_time) {
        time_t now_utc = 0;
        time(&now_utc);
        synced_utc_time = (uint32_t)now_utc;
        time_synced = true;
        Serial.printf("[NTP] Time synced! UTC: %lu (%04d-%02d-%02d %02d:%02d:%02d)\n",
                      (unsigned long)now_utc,
                      timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        return true;
    } else {
        Serial.println("[NTP] Time sync FAILED (timeout).");
        return false;
    }
}

bool NetworkManager::fetchGeoLocation(float* out_lat, float* out_lon, int32_t* out_tz_offset_sec) {
    if (!isConnected()) return false;

    Serial.println("[GeoIP] Fetching location from ip-api.com...");
    HTTPClient http;
    http.begin("http://ip-api.com/json/?fields=lat,lon,offset");
    http.setTimeout(5000);
    int code = http.GET();

    if (code == 200) {
        String body = http.getString();
        http.end();
        Serial.printf("[GeoIP] Response: %s\n", body.c_str());

        // 简单解析 JSON
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (!err) {
            float lat = doc["lat"] | 0.0f;
            float lon = doc["lon"] | 0.0f;
            int32_t offset = doc["offset"] | 0;
            if (out_lat) *out_lat = lat;
            if (out_lon) *out_lon = lon;
            if (out_tz_offset_sec) *out_tz_offset_sec = offset;
            tz_offset_sec = offset;
            Serial.printf("[GeoIP] Location: %.4f, %.4f  UTC%+.1f\n",
                          lat, lon, offset / 3600.0f);
            return true;
        } else {
            Serial.printf("[GeoIP] JSON parse error: %s\n", err.c_str());
        }
    } else {
        http.end();
        Serial.printf("[GeoIP] HTTP error: %d\n", code);
    }
    return false;
}

void NetworkManager::update(uint32_t now_ms) {
    // 异步扫描结果跟踪
    if (is_scanning) {
        isScanCompleted();
    }

    wl_status_t status = WiFi.status();

    if (current_state == WIFI_STATE_CONNECTING) {
        if (status == WL_CONNECTED) {
            handleStateTransition(WIFI_STATE_CONNECTED);
        } else if (now_ms - connect_start_ms > WIFI_CONNECT_TIMEOUT_MS) {
            handleStateTransition(WIFI_STATE_FAILED);
            last_retry_ms = now_ms;
            retry_count++;
        }
    } else if (current_state == WIFI_STATE_CONNECTED) {
        if (status != WL_CONNECTED) {
            Serial.println("[Network] Wi-Fi link lost!");
            handleStateTransition(WIFI_STATE_DISCONNECTED);
            last_retry_ms = now_ms;
        }
    } else if (current_state == WIFI_STATE_FAILED || current_state == WIFI_STATE_DISCONNECTED) {
        // 如果有保存的配置且在后台重试周期内，自动尝试重连（最多重试2次避免循环占用）
        if (strlen(target_ssid) > 0 && retry_count < 2 && (now_ms - last_retry_ms > WIFI_RETRY_INTERVAL_MS)) {
            Serial.printf("[Network] Retrying connection to '%s' (Attempt %d)...\n", target_ssid, retry_count + 1);
            connect(target_ssid, target_password);
        }
    }
}

