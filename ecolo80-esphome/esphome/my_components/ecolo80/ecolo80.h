#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ecolo80 {

class Ecolo80Component;





class Ecolo80PowerSwitch : public switch_::Switch {
 public:
  void set_parent(Ecolo80Component *parent) { this->parent_ = parent; }

 protected:
  void write_state(bool state) override;
  Ecolo80Component *parent_{nullptr};
};

class Ecolo80TemperatureUpButton : public button::Button {
 public:
  void set_parent(Ecolo80Component *parent) { this->parent_ = parent; }

 protected:
  void press_action() override;
  Ecolo80Component *parent_{nullptr};
};

class Ecolo80TemperatureDownButton : public button::Button {
 public:
  void set_parent(Ecolo80Component *parent) { this->parent_ = parent; }

 protected:
  void press_action() override;
  Ecolo80Component *parent_{nullptr};
};

class Ecolo80Component : public Component {
 public:
  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }
  void set_transmit_pin(InternalGPIOPin *pin) { this->transmit_pin_ = pin; }

  void set_target_temperature_sensor(sensor::Sensor *value) { this->target_temperature_sensor_ = value; }
  void set_current_temperature_sensor(sensor::Sensor *value) { this->current_temperature_sensor_ = value; }
  void set_power_switch(Ecolo80PowerSwitch *value) { this->power_switch_ = value; }
  void set_heating_sensor(binary_sensor::BinarySensor *value) { this->heating_sensor_ = value; }

  void setup() override;
  void loop() override;
  void dump_config() override;

  float get_setup_priority() const override {
    return setup_priority::HARDWARE;
  }

  void request_power(bool power_on);
  void request_target_temperature(float target_temperature_f);
  void request_temperature_up();
  void request_temperature_down();

 protected:
  static constexpr uint16_t BUFFER_SIZE = 512;
  static constexpr uint16_t FRAME_BITS = 112;
  static constexpr uint8_t FRAME_BYTES = 14;
  static constexpr uint16_t TX_SYMBOL_COUNT = 1 + FRAME_BITS + 1;

  struct EdgeEvent {
    uint32_t duration_us;
    bool level;
  };

  enum class DecodeState : uint8_t {
    WAIT_GAP,
    WAIT_HEADER,
    WAIT_BIT_LOW,
    WAIT_BIT_HIGH,
  };

  static void gpio_interrupt(Ecolo80Component *component);
  void handle_interrupt_();

  static bool IRAM_ATTR rmt_tx_done_callback_(
      rmt_channel_handle_t channel,
      const rmt_tx_done_event_data_t *event_data,
      void *user_data
  );

  bool setup_rmt_();
  void teardown_rmt_();

  bool start_rmt_frame_(
      const std::array<uint8_t, FRAME_BYTES> &frame
  );

  void build_rmt_symbols_(
      const std::array<uint8_t, FRAME_BYTES> &frame
  );

  void finish_rmt_frame_();
  bool bus_is_idle_() const;



  void service_desired_target_queue_();

  void process_event_(const EdgeEvent &event);
  void begin_frame_();
  void append_bit_(bool bit);
  void complete_frame_();
  void reset_decoder_();

  uint8_t reverse_bits_(uint8_t value) const;
  float decode_temperature_f_(uint8_t code) const;
  uint8_t encode_temperature_f_(float temperature_f) const;

  uint8_t calculate_checksum_(
      const std::array<uint8_t, FRAME_BYTES> &frame
  ) const;

  bool checksum_valid_(
      const std::array<uint8_t, FRAME_BYTES> &frame
  ) const;

  std::string frame_to_hex_(
      const std::array<uint8_t, FRAME_BYTES> &frame
  ) const;

  std::string current_frame_to_hex_() const;
  const char *identify_frame_type_() const;

  const char *describe_byte_(
      uint8_t packet_type,
      uint8_t byte_index
  ) const;

  void analyze_packet_changes_(
      uint8_t packet_type,
      const std::array<uint8_t, FRAME_BYTES> &previous_packet,
      bool have_previous_packet
  );

  bool command_template_available_() const;
  std::array<uint8_t, FRAME_BYTES> build_command_from_status_() const;
  std::array<uint8_t, FRAME_BYTES> build_power_command_(bool power_on) const;
  std::array<uint8_t, FRAME_BYTES> build_target_command_(float target_temperature_f) const;
  std::array<uint8_t, FRAME_BYTES> build_target_step_command_(int8_t half_degree_steps) const;

  void finalize_command_(
      std::array<uint8_t, FRAME_BYTES> &command
  ) const;

  void log_command_preview_(
      const char *description,
      const std::array<uint8_t, FRAME_BYTES> &command
  ) const;

  void log_command_builder_examples_();
  void log_frame_timing_(uint8_t frame_type, uint32_t frame_time_us);
  void analyze_tx_echo_(
      const std::array<uint8_t, FRAME_BYTES> &frame
  );

  InternalGPIOPin *pin_{nullptr};
  InternalGPIOPin *transmit_pin_{nullptr};
  ISRInternalGPIOPin isr_pin_;

  rmt_channel_handle_t rmt_tx_channel_{nullptr};
  rmt_encoder_handle_t rmt_copy_encoder_{nullptr};

  std::array<rmt_symbol_word_t, TX_SYMBOL_COUNT> rmt_symbols_{};

  volatile bool rmt_transmitting_{false};
  volatile bool rmt_tx_done_{false};

  uint32_t rmt_tx_start_us_{0};
  uint32_t rmt_planned_duration_us_{0};
  uint32_t rmt_idle_before_tx_us_{0};


  sensor::Sensor *target_temperature_sensor_{nullptr};
  sensor::Sensor *current_temperature_sensor_{nullptr};

  Ecolo80PowerSwitch *power_switch_{nullptr};
  binary_sensor::BinarySensor *heating_sensor_{nullptr};


  volatile EdgeEvent buffer_[BUFFER_SIZE];
  volatile uint16_t write_index_{0};
  volatile uint16_t read_index_{0};
  volatile bool buffer_overflow_{false};
  volatile uint32_t pending_edges_{0};
  volatile uint32_t total_edges_{0};
  volatile uint32_t last_edge_us_{0};
  volatile bool last_level_{false};

  uint32_t startup_ms_{0};
  uint32_t last_publish_ms_{0};

  DecodeState decode_state_{DecodeState::WAIT_GAP};
  std::array<uint8_t, FRAME_BYTES> frame_{};
  uint16_t bit_count_{0};

  uint32_t valid_frame_count_{0};
  uint32_t rejected_frame_count_{0};
  uint32_t checksum_error_count_{0};

  bool last_power_state_{false};
  bool have_power_state_{false};

  bool last_heating_state_{false};
  bool have_heating_state_{false};

  uint8_t last_target_code_{0};
  bool have_target_code_{false};

  uint8_t last_current_temperature_code_{0};
  bool have_current_temperature_{false};

  std::array<uint8_t, FRAME_BYTES> previous_packet_a_{};
  std::array<uint8_t, FRAME_BYTES> previous_packet_b_{};
  bool have_previous_packet_a_{false};
  bool have_previous_packet_b_{false};

  uint32_t last_any_frame_us_{0};
  uint32_t last_packet_a_us_{0};
  uint32_t last_packet_b_us_{0};
  uint32_t last_keypad_us_{0};

  bool have_last_any_frame_{false};
  bool have_last_packet_a_time_{false};
  bool have_last_packet_b_time_{false};
  bool have_last_keypad_time_{false};

  std::array<uint8_t, FRAME_BYTES> command_template_{};
  bool have_command_template_{false};
  bool command_examples_logged_{false};

  bool power_sequence_active_{false};
  bool requested_power_state_{false};
  std::array<uint8_t, FRAME_BYTES> pending_power_command_{};

  bool target_sequence_active_{false};
  float requested_target_temperature_f_{0.0f};
  bool desired_target_active_{false};
  float desired_target_temperature_f_{0.0f};
  uint32_t desired_target_send_after_ms_{0};
  std::array<uint8_t, FRAME_BYTES> pending_target_command_{};
  uint8_t target_copy_index_{0};
  uint32_t next_target_copy_ms_{0};
  uint32_t target_ack_deadline_ms_{0};

  std::array<uint8_t, FRAME_BYTES> pending_tx_command_{};

  bool tx_echo_expected_{false};
  bool tx_echo_seen_for_copy_{false};
  uint8_t tx_echo_expected_copy_{0};
  uint32_t tx_echo_deadline_ms_{0};
  uint32_t tx_echo_valid_count_{0};
  uint32_t tx_echo_mismatch_count_{0};

  uint8_t power_copy_index_{0};
  uint32_t current_tx_start_ms_{0};
  uint32_t next_power_copy_ms_{0};
  uint32_t power_ack_deadline_ms_{0};
};

}  // namespace ecolo80
}  // namespace esphome
