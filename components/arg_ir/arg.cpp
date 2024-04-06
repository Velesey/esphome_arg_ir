#include "arg.h"

#include "esphome/core/log.h"

#include "binary.h"

namespace esphome {
    namespace arg {

        static
        const char *
            const TAG = "arg.climate";

        const uint16_t ARG_HEADER_MARK = 8328;
        const uint16_t ARG_HEADER_SPACE = 4176;
        const uint16_t ARG_BIT_MARK = 515;
        const uint16_t ARG_ONE_SPACE = 1592;
        const uint16_t ARG_ZERO_SPACE = 523;

        const uint32_t ARG_CARRIER_FREQUENCY = 38000;

        const uint8_t ARG_STATE_LENGTH = 15;
        const uint8_t ARG_HEADER = 0x56;
        const uint8_t ARG_TEMPERATURE_SHIFT = 0x5C;

        const uint8_t ARG_AUTO = B01000000;
        const uint8_t ARG_COOL = B00100000;
        const uint8_t ARG_DRY = B00110000;
        const uint8_t ARG_HEAT = B00010000;
        const uint8_t ARG_FAN = B01010000;

        const uint8_t ARG_FAN_AUTO = B00000000;
        const uint8_t ARG_FAN_HIGH = B00000001;
        const uint8_t ARG_FAN_MED = B00000011;
        const uint8_t ARG_FAN_LOW = B00000010;

        const uint8_t ARG_SWING_VER = B00000010;
        const uint8_t ARG_SWING_HOR = B00000001;
        const uint8_t ARG_POWER_OFF = B11000000;
        const uint8_t ARG_AUH = B01000000;

        void ArgClimate::transmit_state() {

            uint8_t remote_state[ARG_STATE_LENGTH] = {
                0
            };

            for (int i = 0; i < ARG_STATE_LENGTH; ++i) {
                remote_state[i] = 0;
            }

            remote_state[0] = ARG_HEADER;

            auto temp = (uint8_t) roundf(clamp(this -> target_temperature, ARG_TEMP_MIN, ARG_TEMP_MAX));
            remote_state[1] = temp + ARG_TEMPERATURE_SHIFT;

            // Fan speed
            switch (this -> fan_mode.value()) {
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
            remote_state[5] = 0;
            switch (this -> mode) {
            case climate::CLIMATE_MODE_AUTO:
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
            default:
                remote_state[4] |= ARG_AUTO;
                break;
            }

            auto swing_ver =
                ((this -> swing_mode == climate::CLIMATE_SWING_VERTICAL) || (this -> swing_mode == climate::CLIMATE_SWING_BOTH));
            auto swing_hor =
                ((this -> swing_mode == climate::CLIMATE_SWING_HORIZONTAL) || (this -> swing_mode == climate::CLIMATE_SWING_BOTH));

            if (swing_ver)
                remote_state[5] |= ARG_SWING_VER;

            if (swing_hor)
                remote_state[5] |= ARG_SWING_HOR;

            uint8_t checksum = 0x00;
            for (int i = 0; i < ARG_STATE_LENGTH - 1; ++i) {
                checksum += remote_state[i] & B00001111;
                checksum += remote_state[i] >> 4;
            }

            remote_state[14] = checksum;

            ESP_LOGV(TAG, "Sending: %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X %02X", remote_state[0],
                remote_state[1], remote_state[2], remote_state[3], remote_state[4], remote_state[5], remote_state[6],
                remote_state[7], remote_state[8], remote_state[9], remote_state[10], remote_state[11], remote_state[12],
                remote_state[13], remote_state[14]);

            // Send code
            auto transmit = this -> transmitter_ -> transmit();
            auto * data = transmit.get_data();

            data -> set_carrier_frequency(ARG_CARRIER_FREQUENCY);

            // Header
            data -> mark(ARG_HEADER_MARK);
            data -> space(ARG_HEADER_SPACE);
            // Data
            for (uint8_t i: remote_state) {
                for (uint8_t j = 0; j < 8; j++) {
                    data -> mark(ARG_BIT_MARK);
                    bool bit = i & (1 << j);
                    data -> space(bit ? ARG_ONE_SPACE : ARG_ZERO_SPACE);
                }
            }
            // Footer
            data -> mark(ARG_BIT_MARK);

            transmit.perform();
        }

        bool ArgClimate::on_receive(remote_base::RemoteReceiveData data) {
            // Validate header
            if (!data.expect_item(ARG_HEADER_MARK, ARG_HEADER_SPACE)) {
                ESP_LOGV(TAG, "Header fail");
                return false;
            }

            uint8_t remote_state[ARG_STATE_LENGTH] = {
                0
            };
            // Read all bytes.
            for (int i = 0; i < ARG_STATE_LENGTH; i++) {
                // Read bit
                for (int j = 0; j < 8; j++) {
                    if (data.expect_item(ARG_BIT_MARK, ARG_ONE_SPACE)) {
                        remote_state[i] |= 1 << j;
                    } else if (!data.expect_item(ARG_BIT_MARK, ARG_ZERO_SPACE)) {
                        ESP_LOGV(TAG, "Byte %d bit %d fail", i, j);
                        return false;
                    }
                }

                ESP_LOGVV(TAG, "Byte %d %02X", i, remote_state[i]);
            }
            // Validate footer
            if (!data.expect_mark(ARG_BIT_MARK)) {
                ESP_LOGV(TAG, "Footer fail");
                return false;
            }

            uint8_t checksum = 0;
            for (int i = 0; i < ARG_STATE_LENGTH - 1; i++) {
                checksum += remote_state[i] & B00001111;
                checksum += remote_state[i] >> 4;
            }

            if (checksum != remote_state[ARG_STATE_LENGTH - 1]) {
                ESP_LOGVV(TAG, "Checksum fail");
                return false;
            }

            ESP_LOGV(TAG, "Received: %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X %02X", remote_state[0],
                remote_state[1], remote_state[2], remote_state[3], remote_state[4], remote_state[5], remote_state[6],
                remote_state[7], remote_state[8], remote_state[9], remote_state[10], remote_state[11], remote_state[12],
                remote_state[13], remote_state[14]);

            // verify header remote code
            if (remote_state[0] != ARG_HEADER) {
                ESP_LOGVV(TAG, "First byte wrong");
                return false;

            }

            // Mode
            ESP_LOGV(TAG, "Power: %02X", (remote_state[5]));
            if (remote_state[5] == ARG_POWER_OFF) {
                this -> mode = climate::CLIMATE_MODE_OFF;
            } else {
                auto mode = remote_state[4] & B11110000;
                ESP_LOGV(TAG, "Mode: %02X", mode);
                switch (mode) {
                case ARG_HEAT:
                    this -> mode = climate::CLIMATE_MODE_HEAT;
                    break;
                case ARG_COOL:
                    this -> mode = climate::CLIMATE_MODE_COOL;
                    break;
                case ARG_DRY:
                    this -> mode = climate::CLIMATE_MODE_DRY;
                    break;
                case ARG_FAN:
                    this -> mode = climate::CLIMATE_MODE_FAN_ONLY;
                    break;
                case ARG_AUTO:
                    this -> mode = climate::CLIMATE_MODE_AUTO;
                    break;
                }
            }

            // fan
            auto fan = remote_state[4] & B00001111;
            ESP_LOGV(TAG, "Fan: %02X", fan);
            switch (fan) {
            case ARG_FAN_HIGH:
                this -> fan_mode = climate::CLIMATE_FAN_HIGH;
                break;
            case ARG_FAN_MED:
                this -> fan_mode = climate::CLIMATE_FAN_MEDIUM;
                break;
            case ARG_FAN_LOW:
                this -> fan_mode = climate::CLIMATE_FAN_LOW;
                break;
            case ARG_FAN_AUTO:
            default:
                this -> fan_mode = climate::CLIMATE_FAN_AUTO;
                break;
            }

            // temperature
            int temp = remote_state[1] - ARG_TEMPERATURE_SHIFT;
            ESP_LOGV(TAG, "Temperature Climate: %u", temp);
            this -> target_temperature = temp;

            // swing 
            auto swingData = remote_state[5] & B00000011;
            ESP_LOGV(TAG, "Swing status: %02X", swingData);
            if (((swingData & ARG_SWING_VER) == ARG_SWING_VER) &&
                ((swingData & ARG_SWING_HOR) == ARG_SWING_HOR)) {
                this -> swing_mode = climate::CLIMATE_SWING_BOTH;
            } else if ((swingData & ARG_SWING_VER) == ARG_SWING_VER) {
                this -> swing_mode = climate::CLIMATE_SWING_VERTICAL;
            } else if ((swingData & ARG_SWING_HOR) == ARG_SWING_HOR) {
                this -> swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
            } else {
                this -> swing_mode = climate::CLIMATE_SWING_OFF;
            }

            this -> publish_state();
            return true;
        }

    } // namespace arg
} // namespace esphome