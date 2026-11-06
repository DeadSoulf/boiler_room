#pragma once
// Host-only error identities for the ESP-IDF Wi-Fi adapter tests.
using esp_err_t=int;
constexpr esp_err_t ESP_OK=0, ESP_FAIL=-1, ESP_ERR_NO_MEM=0x101, ESP_ERR_TIMEOUT=0x107;
constexpr esp_err_t ESP_ERR_WIFI_NOT_CONNECT=0x300f, ESP_ERR_WIFI_MODE=0x3005;
constexpr esp_err_t ESP_ERR_WIFI_STATE=0x3006, ESP_ERR_WIFI_TIMEOUT=0x300c;
inline const char* esp_err_to_name(esp_err_t error) {
 switch(error) {
  case ESP_OK:return "ESP_OK";
  case ESP_FAIL:return "ESP_FAIL";
  case ESP_ERR_NO_MEM:return "ESP_ERR_NO_MEM";
  case ESP_ERR_TIMEOUT:return "ESP_ERR_TIMEOUT";
  case ESP_ERR_WIFI_NOT_CONNECT:return "ESP_ERR_WIFI_NOT_CONNECT";
  case ESP_ERR_WIFI_MODE:return "ESP_ERR_WIFI_MODE";
  case ESP_ERR_WIFI_STATE:return "ESP_ERR_WIFI_STATE";
  case ESP_ERR_WIFI_TIMEOUT:return "ESP_ERR_WIFI_TIMEOUT";
  default:return "UNKNOWN ERROR";
 }
}
