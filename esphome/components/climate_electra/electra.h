#include "esphome/components/climate_ir/climate_ir.h"
// #include "IRremoteESP8266.h"
// #include "IRsend.h"
#include "ir_Electra.h"
#include "IRutils.h"

namespace esphome {
namespace electra {

class ElectraClimate : public climate_ir::ClimateIR {
 public:
  // Temperature
  // const float ELECTRA_TEMP_MAX = 30.0;
  // const float ELECTRA_TEMP_MIN = 19.0;

  ElectraClimate()
      : climate_ir::ClimateIR(19.0f, 30.0f, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL},
                              {climate::CLIMATE_PRESET_BOOST, climate::CLIMATE_PRESET_NONE}) {

                              }
  void setup() override;
  // void setIFeel(bool state);

 protected:
  // IRElectraAc *ac;
  // climate::ClimateTraits traits() override;
  void transmit_state() override;

 private:
  const uint16_t kElectraAcHdrMark = 9166;
  const uint16_t kElectraAcBitMark = 646;
  const uint16_t kElectraAcHdrSpace = 4470;
  const uint16_t kElectraAcOneSpace = 1647;
  const uint16_t kElectraAcZeroSpace = 547;
  const uint32_t kElectraAcMessageGap = kDefaultMessageGap;  // Just a guess.

  ElectraProtocol proto;
  void stateReset(void);
  void setMode(const uint8_t mode);
  void setTemp(const uint8_t temp);
  void setFan(const uint8_t speed);
  void setSwingV(const bool on);

  void checksum(const uint16_t length = kElectraAcStateLength);
  static bool validChecksum(const uint8_t state[], const uint16_t length = kElectraAcStateLength);
  static uint8_t calcChecksum(const uint8_t state[], const uint16_t length = kElectraAcStateLength);
  // static uint8_t sumBytes(const uint8_t * const start, const uint16_t length, const uint8_t init = 0);

  String toString(void) const;
  uint8_t* getRaw(void);
  void do_transmit(bool sensor_update);
};

}  // namespace electra
}  // namespace esphome
