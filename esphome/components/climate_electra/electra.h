#include "esphome/components/climate_ir/climate_ir.h"
#include "IRremoteESP8266.h"
#include "IRsend.h"
#include "ir_Electra.h"

namespace esphome {
namespace electra {

// Temperature
const float ELECTRA_TEMP_MAX = 30.0;
const float ELECTRA_TEMP_MIN = 19.0;

class ElectraClimate : public climate_ir::ClimateIR {
 public:
  ElectraClimate()
      : climate_ir::ClimateIR(ELECTRA_TEMP_MIN, ELECTRA_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL},
                              {climate::CLIMATE_PRESET_BOOST, climate::CLIMATE_PRESET_NONE}) {
                                ac = new IRElectraAc(0);
                              }

 protected:
  IRElectraAc *ac;
  // climate::ClimateTraits traits() override;
  void transmit_state() override;
  void transmit_sensor_update();

 public:
  void setup() override;
  // bool on_receive(remote_base::RemoteReceiveData data) override;
};

}  // namespace electra
}  // namespace esphome
