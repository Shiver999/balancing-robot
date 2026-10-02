#pragma once

// Logging is not under test; consume arguments to preserve compiler checks.
template <typename... Args>
inline void hostLog(const char*, const char*, Args...) {}
// Route warnings to a typed no-op so startup code links without target logging.
#define ESP_LOGW(...) hostLog(__VA_ARGS__)
