#pragma once

#include "esphome/components/switch/switch.h"
#include "../electra.h"

namespace esphome {
namespace electra {

class IFeelSwitch : public switch_::Switch, public Parented<ElectraClimate> {
 public:
  IFeelSwitch() = default;

 protected:
  void write_state(bool state) override;
};

}  // namespace electra
}  // namespace esphome
