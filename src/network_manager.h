#pragma once

#include <Arduino.h>
#include <WiFi.h>

enum WifiState {
    WIFI_STATE_DISCONNECTED = 0,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_FAILED
};

class NetworkManager {
public:
    NetworkManager();

    void init();
    void update(uint32_t now_ms);

    void connect(const char* ssid, const char* password);
    void disconnect();

    bool isConnected() const { return current_state == WIFI_STATE_CONNECTED; }
    WifiState getState() const { return current_state; }
    int8_t getRSSI() const;
    const char* getIP() const { return ip_str; }
    const char* getSSID() const { return target_ssid; }

private:
    WifiState current_state;
    char target_ssid[33];
    char target_password[65];
    char ip_str[16];

    uint32_t connect_start_ms;
    uint32_t last_retry_ms;
    int retry_count;

    void handleStateTransition(WifiState new_state);
};

extern NetworkManager network_mgr;
