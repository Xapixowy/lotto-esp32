#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_timer.h>
#include <time.h>
#include "Controller.h"
#include "Payload.h"
#include "Display.h"
#include "BoundedBody.h"
#if __has_include("Secrets.h")
#include "Secrets.h"
#else
#include "Secrets.example.h"
#endif

Display display;
lotto::Controller controller;
SemaphoreHandle_t responseMutex;
bool responsePending = false;
lotto::Status receivedStatus = lotto::Status::Fetching;
lotto::Snapshot receivedSnapshot;
bool requestRunning = false;
bool wasConnected = false;

std::uint64_t clockMillis() {
    return static_cast<std::uint64_t>(esp_timer_get_time() / 1000);
}

void requestResults(void*) {
    lotto::Snapshot snapshot;
    lotto::Status status = lotto::Status::BackendUnreachable;
    String url(BACKEND_URL);
    if (WiFi.status() != WL_CONNECTED) status = lotto::Status::WifiDisconnected;
    else if (url.startsWith("https://") && time(nullptr) > 1700000000) {
        WiFiClientSecure client;
        client.setCACert(BACKEND_ROOT_CA);
        client.setHandshakeTimeout(5);
        client.setTimeout(5000);
        HTTPClient http;
        http.setConnectTimeout(5000);
        http.setTimeout(5000);
        http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
        if (http.begin(client, url)) {
            http.addHeader("Authorization", String("Bearer ") + API_TOKEN);
            http.addHeader("Accept", "application/json");
            const int code = http.GET();
            if (code == 401 || code == 403) status = lotto::Status::AccessDenied;
            else if (code >= 500) status = lotto::Status::BackendUnavailable;
            else if (code > 0) status = lotto::Status::InvalidResponse;
            const int length = http.getSize();
            if ((code == 200 || code == 503) && (length == -1 || (length > 0 && length <= 32768))) {
                BoundedBody body;
                JsonDocument json;
                if (http.writeToStream(&body) > 0 && !body.overflow && !deserializeJson(json, body.body)) {
                    status = lotto::parseResultsResponse(code, json.as<JsonVariantConst>(), snapshot);
                } else status = lotto::Status::InvalidResponse;
            } else if (code == 200) status = lotto::Status::InvalidResponse;
            http.end();
        }
    }
    xSemaphoreTake(responseMutex, portMAX_DELAY);
    receivedSnapshot = std::move(snapshot);
    receivedStatus = status;
    responsePending = true;
    xSemaphoreGive(responseMutex);
    vTaskDelete(nullptr);
}

void setup() {
    Serial.begin(115200);
    display.begin();
    responseMutex = xSemaphoreCreateMutex();
    if (DISPLAY_CHECK_ONLY) {
        lotto::Snapshot demo;
        demo.lottoFetchedAt = 1791201600;
        demo.serverTime = 1791201600;
        demo.lottoTime = "14:00:00";
        demo.syncTime = "14:00:00";
        demo.slides = {
            {"Lotto", "Lotto", {{"TEST EKRANU", "simple", {1,4,12,24,36,41}}}},
            {"Keno", "Keno", {{"Zażółć gęślą jaźń", "simple", {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20}}}},
        };
        controller.receive(demo, clockMillis());
        Serial.println("Display-check mode: no Wi-Fi or API requests. Touch raw values appear here.");
    } else {
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
    }
}

void loop() {
    const auto now = clockMillis();
    if (!DISPLAY_CHECK_ONLY) {
        const bool connected = WiFi.status() == WL_CONNECTED;
        if (connected && !wasConnected) controller.connectionRestored();
        wasConnected = connected;
        if (requestRunning && xSemaphoreTake(responseMutex, 0) == pdTRUE) {
            if (responsePending) {
                if (receivedStatus == lotto::Status::Ready && connected) controller.receive(receivedSnapshot, now);
                else controller.fail(receivedStatus, now);
                responsePending = false;
                requestRunning = false;
            }
            xSemaphoreGive(responseMutex);
        }
        if (!connected) controller.fail(lotto::Status::WifiDisconnected, now);
        if (!requestRunning && connected && controller.pollDue(now)) {
            controller.beginPoll(now);
            requestRunning = true;
            if (xTaskCreate(requestResults, "lotto-poll", 12288, nullptr, 1, nullptr) != pdPASS) {
                requestRunning = false;
                controller.fail(lotto::Status::BackendUnavailable, now);
            }
        }
    }
    controller.tick(now);
    lotto::Touch action;
    if (display.readTouch(action, DISPLAY_CHECK_ONLY)) controller.touch(action, now);
    display.draw(controller.view(now));
    delay(20);
}
