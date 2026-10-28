// Diagnostic reproduction: immediate begin failure is mislabeled as a timeout.
#include "panel_radio.cpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace panel;
    using namespace panel_radio;
    assert(begin());
    state.ready = true;
    nvsOK = true;
    wifiStarted = true;
    WiFi.beginFailure = true;
    Command requested;
    requested.job = Job::Connect;
    strcpy(requested.ssid, "requested-new-network");
    strcpy(requested.password, "12345678"); // Synthetic fixture, not a real password.
    connectWifi(requested, true);
    std::cout << "begin_failed_without_old_connection: begin_calls=" << WiFi.beginCount
              << " connected=" << WiFi.connected << " credential_writes=" << prefs.credentialWrites
              << " elapsed_ms=" << fakeMillis << " message=" << state.wifiMessage << "\n";
    assert(WiFi.beginCount == 1 && fakeMillis == 20000 && prefs.credentialWrites == 0);
    assert(!WiFi.connected && !state.connected);
    assert(strstr(state.wifiMessage, "Таймаут Wi-Fi"));
    vSemaphoreDelete(stateMutex);
    stateMutex = nullptr;
    std::cout << "Дефект воспроизведён: немедленный отказ begin выдан за таймаут; credentials не записаны.\n";
}
