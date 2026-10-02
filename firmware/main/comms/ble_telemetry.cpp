#include "sdkconfig.h"
#include "robot/comms/ble_telemetry.hpp"
#if CONFIG_ROBOT_BLE_TELEMETRY
#include "robot/comms/telemetry.hpp"
#include "esp_log.h"
#include <atomic>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
extern "C" {
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
}
namespace robot {
namespace {
constexpr char tag[] = "ble_telemetry";
// NimBLE stores UUID octets least significant first, opposite their printed form.
const ble_uuid128_t service = BLE_UUID128_INIT(0x00,0x01,0x0a,0x6b,0x2e,0x5b,0x2a,0x9a,0x6c,0x4f,0x15,0x1b,0x01,0x00,0x51,0x7f);
const ble_uuid128_t status = BLE_UUID128_INIT(0x00,0x01,0x0a,0x6b,0x2e,0x5b,0x2a,0x9a,0x6c,0x4f,0x15,0x1b,0x03,0x00,0x51,0x7f);
portMUX_TYPE snapshot_lock = portMUX_INITIALIZER_UNLOCKED;
protocol::TelemetryPacket latest = protocol::encodeTelemetry(0, 0, {}, {});
std::uint16_t sequence = 0, value_handle = 0;
std::uint8_t address_type = 0;
bool started = false;
std::atomic<bool> host_ready{false};

// Copy under a short critical section; never acquire the sensor bus in a BLE callback.
int access(std::uint16_t, std::uint16_t, ble_gatt_access_ctxt* ctx, void*) {
    if (ctx->op != BLE_GATT_ACCESS_OP_READ_CHR) return BLE_ATT_ERR_READ_NOT_PERMITTED;
    protocol::TelemetryPacket packet;
    portENTER_CRITICAL(&snapshot_lock); packet = latest; portEXIT_CRITICAL(&snapshot_lock);
    return os_mbuf_append(ctx->om, packet.data(), packet.size()) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}
ble_gatt_chr_def characteristics[2]{};
ble_gatt_svc_def services[2]{};
int gapEvent(ble_gap_event* event, void*);
void advertise() {
    ble_hs_adv_fields fields{};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = const_cast<ble_uuid128_t*>(&service);
    fields.num_uuids128 = 1; fields.uuids128_is_complete = 1;
    int rc = ble_gap_adv_set_fields(&fields);
    // Name goes in scan response; the 128-bit service already occupies advertising space.
    ble_hs_adv_fields response{};
    const char name[] = "Balancing Robot";
    response.name = reinterpret_cast<const std::uint8_t*>(name);
    response.name_len = sizeof(name) - 1; response.name_is_complete = 1;
    if (rc == 0) rc = ble_gap_adv_rsp_set_fields(&response);
    ble_gap_adv_params params{};
    params.conn_mode = BLE_GAP_CONN_MODE_UND; params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    if (rc == 0) rc = ble_gap_adv_start(address_type, nullptr, BLE_HS_FOREVER, &params, gapEvent, nullptr);
    if (rc != 0) ESP_LOGE(tag, "Advertising failed: %d", rc);
}
int gapEvent(ble_gap_event* event, void*) {
    if ((event->type == BLE_GAP_EVENT_CONNECT && event->connect.status != 0) ||
        event->type == BLE_GAP_EVENT_DISCONNECT || event->type == BLE_GAP_EVENT_ADV_COMPLETE) advertise();
    return 0;
}
void synced() {
    if (ble_hs_util_ensure_addr(0) == 0 && ble_hs_id_infer_auto(0, &address_type) == 0) { host_ready.store(true); advertise(); }
    else ESP_LOGE(tag, "BLE address unavailable");
}
void reset(int reason) { host_ready.store(false); ESP_LOGW(tag, "BLE host reset: %d", reason); }
void hostTask(void*) { nimble_port_run(); nimble_port_freertos_deinit(); }
}
esp_err_t startBleTelemetry() {
    if (started) return ESP_ERR_INVALID_STATE;
    // Do not erase persistent storage automatically when initialization fails.
    esp_err_t result = nvs_flash_init();
    if (result != ESP_OK) return result;
    result = nimble_port_init(); if (result != ESP_OK) return result;
    ble_svc_gap_init(); ble_svc_gatt_init();
    characteristics[0].uuid = &status.u;
    characteristics[0].access_cb = access;
    characteristics[0].flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY;
    characteristics[0].val_handle = &value_handle;
    services[0].type = BLE_GATT_SVC_TYPE_PRIMARY;
    services[0].uuid = &service.u; services[0].characteristics = characteristics;
    if (ble_gatts_count_cfg(services) != 0 || ble_gatts_add_svcs(services) != 0 ||
        ble_svc_gap_device_name_set("Balancing Robot") != 0) {
        (void)nimble_port_deinit();
        return ESP_FAIL;
    }
    ble_att_set_preferred_mtu(96);
    ble_hs_cfg.sync_cb = synced;
    ble_hs_cfg.reset_cb = reset;
    started = true;
    nimble_port_freertos_init(hostTask);
    ESP_LOGI(tag, "Telemetry-only BLE started; motor writes unavailable");
    return ESP_OK;
}
void publishBleTelemetry(std::int64_t timestamp_us, const ImuSample& imu, const WheelMeasurement& wheels) {
    if (!started) return;
    const auto packet = protocol::encodeTelemetry(sequence++, timestamp_us, imu, wheels);
    portENTER_CRITICAL(&snapshot_lock); latest = packet; portEXIT_CRITICAL(&snapshot_lock);
    // NimBLE schedules notifications on its host task; callbacks consume one coherent snapshot.
    if (host_ready.load()) ble_gatts_chr_updated(value_handle);
}
}
#else
namespace robot {
esp_err_t startBleTelemetry() { return ESP_ERR_NOT_SUPPORTED; }
void publishBleTelemetry(std::int64_t, const ImuSample&, const WheelMeasurement&) {}
}
#endif
