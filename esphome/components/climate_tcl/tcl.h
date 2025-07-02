#include "esphome/components/climate_ir/climate_ir.h"
#include "IRremoteESP8266.h"
#include "IRsend.h"
#include "ir_Tcl.h"

namespace esphome {
namespace tcl {

// Temperature
const float TCL_TEMP_MAX = 30.0;
const float TCL_TEMP_MIN = 19.0;

class TclClimate : public climate_ir::ClimateIR {
 public:
  TclClimate()
      : climate_ir::ClimateIR(TCL_TEMP_MIN, TCL_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL},
                              {climate::CLIMATE_PRESET_BOOST, climate::CLIMATE_PRESET_NONE}) {
                                ac = new IRTcl112Ac(0);
                                poweredOn = false;
                              }
  void setup() override;
  void setIFeel(bool state);

 protected:
  IRTcl112Ac *ac;
  // climate::ClimateTraits traits() override;
  void transmit_state() override;

 private:
  void do_transmit(bool sensor_update);
  bool poweredOn;
};

}  // namespace electra
}  // namespace esphome
