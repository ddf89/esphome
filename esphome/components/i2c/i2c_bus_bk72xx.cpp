#ifdef USE_BK72XX

#include "i2c_bus_bk72xx.h"
#include <Arduino.h>
#include <cinttypes>
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::i2c {

static const char *const TAG = "i2c";

void BK72xxI2CBus::setup() {
  // Four quarter-periods make one bit, so treat the configured frequency as a
  // ceiling hint rather than a setting. Each GPIO edge on this core costs real
  // time (the pin lookup is not inlined and the platform forces -O1 on the
  // wiring layer), so the achieved rate lands well below the request. That is
  // harmless: I2C specifies a maximum clock, not a minimum.
  uint32_t quarter;
  if (this->frequency_ == 0) {
    quarter = 5;
  } else if (this->frequency_ >= 250000) {
    quarter = 1;
  } else {
    quarter = 1000000UL / (this->frequency_ * 4UL);
    if (quarter == 0) {
      quarter = 1;
    } else if (quarter > 65535) {
      quarter = 65535;
    }
  }
  this->delay_us_ = (uint16_t) quarter;

  // Park both lines as inputs before anything else, so we never drive a line
  // high - not even for the instant it takes to configure the pins.
  this->sda_release_();
  this->scl_release_unchecked_();
  esphome::delayMicroseconds(10);

  this->bus_idle_at_setup_ = this->bus_idle_();
  if (!this->bus_idle_at_setup_) {
    // Either a device is wedged mid-transfer, or - far more commonly - the
    // external pull-ups are missing. Recovery distinguishes the two, and
    // dump_config() reports which.
    this->recovery_attempted_ = true;
    this->recover_();
  }

  this->initialized_ = true;
  if (this->scan_) {
    ESP_LOGV(TAG, "Scanning bus for active devices");
    this->i2c_scan_();
  }
}

void BK72xxI2CBus::dump_config() {
  ESP_LOGCONFIG(TAG, "I2C Bus:");
  ESP_LOGCONFIG(TAG,
                "  Implementation: software (bit-banged)\n"
                "  SDA Pin: GPIO%u\n"
                "  SCL Pin: GPIO%u\n"
                "  Frequency: %" PRIu32 " Hz (requested)\n"
                "  Clock stretch timeout: %" PRIu32 " us",
                this->sda_pin_, this->scl_pin_, this->frequency_, this->timeout_us_);
  if (!this->bus_idle_at_setup_) {
    ESP_LOGW(TAG,
             "  Bus was not idle at startup: SDA and SCL did not both read high. "
             "External pull-up resistors (4.7k to 3V3) on both lines are required - the MCU only ever pulls down.");
  }
  if (this->recovery_attempted_) {
    switch (this->recovery_result_) {
      case RECOVERY_COMPLETED:
        ESP_LOGCONFIG(TAG, "  Recovery: bus successfully recovered");
        break;
      case RECOVERY_FAILED_SCL_LOW:
        ESP_LOGCONFIG(TAG, "  Recovery: failed, SCL is held low on the bus");
        break;
      case RECOVERY_FAILED_SDA_LOW:
        ESP_LOGCONFIG(TAG, "  Recovery: failed, SDA is held low on the bus");
        break;
    }
  }
  if (this->scan_) {
    ESP_LOGI(TAG, "Results from bus scan:");
    if (this->scan_results_.empty()) {
      ESP_LOGI(TAG, "Found no devices");
    } else {
      for (const auto &s : this->scan_results_) {
        if (s.second) {
          ESP_LOGI(TAG, "Found device at address 0x%02X", s.first);
        } else {
          ESP_LOGE(TAG, "Unknown error at address 0x%02X", s.first);
        }
      }
    }
  }
}

ErrorCode BK72xxI2CBus::write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count,
                                    uint8_t *read_buffer, size_t read_count) {
  if (!this->initialized_) {
    ESP_LOGD(TAG, "i2c bus not initialized!");
    return ERROR_NOT_INITIALIZED;
  }

  this->error_ = ERROR_OK;

  // Zero-length probe: START, address byte, STOP. This is the only form
  // i2c_scan_() uses, and the one case where "nobody answered" is an ordinary
  // outcome - it must come back as ERROR_NOT_ACKNOWLEDGED, because the scanner
  // records ERROR_UNKNOWN as a result and logs it at error level, which would
  // mean 100-odd error lines for the unused addresses on every boot.
  if (write_count == 0 && read_count == 0) {
    const bool ack = this->start_(address, false);
    this->stop_();
    if (ack)
      return ERROR_OK;
    return this->error_ == ERROR_OK ? ERROR_NOT_ACKNOWLEDGED : this->error_;
  }

  if (write_count > 0) {
    if (!this->start_(address, false)) {
      this->stop_();
      return this->error_ == ERROR_OK ? ERROR_NOT_ACKNOWLEDGED : this->error_;
    }
    for (size_t i = 0; i < write_count; i++) {
      if (!this->write_byte_(write_buffer[i])) {
        this->stop_();
        return this->error_ == ERROR_OK ? ERROR_NOT_ACKNOWLEDGED : this->error_;
      }
    }
  }

  if (read_count > 0) {
    // A read following a write keeps the bus with a repeated START rather than
    // a STOP, so the register address just written stays selected. This is the
    // pattern essentially every I2C sensor expects.
    const bool addressed = write_count > 0 ? this->repeated_start_(address, true) : this->start_(address, true);
    if (!addressed) {
      this->stop_();
      return this->error_ == ERROR_OK ? ERROR_NOT_ACKNOWLEDGED : this->error_;
    }
    for (size_t i = 0; i < read_count; i++) {
      // ACK every byte except the last. The closing NACK is what tells the
      // target to stop driving SDA so the controller can issue a STOP.
      read_buffer[i] = this->read_byte_(i + 1 < read_count);
      if (this->error_ != ERROR_OK) {
        this->stop_();
        return this->error_;
      }
    }
  }

  this->stop_();
  return ERROR_OK;
}

// ---------------------------------------------------------------------------
// Line control
//
// Beken's Arduino wiring layer makes two implicit transitions that are lethal
// to assume away, so the direction of every pin is managed explicitly here:
//
//   * Writing a level to a pin that is not currently an output does not drive
//     it. The wiring layer instead reconfigures the pin to a weak pull in the
//     requested direction and returns, so a "drive low" on a released pin
//     quietly becomes a pull-down that a real bus overpowers. The mode is
//     therefore always set to output before the level is written.
//   * Reading a pin that is currently an output silently reconfigures it back
//     to an input. Only ever read a line that is already released.
//
// OUTPUT_OPEN_DRAIN is not an option: on beken-72xx it maps to
// GMODE_SET_HIGH_IMPENDANCE, which disables the output driver *and* the input
// buffer, so the pin can neither pull low nor be read back.
// ---------------------------------------------------------------------------

void BK72xxI2CBus::sda_low_() {
  pinMode(this->sda_pin_, OUTPUT);    // NOLINT
  digitalWrite(this->sda_pin_, LOW);  // NOLINT
}

void BK72xxI2CBus::sda_release_() {
  pinMode(this->sda_pin_, INPUT);  // NOLINT
}

void BK72xxI2CBus::scl_low_() {
  pinMode(this->scl_pin_, OUTPUT);    // NOLINT
  digitalWrite(this->scl_pin_, LOW);  // NOLINT
}

bool BK72xxI2CBus::scl_release_() {
  pinMode(this->scl_pin_, INPUT);  // NOLINT
  const uint32_t start = esphome::micros();
  while (!this->read_scl_()) {
    // Compare the elapsed difference, never an absolute deadline: micros()
    // wraps roughly every 71 minutes, and an unsigned compare against a
    // wrapped sum would spin here for the remainder of that period.
    if (esphome::micros() - start > this->timeout_us_) {
      if (this->error_ == ERROR_OK)
        this->error_ = ERROR_TIMEOUT;
      return false;
    }
  }
  return true;
}

void BK72xxI2CBus::scl_release_unchecked_() {
  pinMode(this->scl_pin_, INPUT);  // NOLINT
  // Allow for the pull-up's rise time, but never wait on a target here. This
  // runs from stop_() and recover_(), i.e. while abandoning a transfer, where
  // SCL may legitimately still be held low and where overwriting error_ would
  // hide the fault that caused the abort in the first place.
  const uint32_t start = esphome::micros();
  const uint32_t limit = (uint32_t) this->delay_us_ * 4;
  while (esphome::micros() - start < limit) {
    if (this->read_scl_())
      break;
  }
}

bool BK72xxI2CBus::read_sda_() {
  return digitalRead(this->sda_pin_) != LOW;  // NOLINT
}

bool BK72xxI2CBus::read_scl_() {
  return digitalRead(this->scl_pin_) != LOW;  // NOLINT
}

void BK72xxI2CBus::wait_() { esphome::delayMicroseconds(this->delay_us_); }

// ---------------------------------------------------------------------------
// Framing
//
// Timing, in units of delay_us_ (one quarter bit): SCL high for 2, SCL low for
// 2 (one after the falling edge plus one of data setup before the next rising
// edge), data sampled 1 into the high phase, START/STOP hold and setup 1 each,
// bus-free 1 after a STOP.
//
// These helpers never set error_ themselves except for the clock-stretch
// timeout inside scl_release_(). They only report failure through their return
// value, and write_readv() labels it - which is what keeps a stretch timeout
// from being misreported as a NACK.
// ---------------------------------------------------------------------------

bool BK72xxI2CBus::start_(uint8_t address, bool read) {
  this->sda_release_();
  if (!this->scl_release_())
    return false;
  this->wait_();

  if (!this->read_sda_()) {
    // Both lines should be idle high by now. SDA low means somebody else is
    // holding it down, which on a single-controller bus is a wedge rather than
    // arbitration loss - and is also what missing pull-ups look like.
    if (this->error_ == ERROR_OK)
      this->error_ = ERROR_UNKNOWN;
    return false;
  }

  this->sda_low_();  // SDA falling while SCL is high is a START
  this->wait_();
  this->scl_low_();
  this->wait_();
  return this->write_byte_((uint8_t) ((address << 1) | (read ? 1 : 0)));
}

bool BK72xxI2CBus::repeated_start_(uint8_t address, bool read) {
  // Entered with SCL already low, so SDA gets an extra setup period here that
  // start_() does not need.
  this->sda_release_();
  this->wait_();
  if (!this->scl_release_())
    return false;
  this->wait_();
  this->sda_low_();
  this->wait_();
  this->scl_low_();
  this->wait_();
  return this->write_byte_((uint8_t) ((address << 1) | (read ? 1 : 0)));
}

void BK72xxI2CBus::stop_() {
  this->sda_low_();
  this->wait_();
  this->scl_release_unchecked_();
  this->wait_();
  this->sda_release_();  // SDA rising while SCL is high is a STOP
  this->wait_();         // bus-free time before any next START
}

bool BK72xxI2CBus::write_byte_(uint8_t data) {
  for (uint8_t i = 0; i < 8; i++) {
    if ((data & 0x80) != 0) {
      this->sda_release_();
    } else {
      this->sda_low_();
    }
    data <<= 1;
    this->wait_();  // data setup before the rising edge
    if (!this->scl_release_())
      return false;
    this->wait_();
    this->wait_();
    this->scl_low_();
    this->wait_();
  }

  // Ninth clock: the target drives SDA low to acknowledge.
  this->sda_release_();
  this->wait_();
  if (!this->scl_release_())
    return false;
  this->wait_();
  const bool nack = this->read_sda_();
  this->wait_();
  this->scl_low_();
  this->wait_();
  return !nack;
}

uint8_t BK72xxI2CBus::read_byte_(bool ack) {
  uint8_t value = 0;

  // Released once, before the loop: SDA stays released for all eight bits.
  this->sda_release_();

  for (uint8_t i = 0; i < 8; i++) {
    value <<= 1;
    this->wait_();
    if (!this->scl_release_())
      return value;  // partial byte; the caller detects this via error_
    this->wait_();
    if (this->read_sda_())
      value |= 1;
    this->wait_();
    this->scl_low_();
    this->wait_();
  }

  // Ninth clock: acknowledge, unless this was the final byte.
  if (ack) {
    this->sda_low_();
  } else {
    this->sda_release_();
  }
  this->wait_();
  if (!this->scl_release_())
    return value;
  this->wait_();
  this->wait_();
  this->scl_low_();
  this->wait_();

  // Release SDA only once SCL is already low. Releasing it while SCL is high
  // would present a STOP to every device on the bus.
  this->sda_release_();
  return value;
}

bool BK72xxI2CBus::bus_idle_() {
  this->sda_release_();
  this->scl_release_unchecked_();
  esphome::delayMicroseconds(5);
  return this->read_sda_() && this->read_scl_();
}

/// Perform I2C bus recovery, see:
/// https://www.nxp.com/docs/en/user-guide/UM10204.pdf
/// https://www.analog.com/media/en/technical-documentation/application-notes/54305147357414AN686_0.pdf
void BK72xxI2CBus::recover_() {
  ESP_LOGI(TAG, "Performing bus recovery");

  // Target a 100kHz toggle for the recovery pulses, the maximum for
  // standard-mode I2C. The real rate comes out lower because of the GPIO call
  // overhead, which is no problem here.
  const uint32_t half_period_usec = 5;

  // If SCL cannot be brought high, some device is holding the clock down and
  // no amount of clocking from this end will fix it.
  this->scl_release_unchecked_();
  esphome::delayMicroseconds(half_period_usec);
  if (!this->read_scl_()) {
    ESP_LOGE(TAG, "Recovery failed: SCL is held LOW on the bus");
    this->recovery_result_ = RECOVERY_FAILED_SCL_LOW;
    return;
  }

  // From the specification:
  // "If the data line (SDA) is stuck LOW, send nine clock pulses. The device
  //  that held the bus LOW should release it sometime within those nine
  //  clocks."
  this->sda_release_();
  for (uint8_t i = 0; i < 9; i++) {
    if (this->read_sda_())
      break;  // the stuck device has let go, no need for the rest

    this->scl_low_();
    esphome::delayMicroseconds(half_period_usec);
    this->scl_release_unchecked_();
    esphome::delayMicroseconds(half_period_usec);

    // The device may be stretching the clock. Wait for it, but not forever;
    // there is no specified maximum. Feed the watchdog while waiting, since
    // resetting the MCU would not recover the bus anyway.
    uint16_t wait = 250;
    while (wait-- != 0 && !this->read_scl_()) {
      App.feed_wdt();
      esphome::delayMicroseconds(half_period_usec * 2);
    }
    if (!this->read_scl_()) {
      ESP_LOGE(TAG, "Recovery failed: SCL is held LOW during clock pulse cycle");
      this->recovery_result_ = RECOVERY_FAILED_SCL_LOW;
      return;
    }
  }

  // By now any stuck device ought to have clocked out its remaining bits and
  // released SDA, letting the pull-up take it high.
  if (!this->read_sda_()) {
    ESP_LOGE(TAG, "Recovery failed: SDA is held LOW after clock pulse cycle");
    this->recovery_result_ = RECOVERY_FAILED_SDA_LOW;
    return;
  }

  // From the specification:
  // "I2C-bus compatible devices must reset their bus logic on receipt of a
  //  START or repeated START condition such that they all anticipate the
  //  sending of a target address."
  // The nine pulses above may have drained one byte while the device still has
  // more to send, so issue a START immediately followed by a STOP to snap it
  // out of the transfer. Both lines are high here, so pulling SDA low is a
  // START and releasing it again is a STOP.
  esphome::delayMicroseconds(half_period_usec);
  this->sda_low_();
  esphome::delayMicroseconds(half_period_usec);
  this->sda_release_();

  this->recovery_result_ = RECOVERY_COMPLETED;
}

}  // namespace esphome::i2c

#endif  // USE_BK72XX
