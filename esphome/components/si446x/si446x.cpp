#include "si446x.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <cinttypes>
#include <cstring>

namespace esphome {
namespace si446x {

static const char *const TAG = "si446x";
static const uint8_t siconfig[] = RADIO_CONFIGURATION_DATA_ARRAY;

void SI446xComponent::setup() {
  ESP_LOGV(TAG, "setup");

  this->spi_setup();
  this->set_spi_parent(this->parent_);
  this->enable();
  this->sdn_pin_->setup();
  this->sdn_pin_->digital_write(false);
  this->cs_->setup();
  this->cs_->digital_write(true);
  this->nirq_pin_->setup();
  // this->set_radio_config(RADIO_CONFIGURATION_DATA_ARRAY);
  // this->dump_radio_config();

  this->reset_radio();
}

float SI446xComponent::get_setup_priority() const { return setup_priority::DATA; }

void SI446xComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "SI446x:\n"
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

void SI446xComponent::dump_radio_config() {
  ESP_LOGV(TAG, "dump_radio_config");

  for (uint16_t i = 0 ; i < sizeof(siconfig); i++) {
    ESP_LOGCONFIG(TAG, "SI446x Radio Config: (pos %d): %x\n", i, siconfig[i]);
  }
  // ESP_LOGCONFIG(TAG, "SI446x Radio Config (length %d / %d): %x %x %s\n", sizeof(siconfig), sizeof(str), siconfig[0], siconfig[12], str);
  std::string str = (char *) siconfig;
  // ESP_LOGCONFIG(TAG, "SI446x Radio Config (length %d / %d): %x %x %s\n", sizeof(siconfig), sizeof(str), siconfig[0], siconfig[12], str);
  ESP_LOGCONFIG(TAG, "SI446x Radio Config (length %d): %s\n", sizeof(siconfig), str);
  // ESP_LOGCONFIG(TAG, "SI446x Radio Config (length %d / %d): %s %s\n", sizeof(siconfig), sizeof(str), siconfig[0], siconfig[1]);
}

std::string SI446xComponent::hextobin(u_int8_t s) { return std::bitset<8>(s).to_string(); }

void SI446xComponent::reset_radio() {
  ESP_LOGV(TAG, "reset_radio");
  if (!sdn_pin_->digital_read()) {
    // this->disable();
    this->cs_->digital_write(true);
    this->sdn_pin_->digital_write(true);
    delay(200);
  }

  sdn_pin_->digital_write(false);
  delay(200);

  this->send_init_config();
}

void SI446xComponent::send_init_config() {
  ESP_LOGV(TAG, "send_init_config");

  uint8_t buff[17];

  for (uint16_t i = 0; i < sizeof(siconfig); i++) {
    memcpy_P(buff, &siconfig[i], sizeof(buff));
    this->spi_send(&buff[1], buff[0], NULL, 0);
    i += buff[0];
    // delay(50);
  }
}

uint8_t SI446xComponent::spi_receive(void *bb, uint8_t len) {
  uint8_t cts = 0;

  // this->enable();
  this->cs_->digital_write(false);

  uint8_t ret = this->transfer_byte(0x44);
    ESP_LOGV("si4463.h", "reponse %x", ret);

  cts = ret == 0xFF;

  if (cts) {
    this->transfer_byte(0xFF);
    for (uint8_t i = 0; i < len; i++)
      ((uint8_t *) bb)[i] = this->transfer_byte(0xFF);
  }

  // this->disable();
  this->cs_->digital_write(true);

  return cts;
}

uint8_t SI446xComponent::spi_receive_wait(void *out, uint8_t outLen, uint16_t timeout) {
  bool withTimeout = timeout > 0;

  while (!this->spi_receive(out, outLen)) {
    ESP_LOGV("si4463.h", "cts false");
    delay(4);
    if (withTimeout && !--timeout) {
      return 0;
    }
  }
  return 1;
}

void SI446xComponent::spi_send(void *commandData, uint8_t len, void *out, uint8_t outlen) {
  // this->enable();
  this->cs_->digital_write(false);

  for (uint8_t i = 0; i < len; i++) {
    ESP_LOGV("si4463.h", "sendcommand %x", ((uint8_t*)commandData)[i]);
    Serial.printf("sendcommand %x\n",((uint8_t*)commandData)[i]);
    this->write_byte(((uint8_t *) commandData)[i]);
    // delay(2);
  }

  // this->disable();
  this->cs_->digital_write(true);

  if (outlen > 0) {
    ESP_LOGV("si4463.h", "waiting for response");
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

void SI446xComponent::toggleSdn() {
  this->sdn_pin_->digital_write(!this->sdn_pin_->digital_read());
}

void SI446xComponent::toggleCs() {
  this->cs_->digital_write(!this->cs_->digital_read());
}

void SI446xComponent::toggleIrq() {
  this->nirq_pin_->digital_write(!this->nirq_pin_->digital_read());
}

}  // namespace si446x
}  // namespace esphome
