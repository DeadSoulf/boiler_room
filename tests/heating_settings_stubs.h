#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

namespace heating_fixture {
inline std::mutex lock;
inline std::vector<uint8_t> blob;
inline bool space = false, blockLock = false, unreadable = false;
inline bool failCommit = false, damageNextVerification = false, damageRead = false;
inline unsigned writes = 0;
}
using SemaphoreHandle_t = std::mutex*;
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return &heating_fixture::lock; }
inline int pdMS_TO_TICKS(int ms) { return ms; }
constexpr int pdTRUE = 1;
inline int xSemaphoreTake(SemaphoreHandle_t lock, int) {
  if (heating_fixture::blockLock) return 0;
  lock->lock(); return pdTRUE;
}
inline void xSemaphoreGive(SemaphoreHandle_t lock) { lock->unlock(); }

using nvs_handle_t = int;
using esp_err_t = int;
constexpr int NVS_READONLY = 1, NVS_READWRITE = 2;
constexpr int ESP_OK = 0, ESP_FAIL = -1, ESP_ERR_NVS_NOT_FOUND = 0x1102;
inline int nvs_open(const char* space, int mode, nvs_handle_t* handle) {
  namespace f = heating_fixture;
  assert(std::strcmp(space, "bp_heating") == 0);
  if (mode == NVS_READONLY && !f::space) return ESP_ERR_NVS_NOT_FOUND;
  if (mode == NVS_READWRITE) f::space = true;
  *handle = mode;
  return ESP_OK;
}
inline int nvs_get_blob(nvs_handle_t mode, const char* key, void* dest, size_t* n) {
  namespace f = heating_fixture;
  assert(mode == NVS_READONLY && !std::strcmp(key, "settings"));
  if (f::unreadable) return ESP_FAIL;
  if (f::blob.empty()) return ESP_ERR_NVS_NOT_FOUND;
  if (*n < f::blob.size()) return ESP_FAIL;
  *n = f::blob.size();
  std::memcpy(dest, f::blob.data(), *n);
  if (f::damageRead) {
    static_cast<uint8_t*>(dest)[0] ^= 1;
    f::damageRead = false;
  }
  return ESP_OK;
}
inline int nvs_set_blob(nvs_handle_t mode, const char* key, const void* data, size_t n) {
  namespace f = heating_fixture;
  assert(mode == NVS_READWRITE && !std::strcmp(key, "settings"));
  const auto* bytes = static_cast<const uint8_t*>(data);
  f::blob.assign(bytes, bytes+n);
  ++f::writes;
  f::damageRead = f::damageNextVerification; f::damageNextVerification = false;
  return ESP_OK;
}
inline int nvs_erase_key(nvs_handle_t mode, const char* key) {
  assert(mode == NVS_READWRITE && !std::strcmp(key, "settings"));
  if (heating_fixture::blob.empty()) return ESP_ERR_NVS_NOT_FOUND;
  heating_fixture::blob.clear();
  return ESP_OK;
}
inline int nvs_commit(nvs_handle_t mode) {
  assert(mode == NVS_READWRITE);
  if (heating_fixture::failCommit) { heating_fixture::failCommit = false; return ESP_FAIL; }
  return ESP_OK;
}
inline void nvs_close(nvs_handle_t) {}
