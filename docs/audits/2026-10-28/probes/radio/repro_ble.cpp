// The exact upstream blocking wrapper is included by the local BLEDevice stub.
#include "panel_radio.cpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace panel_radio;
    assert(begin());
    auto* scanner = BLEDevice::getScan();
    scanner->startSucceeded = false;
    scanBle();
    std::cout << "start_called=" << scanner->startCalls
              << " actual_start_success=" << scanner->startSucceeded << "\n";
    std::cout << "ble_count=" << unsigned(state.bleCount) << " message=" << state.bleMessage << "\n";
    assert(scanner->startCalls == 1 && !scanner->startSucceeded);
    assert(state.bleCount == 0 && strstr(state.bleMessage, "Поиск завершён"));
    vSemaphoreDelete(stateMutex);
    stateMutex = nullptr;
    std::cout << "Дефект воспроизведён: отказ запуска BLE-сканирования выдан за завершённый поиск.\n";
}
