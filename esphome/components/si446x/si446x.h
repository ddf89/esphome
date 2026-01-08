#pragma once

#include <utility>
#include <vector>
#include <mem.h>

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/automation.h"

#include "si446x_config.h"

namespace esphome {
namespace si446x {

const std::vector<u_int8_t> GET_PH_INFO{0x02, 0x21};
const std::vector<u_int8_t> GET_MODEM_INFO{0x06, 0x22};
const std::vector<u_int8_t> GET_PART_INFO{0x08, 0x01};
const std::vector<u_int8_t> GET_CHIP_STATUS{0x03, 0x23};
const std::vector<u_int8_t> REQUEST_DEVICE_STATE{0x04, 0x33};
const std::vector<u_int8_t> FETCH_INTERRUPTS{0x08, 0x20, 0xfb, 0x3f, 0x3f};
const std::vector<u_int8_t> CLEAR_INTERRUPTS{0x08, 0x20};
const std::vector<u_int8_t> SET_DEVICE_STATE{0x00, 0x34};

enum DeviceState { SLEEP = 0x01, READY = 0x03 };

class SI446xComponent : public spi::SPIDevice<spi::BIT_ORDER_LSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                              spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ>,
                        public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void dump_radio_config();

  void set_sdn_pin(GPIOPin *sdn) { this->sdn_pin_ = sdn; }
  void set_nirq_pin(GPIOPin *nirq) { this->nirq_pin_ = nirq; }
  void set_radio_config(std::vector<uint8_t> siconf) { this->siconfig_ = siconf; }

  void reset_radio();
  void send_init_config();
  void spi_send_command(std::vector<uint8_t> command, std::vector<uint8_t> extra_params);
  void spi_send(uint8_t command, void *out, uint8_t outlen);
  void spi_send(void *commandData, uint8_t len, void *out, uint8_t outlen);
  uint8_t spi_receive_wait(void *out, uint8_t outLen, uint16_t timeout);
  uint8_t spi_receive(void *bb, uint8_t len);
  std::string hextobin(u_int8_t s);

 protected:
  GPIOPin *sdn_pin_;
  GPIOPin *nirq_pin_;
  std::vector<uint8_t> siconfig_;
};

}  // namespace si446x
}  // namespace esphome
