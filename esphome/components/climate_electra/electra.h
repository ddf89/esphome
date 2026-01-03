#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace electra {

const uint16_t kNoRepeat = 0;
const uint16_t kElectraAcStateLength = 13;
const uint16_t kElectraAcBits = kElectraAcStateLength * 8;

const uint8_t kElectraAcMinTemp = 16;   // 16C
const uint8_t kElectraAcMaxTemp = 32;   // 32C
const uint8_t kElectraAcTempDelta = 8;
const uint8_t kElectraAcSwingOn =  0b000;
const uint8_t kElectraAcSwingOff = 0b111;

const uint8_t kElectraAcFanAuto =    0b101;
const uint8_t kElectraAcFanLow =     0b011;
const uint8_t kElectraAcFanMed =     0b010;
const uint8_t kElectraAcFanHigh =    0b001;

const uint8_t kElectraAcAuto =     0b000;
const uint8_t kElectraAcCool =     0b001;
const uint8_t kElectraAcDry =      0b010;
const uint8_t kElectraAcHeat =     0b100;
const uint8_t kElectraAcFan =      0b110;

union ElectraProtocol {
  uint8_t raw[kElectraAcStateLength];   ///< The state of the IR remote
  struct {
    // Byte 0
    uint8_t         :8;
    // Byte 1
    uint8_t SwingV  :3;
    uint8_t Temp    :5;
    // Byte 2
    uint8_t         :5;
    uint8_t SwingH  :3;
    // Byte 3
    uint8_t              :6;
    uint8_t SensorUpdate :1;
    uint8_t              :1;
    // Byte 4
    uint8_t         :5;
    uint8_t Fan     :3;
    // Byte 5
    uint8_t         :6;
    uint8_t Turbo   :1;
    uint8_t         :1;
    // Byte 6
    uint8_t         :3;
    uint8_t IFeel   :1;
    uint8_t         :1;
    uint8_t Mode    :3;
    // Byte 7
    uint8_t SensorTemp :8;
    // Byte 8
    uint8_t         :8;
    // Byte 9
    uint8_t         :2;
    uint8_t Clean   :1;
    uint8_t         :2;
    uint8_t Power   :1;
    uint8_t         :2;
    // Byte 10
    uint8_t         :8;
    // Byte 11
    uint8_t LightToggle :8;
    // Byte 12
    uint8_t Sum     :8;
  };
};

class ElectraClimate : public climate_ir::ClimateIR {
 public:
  const uint16_t kElectraAcMinRepeat = kNoRepeat;
  const uint32_t kDefaultMessageGap = 100000;
  const uint8_t kElectraAcLightToggleOff = 0x08;
  const uint16_t kElectraAcHdrMark = 9166;
  const uint16_t kElectraAcBitMark = 646;
  const uint16_t kElectraAcHdrSpace = 4470;
  const uint16_t kElectraAcOneSpace = 1647;
  const uint16_t kElectraAcZeroSpace = 547;
  const uint32_t kElectraAcMessageGap = kDefaultMessageGap;  // Just a guess.

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
  // climate::ClimateTraits traits() override;
  void transmit_state() override;

 private:
  ElectraProtocol proto;
  void stateReset(void);
  void setMode(const uint8_t mode);
  void setTemp(const uint8_t temp);
  void setFan(const uint8_t speed);
  void setSwingV(const bool on);

  void checksum(const uint16_t length = kElectraAcStateLength);
  static bool validChecksum(const uint8_t state[], const uint16_t length = kElectraAcStateLength);
  static uint8_t calcChecksum(const uint8_t state[], const uint16_t length = kElectraAcStateLength);
  static uint8_t sumBytes(const uint8_t * const start, const uint16_t length, const uint8_t init = 0);

  String toString(void);
  uint8_t* getRaw(void);
  void do_transmit(bool sensor_update);
};

}  // namespace electra
}  // namespace esphome
