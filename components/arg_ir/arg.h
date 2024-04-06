#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace arg {

// Support for ARG air conditioners with KKG26A-C1 remote

// Temperature
const float ARG_TEMP_MIN = 16.0;
const float ARG_TEMP_MAX = 32.0;

class ArgClimate : public climate_ir::ClimateIR {
 public:
  ArgClimate()
      : climate_ir::ClimateIR(ARG_TEMP_MIN, ARG_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                               climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH}) {}

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  /// Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
};

}  // namespace arg
}  // namespace esphome
