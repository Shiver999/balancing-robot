#pragma once

// Host-only stand-in for ESP-IDF error constants; never on the target include path.
// Match only constants needed by the real sources and host fixtures; not a full IDF API.
using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0;
constexpr esp_err_t ESP_FAIL = -1;
constexpr esp_err_t ESP_ERR_INVALID_ARG = 0x102;
constexpr esp_err_t ESP_ERR_INVALID_STATE = 0x103;
constexpr esp_err_t ESP_ERR_NOT_SUPPORTED = 0x106;
constexpr esp_err_t ESP_ERR_INVALID_VERSION = 0x10A;
constexpr esp_err_t ESP_ERR_INVALID_RESPONSE = 0x108;
constexpr esp_err_t ESP_ERR_NOT_FINISHED = 0x10C;
constexpr esp_err_t ESP_ERR_TIMEOUT = 0x107;
