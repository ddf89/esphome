#include "ifeel.h"
#include "esphome/core/log.h"

namespace esphome {
namespace electra {

static const char *const TAG = "electra.climate";

void IFeelSwitch::write_state(bool state) {
  this->parent_->setIFeel(state);
}

}
}
