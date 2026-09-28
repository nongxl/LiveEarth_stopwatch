#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <vector>

enum WifiState {
    WIFI_STATE_DISCONNECTED = 0,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_FAILED
};

struct ScannedWifiItem {
    char ssid[33];
    int32_t rssi;
    bool is_open;
};

class NetworkManager {
public:
    NetworkManager();

    void init();
    void update(uint32_t now_ms);

    void connect(const char* ssid, const char* password);
    void disconnect();

    // NVS 配置持久化
    bool hasSavedConfig();
    bool loadConfig(char* ssid, size_t ssid_len, char* password, size_t pass_len);
    bool saveConfig(const char* ssid, const char* password);
    void clearConfig();
    bool connectSaved();

    // 扫描网络
    void startScan(bool async = true);
    bool isScanCompleted();
    int getScannedCount();
    const std::vector<ScannedWifiItem>& getScannedList() const { return scanned_networks; }

    bool isConnected() const { return current_state == WIFI_STATE_CONNECTED; }
    bool isConnecting() const { return current_state == WIFI_STATE_CONNECTING; }
    bool isFailed() const { return current_state == WIFI_STATE_FAILED; }
    WifiState getState() const { return current_state; }
    int8_t getRSSI() const;
    const char* getIP() const { return ip_str; }
    const char* getSSID() const { return target_ssid; }

    // 重置连接失败计数/状态
    void resetFailState();

private:
    WifiState current_state;
    char target_ssid[33];
    char target_password[65];
    char ip_str[16];

    uint32_t connect_start_ms;
    uint32_t last_retry_ms;
    int retry_count;

    std::vector<ScannedWifiItem> scanned_networks;
    bool is_scanning;

    void handleStateTransition(WifiState new_state);
};

extern NetworkManager network_mgr;

