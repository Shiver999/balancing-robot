#pragma once

// Logging is not under test; consume arguments to preserve compiler checks.
template <typename... Args>
inline void hostLog(const char*, const char*, Args...) {}
#define ESP_LOGW(...) hostLog(__VA_ARGS__)
