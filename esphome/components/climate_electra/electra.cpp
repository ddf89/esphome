#define _IR_ENABLE_DEFAULT_ false
#define SEND_ELECTRA_AC true
#define DECODE_ELECTRA_AC false

#include "electra.h"
#include "esphome/core/log.h"

namespace esphome {
namespace electra {

static const char *const TAG = "electra.climate";

const uint16_t kElectraAcHdrMark = 9166;
const uint16_t kElectraAcBitMark = 646;
const uint16_t kElectraAcHdrSpace = 4470;
const uint16_t kElectraAcOneSpace = 1647;
const uint16_t kElectraAcZeroSpace = 547;
const uint32_t kElectraAcMessageGap = 100000;  // Just a guess.
const uint8_t kElectraAcStateLength = 13;

void ElectraClimate::setup() {
  climate_ir::ClimateIR::setup();
  if (this->sensor_) {
    this->sensor_->add_on_state_callback([this](float state) {
      this->current_temperature = state;

      if (this->mode == climate::CLIMATE_MODE_OFF) {
        return;
      }

      ESP_LOGD(TAG, "temp sensor state callback");

      this->do_transmit(true);
    });
  }
}

void ElectraClimate::transmit_state() {
  this->do_transmit(false);
}

void ElectraClimate::do_transmit(bool sensor_update) {
  ac->stateReset();

  ac->setPower(true);

  // Set mode
  switch (this->mode) {
    case climate::CLIMATE_MODE_AUTO:
    case climate::CLIMATE_MODE_HEAT_COOL:
      ac->setMode(kElectraAcAuto);
      break;
    case climate::CLIMATE_MODE_COOL:
      ac->setMode(kElectraAcCool);
      break;
    case climate::CLIMATE_MODE_HEAT:
      ac->setMode(kElectraAcHeat);
      break;
    case climate::CLIMATE_MODE_DRY:
      ac->setMode(kElectraAcDry);
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      ac->setMode(kElectraAcFan);
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      ac->setPower(false);
      break;
  }

  if (this->preset.has_value()) {
    switch (this->preset.value()) {
      case climate::CLIMATE_PRESET_BOOST:
        ac->setTurbo(true);
        break;
      case climate::CLIMATE_PRESET_NONE:
      default:
        ac->setTurbo(false);
    }
  } else {
    ac->setTurbo(false);
  }

  ac->setTemp(this->target_temperature);

  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_HIGH:
      ac->setFan(kElectraAcFanHigh);
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      ac->setFan(kElectraAcFanMed);
      break;
    case climate::CLIMATE_FAN_LOW:
      ac->setFan(kElectraAcFanLow);
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      ac->setFan(kElectraAcFanAuto);
  }

  ac->setSwingH(false);

  if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL) {
    ac->setSwingV(true);
  } else {
    ac->setSwingV(false);
  }

  if (ac->getPower() || poweredOn) {
    poweredOn = ac->getPower();
  } else {
    return;
  }

  ac->setIFeel(true);

  if (sensor_update) {
      uint8_t t = uint8_t(lround(this->current_temperature + 0.5));
      ESP_LOGD(TAG, "Sending iFeel sensor update %d", t);

      ac->setSensorUpdate(true);
      ac->setSensorTemp(t);
  } else {
      ac->setSensorUpdate(false);
  }

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(38000);

  ESP_LOGD(TAG, "ac ir remote state %s", this->ac->toString().c_str());
  uint8_t *message = this->ac->getRaw();

  data->mark(kElectraAcHdrMark);
  data->space(kElectraAcHdrSpace);

  // Data
  for (uint8_t i = 0; i < kElectraAcStateLength; i++)
  {
      uint8_t d = *(message + i);
      for (uint8_t bit = 0; bit < 8; bit++, d >>= 1)
      {
          if (d & 1)
          {
              data->mark(kElectraAcBitMark);
              data->space(kElectraAcOneSpace);
          }
          else
          {
              data->mark(kElectraAcBitMark);
              data->space(kElectraAcZeroSpace);
          }
      }
  }

  // Footer
  data->mark(kElectraAcBitMark);
  data->space(kElectraAcMessageGap);

  transmit.perform();
}

}  // namespace electra
}  // namespace esphome
