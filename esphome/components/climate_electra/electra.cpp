#include "electra.h"
#include "esphome/core/log.h"

namespace esphome {
namespace electra {

static const char *const TAG = "electra.climate";

void ElectraClimate::setup() {
  climate_ir::ClimateIR::setup();
  this->stateReset();
  // if (this->sensor_) {
  //   this->sensor_->add_on_state_callback([this](float state) {
  //     this->current_temperature = state;

  //     // if (this->mode == climate::CLIMATE_MODE_OFF) {
  //     //   return;
  //     // }

  //     // ESP_LOGD(TAG, "temp sensor state callback");

  //     // this->do_transmit(true);
  //   });
  // }
}

void ElectraClimate::stateReset(void) {
  for (uint8_t i = 1; i < kElectraAcStateLength - 2; i++) proto.raw[i] = 0;
  proto.raw[0] = 0xC3;
  proto.LightToggle = kElectraAcLightToggleOff;
  // [12] is the checksum.
}

void ElectraClimate::transmit_state() {
  this->do_transmit(false);
}

void ElectraClimate::do_transmit(bool sensor_update) {
  // Set mode
  this->proto.Power = true;

  switch (this->mode) {
    case climate::CLIMATE_MODE_AUTO:
    case climate::CLIMATE_MODE_HEAT_COOL:
      this->setMode(kElectraAcAuto);
      break;
    case climate::CLIMATE_MODE_COOL:
      this->setMode(kElectraAcCool);
      break;
    case climate::CLIMATE_MODE_HEAT:
      this->setMode(kElectraAcHeat);
      break;
    case climate::CLIMATE_MODE_DRY:
      this->setMode(kElectraAcDry);
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      this->setMode(kElectraAcFan);
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      this->proto.Power = false;
      break;
  }

  switch (this->preset.has_value() && this->preset.value()) {
    case climate::CLIMATE_PRESET_BOOST:
      this->proto.Turbo = true;
      break;
    case climate::CLIMATE_PRESET_NONE:
    default:
      this->proto.Turbo = false;
  }

  this->setTemp(this->target_temperature);

  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_HIGH:
      this->setFan(kElectraAcFanHigh);
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      this->setFan(kElectraAcFanMed);
      break;
    case climate::CLIMATE_FAN_LOW:
      this->setFan(kElectraAcFanLow);
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      this->setFan(kElectraAcFanAuto);
  }

  if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL) {
    this->setSwingV(true);
  } else {
    this->setSwingV(false);
  }

  if (!this->proto.Power) {
    return;
  }

  // if (sensor_update) {
  //     uint8_t t = uint8_t(lround(this->current_temperature + 0.5));
  //     ESP_LOGD(TAG, "Sending iFeel sensor update %d", t);

  //     ac->setIFeel(true);
  //     ac->setSensorUpdate(true);
  //     ac->setSensorTemp(t);
  // } else {
      // ac->setIFeel(false);
      // ac->setSensorUpdate(false);
  // }

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(38000);

  ESP_LOGD(TAG, "ac ir remote state %s", this->toString().c_str());
  uint8_t *message = this->getRaw();

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

void ElectraClimate::setMode(const uint8_t mode) {
  switch (mode) {
    case kElectraAcAuto:
    case kElectraAcDry:
    case kElectraAcCool:
    case kElectraAcHeat:
    case kElectraAcFan:
      proto.Mode = mode;
      break;
    default:
      // If we get an unexpected mode, default to AUTO.
      proto.Mode = kElectraAcAuto;
  }
}

void ElectraClimate::setTemp(const uint8_t temp) {
  uint8_t newtemp = std::max(kElectraAcMinTemp, temp);
  newtemp = std::min(kElectraAcMaxTemp, newtemp) - kElectraAcTempDelta;
  proto.Temp = newtemp;
}

void ElectraClimate::setFan(const uint8_t speed) {
  switch (speed) {
    case kElectraAcFanAuto:
    case kElectraAcFanHigh:
    case kElectraAcFanMed:
    case kElectraAcFanLow:
      proto.Fan = speed;
      break;
    default:
      // If we get an unexpected speed, default to Auto.
      proto.Fan = kElectraAcFanAuto;
  }
}

void ElectraClimate::setSwingV(const bool on) {
  proto.SwingV = (on ? kElectraAcSwingOn : kElectraAcSwingOff);
}

uint8_t *ElectraClimate::getRaw(void) {
  checksum();
  return proto.raw;
}

void ElectraClimate::checksum(uint16_t length) {
  if (length < 2) return;
  proto.Sum = calcChecksum(proto.raw, length);
}

uint8_t ElectraClimate::calcChecksum(const uint8_t state[], const uint16_t length) {
  if (length == 0) return state[0];
  return sumBytes(state, length - 1);
}

// uint8_t ElectraClimate::sumBytes(const uint8_t * const start, const uint16_t length, const uint8_t init) {
//   uint8_t checksum = init;
//   const uint8_t *ptr;
//   for (ptr = start; ptr - start < length; ptr++) checksum += *ptr;
//   return checksum;
// }

String ElectraClimate::toString(void) const {
  String result = "";
  result.reserve(160);  // Reserve some heap for the string to reduce fragging.
  if (!proto.SensorUpdate) {
    result += irutils::addBoolToString(proto.Power, "Power", false);
    result += irutils::addModeToString(proto.Mode, kElectraAcAuto, kElectraAcCool,
                              kElectraAcHeat, kElectraAcDry, kElectraAcFan);
    result += irutils::addTempToString(proto.Temp + kElectraAcTempDelta);
    result += irutils::addFanToString(proto.Fan, kElectraAcFanHigh, kElectraAcFanLow,
                             kElectraAcFanAuto, kElectraAcFanAuto,
                             kElectraAcFanMed);
    result += irutils::addBoolToString(!proto.SwingV, "SwingV");
    // result += irutils::addBoolToString(!proto.SwingH, kSwingHStr);
    // result += irutils::addToggleToString(getLightToggle(), kLightStr);
    // result += irutils::addBoolToString(proto.Clean, kCleanStr);
    result += irutils::addBoolToString(proto.Turbo, "Turbo");
    // result += irutils::addBoolToString(proto.IFeel, kIFeelStr);
  }
  // if (proto.IFeel || proto.SensorUpdate) {
  //   result += addIntToString(getSensorTemp(), kSensorTempStr, !proto.SensorUpdate);
  //   result += 'C';
  // }
  return result;
}

}  // namespace electra
}  // namespace esphome
