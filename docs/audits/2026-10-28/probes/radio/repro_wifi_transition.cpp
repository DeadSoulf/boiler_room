// Diagnostic reproduction: success means the known faulty behavior was observed.
#include "panel_radio.cpp"
#include <cassert>
#include <cstddef>
#include <iostream>

using namespace panel;
using namespace panel_radio;

// The Preferences capture follows this exact on-flash Credentials layout.
static_assert(offsetof(Credentials, ssid) == sizeof(uint32_t), "Update the credential capture for a changed layout");

void reset() {
    state = RadioState{};
    state.ready = true;
    pendingCommand = false;
    urgent = Job::None;
    WiFi = Wifi{};
    prefs = Preferences{};
    nvsOK = true;
    wifiStarted = true;
    haveCredentials = false;
    recoveryPending = false;
    fakeMillis = 0;
    lastDisconnectReason = 0;
    delayHook = nullptr;
}

int main() {
    assert(begin());
    reset();
    WiFi.connected = true;
    WiFi.disconnectSuccess = false;
    jobState(Job::Disconnect, true);
    disconnectWifi(false);
    jobState(Job::None, false);
    updateConnection();
    std::cout << "disconnect_failure: actual_connected=" << WiFi.connected
              << " ui_connected=" << state.connected << " message=" << state.wifiMessage << "\n";
    assert(state.connected && strstr(state.wifiMessage, "Wi-Fi отключён"));

    reset();
    WiFi.connected = true;
    WiFi.disconnectSuccess = false;
    WiFi.beginFailure = true;
    Command requested;
    requested.job = Job::Connect;
    strcpy(requested.ssid, "requested-new-network");
    strcpy(requested.password, "12345678"); // Synthetic fixture, not a real password.
    jobState(Job::Connect, true);
    connectWifi(requested, true);
    jobState(Job::None, false);
    updateConnection();
    std::cout << "switch_failure: requested_ssid=" << requested.ssid
              << " actual_ssid=" << WiFi.SSID() << " credential_writes=" << prefs.credentialWrites
              << " saved_ssid=" << prefs.lastCredentialSsid << " elapsed_ms=" << fakeMillis
              << " message=" << state.wifiMessage << "\n";
    assert(WiFi.SSID() != requested.ssid && prefs.credentialWrites == 1);
    assert(prefs.lastCredentialSsid == requested.ssid && fakeMillis == 0);
    assert(strstr(state.wifiMessage, "Подключено."));
    vSemaphoreDelete(stateMutex);
    stateMutex = nullptr;
    std::cout << "Дефект воспроизведён: сбой отключения и перехода на новую сеть выдан за успех.\n";
}
