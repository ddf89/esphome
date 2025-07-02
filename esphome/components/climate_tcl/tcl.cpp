#define _IR_ENABLE_DEFAULT_ false

#include "tcl.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tcl {

static const char *const TAG = "tcl.climate";

const uint16_t kTcl112AcHdrMark = 3000;
const uint16_t kTcl112AcHdrSpace = 1650;
const uint16_t kTcl112AcBitMark = 500;
const uint16_t kTcl112AcOneSpace = 1050;
const uint16_t kTcl112AcZeroSpace = 325;
const uint32_t kTcl112AcGap = kDefaultMessageGap;  // Just a guess.
// Total tolerance percentage to use for matching the header mark.
const uint8_t kTcl112AcHdrMarkTolerance = 6;
const uint8_t kTcl112AcTolerance = 5;  // Extra Percentage for the rest.


void TclClimate::setup() {
  climate_ir::ClimateIR::setup();
  if (this->sensor_) {
    this->sensor_->add_on_state_callback([this](float state) {
      this->current_temperature = state;

      // if (this->mode == climate::CLIMATE_MODE_OFF) {
      //   return;
      // }

      // ESP_LOGD(TAG, "temp sensor state callback");

      // this->do_transmit(true);
    });
  }
}

void TclClimate::transmit_state() {
  this->do_transmit(false);
}

void TclClimate::do_transmit(bool sensor_update) {
  ac->stateReset();

  ac->setPower(true);

  // Set mode
  switch (this->mode) {
    case climate::CLIMATE_MODE_AUTO:
    case climate::CLIMATE_MODE_HEAT_COOL:
      ac->setMode(kTcl112AcAuto);
      break;
    case climate::CLIMATE_MODE_COOL:
      ac->setMode(kTcl112AcCool);
      break;
    case climate::CLIMATE_MODE_HEAT:
      ac->setMode(kTcl112AcHeat);
      break;
    case climate::CLIMATE_MODE_DRY:
      ac->setMode(kTcl112AcDry);
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      ac->setMode(kTcl112AcFan);
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
      ac->setFan(kTcl112AcFanHigh);
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      ac->setFan(kTcl112AcFanMed);
      break;
    case climate::CLIMATE_FAN_LOW:
      ac->setFan(kTcl112AcFanLow);
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      ac->setFan(kTcl112AcFanAuto);
  }

  ac->setSwingHorizontal(false);

  if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL) {
    ac->setSwingVertical(true);
  } else {
    ac->setSwingVertical(false);
  }

  if (ac->getPower() || poweredOn) {
    poweredOn = ac->getPower();
  } else {
    return;
  }

  if (sensor_update) {
      uint8_t t = uint8_t(lround(this->current_temperature + 0.5));
      ESP_LOGD(TAG, "Sending iFeel sensor update %d", t);

      // ac->setIFeel(true);
      // ac->setSensorUpdate(true);
      // ac->setSensorTemp(t);
  } else {

  }

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(38000);

  ESP_LOGD(TAG, "ac ir remote state %s", this->ac->toString().c_str());
  uint8_t *message = this->ac->getRaw();

  data->mark(kTcl112AcHdrMark);
  data->space(kTcl112AcHdrSpace);

  // Data
  for (uint8_t i = 0; i < kTcl112AcStateLength; i++)
  {
      uint8_t d = *(message + i);
      for (uint8_t bit = 0; bit < 8; bit++, d >>= 1)
      {
          if (d & 1)
          {
              data->mark(kTcl112AcBitMark);
              data->space(kTcl112AcOneSpace);
          }
          else
          {
              data->mark(kTcl112AcBitMark);
              data->space(kTcl112AcZeroSpace);
          }
      }
  }

  // Footer
  data->mark(kTcl112AcBitMark);
  data->space(kTcl112AcGap);

  transmit.perform();
}

}  // namespace tcl
}  // namespace esphome
