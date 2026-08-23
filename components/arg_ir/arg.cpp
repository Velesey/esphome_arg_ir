#include "arg.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cstdint>

namespace esphome {
namespace arg {

static const char *const TAG = "arg.climate";

static constexpr uint16_t ARG_HEADER_MARK = 8328;
static constexpr uint16_t ARG_HEADER_SPACE = 4176;
static constexpr uint16_t ARG_BIT_MARK = 515;
static constexpr uint16_t ARG_ONE_SPACE = 1592;
static constexpr uint16_t ARG_ZERO_SPACE = 523;

static constexpr uint32_t ARG_CARRIER_FREQUENCY = 38000;

static constexpr uint8_t ARG_STATE_LENGTH = 15;
static constexpr uint8_t ARG_HEADER = 0x56;
static constexpr uint8_t ARG_TEMPERATURE_SHIFT = 0x5C;

static constexpr uint8_t ARG_AUTO = 0b01000000;
static constexpr uint8_t ARG_COOL = 0b00100000;
static constexpr uint8_t ARG_DRY = 0b00110000;
static constexpr uint8_t ARG_HEAT = 0b00010000;
static constexpr uint8_t ARG_FAN = 0b01010000;
static constexpr uint8_t ARG_MODE_MASK = 0b11110000;

static constexpr uint8_t ARG_FAN_AUTO = 0b00000000;
static constexpr uint8_t ARG_FAN_HIGH = 0b00000001;
static constexpr uint8_t ARG_FAN_MED = 0b00000011;
static constexpr uint8_t ARG_FAN_LOW = 0b00000010;
static constexpr uint8_t ARG_FAN_MASK = 0b00001111;

static constexpr uint8_t ARG_SWING_VER = 0b00000010;
static constexpr uint8_t ARG_SWING_HOR = 0b00000001;
static constexpr uint8_t ARG_SWING_MASK = ARG_SWING_VER | ARG_SWING_HOR;
static constexpr uint8_t ARG_POWER_OFF = 0b11000000;
static constexpr uint8_t ARG_AUH = 0b01000000;
static constexpr uint8_t ARG_NIBBLE_MASK = 0b00001111;

void ArgClimate::transmit_state() {
  uint8_t remote_state[ARG_STATE_LENGTH] = {};

  remote_state[0] = ARG_HEADER;

  const auto temp = static_cast<uint8_t>(roundf(clamp(this->target_temperature, ARG_TEMP_MIN, ARG_TEMP_MAX)));
  remote_state[1] = temp + ARG_TEMPERATURE_SHIFT;

  // Fan speed
  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)) {
    case climate::CLIMATE_FAN_HIGH:
      remote_state[4] |= ARG_FAN_HIGH;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      remote_state[4] |= ARG_FAN_MED;
      break;
    case climate::CLIMATE_FAN_LOW:
      remote_state[4] |= ARG_FAN_LOW;
      break;
    case climate::CLIMATE_FAN_AUTO:
      remote_state[4] |= ARG_FAN_AUTO;
      break;
    default:
      break;
  }

  // Mode
  switch (this->mode) {
    case climate::CLIMATE_MODE_AUTO:
    case climate::CLIMATE_MODE_HEAT_COOL:
      remote_state[4] |= ARG_AUTO;
      break;
    case climate::CLIMATE_MODE_HEAT:
      remote_state[4] |= ARG_HEAT;
      remote_state[6] |= ARG_AUH;
      break;
    case climate::CLIMATE_MODE_COOL:
      remote_state[4] |= ARG_COOL;
      break;
    case climate::CLIMATE_MODE_DRY:
      remote_state[4] |= ARG_DRY;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      remote_state[4] |= ARG_FAN;
      break;
    case climate::CLIMATE_MODE_OFF:
      remote_state[5] |= ARG_POWER_OFF;
      remote_state[4] |= ARG_AUTO;
      break;
    default:
      remote_state[4] |= ARG_AUTO;
      break;
  }

  const bool swing_ver =
      this->swing_mode == climate::CLIMATE_SWING_VERTICAL || this->swing_mode == climate::CLIMATE_SWING_BOTH;
  const bool swing_hor =
      this->swing_mode == climate::CLIMATE_SWING_HORIZONTAL || this->swing_mode == climate::CLIMATE_SWING_BOTH;

  if (swing_ver)
    remote_state[5] |= ARG_SWING_VER;

  if (swing_hor)
    remote_state[5] |= ARG_SWING_HOR;

  uint8_t checksum = 0;
  for (uint8_t i = 0; i < ARG_STATE_LENGTH - 1; ++i) {
    checksum += remote_state[i] & ARG_NIBBLE_MASK;
    checksum += remote_state[i] >> 4;
  }

  remote_state[ARG_STATE_LENGTH - 1] = checksum;

  ESP_LOGV(TAG,
           "Sending: %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X %02X",
           remote_state[0], remote_state[1], remote_state[2], remote_state[3], remote_state[4], remote_state[5],
           remote_state[6], remote_state[7], remote_state[8], remote_state[9], remote_state[10], remote_state[11],
           remote_state[12], remote_state[13], remote_state[14]);

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(ARG_CARRIER_FREQUENCY);
  data->reserve(2 + ARG_STATE_LENGTH * 16 + 1);

  data->item(ARG_HEADER_MARK, ARG_HEADER_SPACE);
  for (uint8_t byte : remote_state) {
    for (uint8_t bit = 0; bit < 8; ++bit)
      data->item(ARG_BIT_MARK, byte & (1U << bit) ? ARG_ONE_SPACE : ARG_ZERO_SPACE);
  }
  data->mark(ARG_BIT_MARK);

  transmit.perform();
}

bool ArgClimate::on_receive(remote_base::RemoteReceiveData data) {
  if (!data.expect_item(ARG_HEADER_MARK, ARG_HEADER_SPACE)) {
    ESP_LOGV(TAG, "Header fail");
    return false;
  }

  uint8_t remote_state[ARG_STATE_LENGTH] = {};
  for (uint8_t byte = 0; byte < ARG_STATE_LENGTH; ++byte) {
    for (uint8_t bit = 0; bit < 8; ++bit) {
      if (data.expect_item(ARG_BIT_MARK, ARG_ONE_SPACE)) {
        remote_state[byte] |= 1U << bit;
      } else if (!data.expect_item(ARG_BIT_MARK, ARG_ZERO_SPACE)) {
        ESP_LOGV(TAG, "Byte %u bit %u fail", byte, bit);
        return false;
      }
    }
    ESP_LOGVV(TAG, "Byte %u %02X", byte, remote_state[byte]);
  }

  if (!data.expect_mark(ARG_BIT_MARK)) {
    ESP_LOGV(TAG, "Footer fail");
    return false;
  }

  uint8_t checksum = 0;
  for (uint8_t i = 0; i < ARG_STATE_LENGTH - 1; ++i) {
    checksum += remote_state[i] & ARG_NIBBLE_MASK;
    checksum += remote_state[i] >> 4;
  }
  if (checksum != remote_state[ARG_STATE_LENGTH - 1]) {
    ESP_LOGVV(TAG, "Checksum fail");
    return false;
  }

  ESP_LOGV(TAG,
           "Received: %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X %02X",
           remote_state[0], remote_state[1], remote_state[2], remote_state[3], remote_state[4], remote_state[5],
           remote_state[6], remote_state[7], remote_state[8], remote_state[9], remote_state[10], remote_state[11],
           remote_state[12], remote_state[13], remote_state[14]);

  if (remote_state[0] != ARG_HEADER) {
    ESP_LOGVV(TAG, "First byte wrong");
    return false;
  }

  ESP_LOGV(TAG, "Power: %02X", remote_state[5]);
  if ((remote_state[5] & ARG_POWER_OFF) == ARG_POWER_OFF) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    const auto mode = remote_state[4] & ARG_MODE_MASK;
    ESP_LOGV(TAG, "Mode: %02X", mode);
    switch (mode) {
      case ARG_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case ARG_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case ARG_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case ARG_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      case ARG_AUTO:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
      default:
        ESP_LOGV(TAG, "Unknown mode: %02X", mode);
        return false;
    }
  }

  const auto fan = remote_state[4] & ARG_FAN_MASK;
  ESP_LOGV(TAG, "Fan: %02X", fan);
  switch (fan) {
    case ARG_FAN_HIGH:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case ARG_FAN_MED:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case ARG_FAN_LOW:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case ARG_FAN_AUTO:
    default:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
  }

  const float temperature = static_cast<float>(remote_state[1] - ARG_TEMPERATURE_SHIFT);
  if (temperature < ARG_TEMP_MIN || temperature > ARG_TEMP_MAX) {
    ESP_LOGV(TAG, "Temperature out of range: %.0f", temperature);
    return false;
  }
  ESP_LOGV(TAG, "Target temperature: %.0f", temperature);
  this->target_temperature = temperature;

  const auto swing = remote_state[5] & ARG_SWING_MASK;
  ESP_LOGV(TAG, "Swing status: %02X", swing);
  if (swing == ARG_SWING_MASK) {
    this->swing_mode = climate::CLIMATE_SWING_BOTH;
  } else if ((swing & ARG_SWING_VER) != 0) {
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else if ((swing & ARG_SWING_HOR) != 0) {
    this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
  } else {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }

  this->publish_state();
  return true;
}

}  // namespace arg
}  // namespace esphome
