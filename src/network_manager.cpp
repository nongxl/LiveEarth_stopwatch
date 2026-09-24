#include "network_manager.h"
#include "config.h"

NetworkManager network_mgr;

NetworkManager::NetworkManager()
    : current_state(WIFI_STATE_DISCONNECTED),
      connect_start_ms(0),
      last_retry_ms(0),
      retry_count(0)
{
    target_ssid[0] = '\0';
    target_password[0] = '\0';
    strcpy(ip_str, "0.0.0.0");
}

void NetworkManager::init() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(50);

    // 如果在 config.h 中定义了默认 SSID 且非空，则发起初始连接
    if (strlen(WIFI_SSID) > 0) {
        connect(WIFI_SSID, WIFI_PASSWORD);
    } else {
        Serial.println("[Network] No default Wi-Fi SSID configured. Running in offline-first mode.");
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
    handleStateTransition(WIFI_STATE_CONNECTING);
}

void NetworkManager::disconnect() {
    WiFi.disconnect(true);
    handleStateTransition(WIFI_STATE_DISCONNECTED);
    strcpy(ip_str, "0.0.0.0");
}

int8_t NetworkManager::getRSSI() const {
    if (current_state == WIFI_STATE_CONNECTED) {
        return WiFi.RSSI();
    }
    return -100;
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

void NetworkManager::update(uint32_t now_ms) {
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
        // 后台定时自动尝试重连
        if (strlen(target_ssid) > 0 && (now_ms - last_retry_ms > WIFI_RETRY_INTERVAL_MS)) {
            Serial.printf("[Network] Retrying connection to '%s' (Attempt %d)...\n", target_ssid, retry_count + 1);
            connect(target_ssid, target_password);
        }
    }
}
