#pragma once

#ifdef USE_BK72XX

#include <cstddef>
#include <cstdint>

#include "esphome/core/component.h"
#include "i2c_bus.h"

namespace esphome::i2c {

enum RecoveryCode {
  RECOVERY_FAILED_SCL_LOW,
  RECOVERY_FAILED_SDA_LOW,
  RECOVERY_COMPLETED,
};

/// Bit-banged I2C controller for the Beken BK72xx family (Tuya CBU, CB3S, ...).
///
/// Why this exists rather than reusing ArduinoI2CBus: LibreTiny ships no Wire
/// library for the beken-72xx core. Only realtek-amb and lightning-ln882h have
/// one; cores/beken-72xx/arduino/libraries/ contains just Serial and WiFi, and
/// implementing Wire for BK is still an open item upstream. The chip's two
/// hardware I2C controllers are unusable here for three independent reasons:
/// the vendor SDK drivers busy-wait with no timeout (an absent or NACKing
/// device hangs the CPU until the watchdog fires, so a bus scan deadlocks on
/// the first empty address), their register-oriented API cannot express the
/// zero-length address probe a scan needs, and their pins are fixed second
/// functions that collide with JTAG/flash and the log console.
///
/// So this is a true open-drain software controller: a line is driven LOW by
/// switching the pin to OUTPUT-LOW and released by switching it back to INPUT.
/// The HIGH level always comes from the pull-up resistors, never from the MCU.
///
/// EXTERNAL PULL-UPS ARE REQUIRED (4.7k to 3V3 on both SDA and SCL). The
/// BK7231N does have internal pull-ups, but they are too weak to hold an I2C
/// bus. Their absence is by far the most common failure mode, which is why
/// setup() reports a non-idle bus explicitly instead of letting every transfer
/// fail with a generic error.
class BK72xxI2CBus final : public InternalI2CBus, public Component {
 public:
  void setup() override;
  void dump_config() override;
  ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                        size_t read_count) override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  void set_scan(bool scan) { this->scan_ = scan; }
  void set_sda_pin(uint8_t sda_pin) { this->sda_pin_ = sda_pin; }
  void set_scl_pin(uint8_t scl_pin) { this->scl_pin_ = scl_pin; }
  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }
  /// Clock-stretch budget in microseconds. This is how long a target may hold
  /// SCL low before a transfer is abandoned with ERROR_TIMEOUT.
  void set_timeout(uint32_t timeout) { this->timeout_us_ = timeout; }

  /// Software bus, so there is no hardware peripheral index to report.
  int get_port() const override { return 0; }

 protected:
  // --- Line control. "release" never drives high; the pull-up does. ---
  void sda_low_();
  void sda_release_();
  void scl_low_();
  /// Releases SCL and waits for it to actually read high, which is how a
  /// clock-stretching target is detected. False on timeout (sets error_).
  bool scl_release_();
  /// Releases SCL without the blocking poll and without touching error_. Used
  /// by stop_() and recover_(), where SCL may legitimately still be stuck and
  /// where clobbering the in-flight error code would mask the real fault.
  void scl_release_unchecked_();
  bool read_sda_();
  bool read_scl_();
  void wait_();

  // --- Framing. These never set error_ except for the stretch timeout; the
  // public entry point labels a false return, so a timeout is never
  // misreported as a NACK. ---
  bool start_(uint8_t address, bool read);
  bool repeated_start_(uint8_t address, bool read);
  void stop_();
  bool write_byte_(uint8_t data);
  uint8_t read_byte_(bool ack);

  /// True when both lines read high, i.e. the bus is idle and the pull-ups are
  /// actually doing their job.
  bool bus_idle_();
  void recover_();

  uint8_t sda_pin_{0};
  uint8_t scl_pin_{0};
  uint32_t frequency_{0};
  uint32_t timeout_us_{10000};
  /// Quarter bit period. Derived from frequency_ in setup().
  uint16_t delay_us_{5};
  bool initialized_{false};
  bool bus_idle_at_setup_{true};
  /// Sticky per-transfer error. Set once and only when still ERROR_OK, so the
  /// first real cause survives to the caller.
  ErrorCode error_{ERROR_OK};
  RecoveryCode recovery_result_{RECOVERY_COMPLETED};
  bool recovery_attempted_{false};
};

}  // namespace esphome::i2c

#endif  // USE_BK72XX
