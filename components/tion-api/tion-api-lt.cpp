#include <cmath>

#include "log.h"
#include "utils.h"
#include "tion-api-lt.h"

namespace dentra {
namespace tion {

static const char *const TAG = "tion-api-lt";

enum {
  FRAME_TYPE_STATE_SET = 0x1230,  // set no save req
  FRAME_TYPE_STATE_RSP = 0x1231,
  FRAME_TYPE_STATE_REQ = 0x1232,
  FRAME_TYPE_STATE_SAV = 0x1234,  // set save req

  FRAME_TYPE_DEV_STATUS_REQ = 0x4009,
  FRAME_TYPE_DEV_STATUS_RSP = 0x400A,

  FRAME_TYPE_AUTOKIV_PARAM_SET = 0x1240,
  FRAME_TYPE_AUTOKIV_PARAM_RSP = 0x1241,
  FRAME_TYPE_AUTOKIV_PARAM_REQ = 0x1242,
};

#pragma pack(push, 1)

// used to change state of device
struct tionlt_mode_t {
  struct {
    bool power_state : 1;
    bool sound_state : 1;
    bool led_state : 1;
    uint8_t auto_co2 : 1;
    uint8_t heater_state : 1;
    uint8_t last_com_source : 1;  // или last_com_source, или save
    bool factory_reset : 1;
    bool error_reset : 1;
    bool filter_reset : 1;
    // uint8_t save;
    uint8_t reserved : 7;
  };
  uint8_t gate_position;
  int8_t target_temperature;
  uint8_t fan_speed;
  _button_presets button_presets;
  uint16_t filter_time;
  uint8_t test_type;

  static tionlt_mode_t create(const tionlt_state_t &state) {
    tionlt_mode_t mode{};

    mode.filter_time = state.counters.filter_time;

    mode.fan_speed = state.system.fan_speed;
    mode.gate_position = state.system.gate_position;
    mode.target_temperature = state.system.target_temperature;

    mode.power_state = state.system.power_state;
    mode.sound_state = state.system.sound_state;
    mode.led_state = state.system.led_state;
    mode.heater_state = state.system.heater_state;

    mode.test_type = state.test_type;
    mode.button_presets = state.button_presets;

    return mode;
  }
};

#pragma pack(pop)

float tionlt_state_t::heater_power() const {
  switch (system.heater_present) {
    case 0:  // TODO change to enum
      return 0.0f;
    case 1:
      return heater_var * (0.01f * 1000.0f);
    default:
      TION_LOGW(TAG, "unknown heater_present value %u", system.heater_present);
      return NAN;
  }
}

void TionApiLt::read_(uint16_t frame_type, const void *frame_data, uint16_t frame_data_size) {
  TION_LOGV(TAG, "read frame data 0x%04X: %s", frame_type, hexencode(frame_data, frame_data_size).c_str());
  // do not use switch statement with non-contiguous values, as this will generate a lookup table with wasted space.
  if (false) {
    // just pretty formatting
  } else if (frame_type == FRAME_TYPE_STATE_RSP) {
    struct state_frame_t {
      uint32_t request_id;
      tionlt_state_t state;
    } PACKED;
    if (frame_data_size != sizeof(state_frame_t)) {
      TION_LOGW(TAG, "Incorrect state response data size: %u", frame_data_size);
    } else {
      this->read(static_cast<const state_frame_t *>(frame_data)->state);
    }
  } else if (frame_type == FRAME_TYPE_DEV_STATUS_RSP) {
    if (frame_data_size != sizeof(tion_dev_status_t)) {
      TION_LOGW(TAG, "Incorrect device status response data size: %u", frame_data_size);
    } else {
      this->read(*static_cast<const tion_dev_status_t *>(frame_data));
    }
  } else if (frame_type == FRAME_TYPE_AUTOKIV_PARAM_RSP) {
    TION_LOGW(TAG, "FRAME_TYPE_AUTOKIV_PARAM_RSP response: %s", hexencode(frame_data, frame_data_size).c_str());
  } else {
    TION_LOGW(TAG, "Unsupported frame type 0x%04X: %s", frame_type, hexencode(frame_data, frame_data_size).c_str());
  }
}

void TionApiLt::request_dev_status() {
  TION_LOGD(TAG, "Request device status");
  this->write_(FRAME_TYPE_DEV_STATUS_REQ);
}

void TionApiLt::request_state() {
  TION_LOGD(TAG, "Request state");
  this->write_(FRAME_TYPE_STATE_REQ);
}

bool TionApiLt::write_state(const tionlt_state_t &state) const {
  TION_LOGD(TAG, "Write state");
  auto mode = tionlt_mode_t::create(state);
  if (mode.filter_time == 0) {
    TION_LOGW(TAG, "State is not initialized");
    return false;
  }
  return this->write_(FRAME_TYPE_STATE_SET, mode, this->next_command_id());
}

bool TionApiLt::reset_filter(const tionlt_state_t &state) const {
  TION_LOGD(TAG, "Reset filter");
  auto mode = tionlt_mode_t::create(state);
  if (mode.filter_time == 0) {
    TION_LOGW(TAG, "State is not initialized");
    return false;
  }
  mode.filter_reset = true;
  mode.filter_time = 0;
  return this->write_(FRAME_TYPE_STATE_SET, mode, this->next_command_id());
}

bool TionApiLt::factory_reset(const tionlt_state_t &state) const {
  TION_LOGD(TAG, "Factory reset");
  auto mode = tionlt_mode_t::create(state);
  mode.factory_reset = true;
  return this->write_(FRAME_TYPE_STATE_SET, mode, this->next_command_id());
}

}  // namespace tion
}  // namespace dentra
