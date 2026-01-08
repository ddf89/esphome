#include "si446x.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <cinttypes>
#include <cstring>

namespace esphome {
namespace si446x {

static const char *const TAG = "si446x";

void SI446xComponent::setup() {
  this->spi_setup();
  this->disable();
  this->sdn_pin_->setup();
  this->sdn_pin_->digital_write(false);
  this->nirq_pin_->setup();
  this->set_radio_config(RADIO_CONFIGURATION_DATA_ARRAY);

  this->reset_radio();
}

float SI446xComponent::get_setup_priority() const { return setup_priority::DATA; }

void SI446xComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "SI446x\n"
                "  Mode: %d",
                this->mode_);
  LOG_PIN("  CS pin: ", this->cs_);
  LOG_PIN("  SDN pin: ", this->sdn_pin_);
  LOG_PIN("  nIRQ pin: ", this->nirq_pin_);
  if (this->data_rate_ < 1000000) {
    ESP_LOGCONFIG(TAG, "  Data rate: %" PRId32 "kHz", this->data_rate_ / 1000);
  } else {
    ESP_LOGCONFIG(TAG, "  Data rate: %" PRId32 "MHz", this->data_rate_ / 1000000);
  }
}

void SI446xComponent::dump_radio_config() { ESP_LOGCONFIG(TAG, "SI446x Radio Config:\n%x", siconfig_); }

std::string SI446xComponent::hextobin(u_int8_t s) { return std::bitset<8>(s).to_string(); }

void SI446xComponent::reset_radio() {
  if (sdn_pin_->digital_read()) {
    this->disable();
    sdn_pin_->digital_write(false);
    delay(200);
  }

  sdn_pin_->digital_write(true);
  delay(200);

  this->send_init_config();
}

void SI446xComponent::send_init_config() {
  uint8_t buff[17];

  for (uint16_t i = 0; i < sizeof(siconfig_); i++) {
    memcpy_P(buff, &siconfig_[i], sizeof(buff));
    this->spi_send(&buff[1], buff[0], NULL, 0);
    i += buff[0];
    // delay(50);
  }
}

uint8_t SI446xComponent::spi_receive(void *bb, uint8_t len) {
  uint8_t cts = 0;

  this->enable();

  uint8_t ret = this->transfer_byte(0x44);
  //   ESP_LOGV("si4463.h", "reponse %x", ret);

  cts = ret == 0xFF;

  if (cts) {
    this->transfer_byte(0xFF);
    for (uint8_t i = 0; i < len; i++)
      ((uint8_t *) bb)[i] = this->transfer_byte(0xFF);
  }

  this->disable();

  return cts;
}

uint8_t SI446xComponent::spi_receive_wait(void *out, uint8_t outLen, uint16_t timeout) {
  bool withTimeout = timeout > 0;

  while (!this->spi_receive(out, outLen)) {
    // ESP_LOGV("si4463.h", "cts false");
    delay(4);
    if (withTimeout && !--timeout) {
      return 0;
    }
  }
  return 1;
}

void SI446xComponent::spi_send(void *commandData, uint8_t len, void *out, uint8_t outlen) {
  this->enable();

  for (uint8_t i = 0; i < len; i++) {
    // ESP_LOGV("si4463.h", "sendcommand %x", ((uint8_t*)commandData)[i]);
    // Serial.printf("sendcommand %x\n",((uint8_t*)commandData)[i]);
    this->write_byte(((uint8_t *) commandData)[i]);
    // delay(2);
  }

  this->disable();

  if (outlen > 0) {
    // ESP_LOGV("si4463.h", "waiting for response");
    if (this->spi_receive_wait(out, outlen, (uint16_t) 100) == 0) {
      out = NULL;
    }
  }
}

void SI446xComponent::spi_send(uint8_t command, void *out, uint8_t outlen) {
  uint8_t commandList[] = {command};
  this->spi_send(commandList, (uint8_t) 1, out, outlen);
}

void spi_send_command(std::vector<uint8_t> command, std::vector<uint8_t> extra_params) {}

}  // namespace si446x
}  // namespace esphome
