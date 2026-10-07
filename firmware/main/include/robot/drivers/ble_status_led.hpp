#pragma once
#include "esp_err.h"
namespace robot {
// Start a single LED worker. BLE callbacks only update its desired connection state.
esp_err_t startBleStatusLed();
void setBleStatusConnected(bool connected);
}
