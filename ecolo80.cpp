#include "ecolo80.h"

#include <cinttypes>
#include <cmath>
#include <cstdio>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ecolo80 {

static const char *const TAG = "ecolo80";

static constexpr uint32_t GAP_MIN_US = 7000;
static constexpr uint32_t GAP_MAX_US = 11000;
static constexpr uint32_t HEADER_MIN_US = 3800;
static constexpr uint32_t HEADER_MAX_US = 5200;
static constexpr uint32_t BIT_LOW_MIN_US = 650;
static constexpr uint32_t BIT_LOW_MAX_US = 1450;
static constexpr uint32_t BIT_ZERO_MIN_US = 650;
static constexpr uint32_t BIT_ZERO_MAX_US = 1700;
static constexpr uint32_t BIT_ONE_MIN_US = 2200;
static constexpr uint32_t BIT_ONE_MAX_US = 3800;

static constexpr uint32_t TX_RESOLUTION_HZ = 1000000;
static constexpr uint32_t TX_GAP_LOW_US = 9000;
static constexpr uint32_t TX_HEADER_HIGH_US = 4500;
static constexpr uint32_t TX_BIT_LOW_US = 1000;
static constexpr uint32_t TX_ZERO_HIGH_US = 1000;
static constexpr uint32_t TX_ONE_HIGH_US = 3000;
static constexpr uint32_t TX_TERMINATOR_LOW_US = 1000;
static constexpr uint32_t TX_TERMINATOR_RELEASE_US = 1;

static constexpr uint32_t REQUIRED_IDLE_US = 12000;
static constexpr uint32_t POST_CONTROLLER_FRAME_GUARD_US = 250000;

static constexpr uint32_t POWER_REPEAT_MS = 820;
static constexpr uint32_t POWER_ACK_TIMEOUT_MS = 12000;
static constexpr uint8_t POWER_MAX_COPIES = 4;
static constexpr uint32_t TARGET_ACK_TIMEOUT_MS = 9000;
static constexpr uint32_t TARGET_REPEAT_MS = 500;
static constexpr uint8_t TARGET_MAX_COPIES = 2;
static constexpr uint32_t COMMAND_PREVIEW_DELAY_MS = 15000;

void Ecolo80ModelSelect::control(size_t index) {
  if (this->parent_ != nullptr) {
    this->parent_->set_model_index(index, true);
  }

  this->publish_state(index);
}

void Ecolo80PowerSwitch::write_state(bool state) {
  if (this->parent_ != nullptr) {
    this->parent_->request_power(state);
  }
}

void Ecolo80TemperatureUpButton::press_action() {
  if (this->parent_ != nullptr) {
    this->parent_->request_temperature_up();
  }
}

void Ecolo80TemperatureDownButton::press_action() {
  if (this->parent_ != nullptr) {
    this->parent_->request_temperature_down();
  }
}

void Ecolo80Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ECOLO component v15 final with 5-second consumption refresh...");

  if (this->model_select_ != nullptr) {
    this->model_preference_ =
        global_preferences->make_preference<uint8_t>(
            this->model_select_->get_object_id_hash()
        );

    uint8_t stored_model_index = 2;

    if (!this->model_preference_.load(
            &stored_model_index) ||
        stored_model_index >= 5) {
      stored_model_index = 2;
    }

    this->apply_model_index_(
        stored_model_index
    );

    this->model_select_->publish_state(
        stored_model_index
    );
  } else {
    this->apply_model_index_(2);
  }

  if (this->pin_ == nullptr || this->transmit_pin_ == nullptr) {
    ESP_LOGE(TAG, "Receive or transmit GPIO is not configured");
    this->mark_failed();
    return;
  }

  /*
   * LOW on the configured transmit pin keeps the 2N3904 off and releases the blue bus.
   * RMT then takes ownership of the same GPIO.
   */
  this->transmit_pin_->setup();
  this->transmit_pin_->digital_write(false);

  if (!this->setup_rmt_()) {
    this->mark_failed();
    return;
  }

  this->pin_->setup();
  this->isr_pin_ = this->pin_->to_isr();

  this->last_level_ = this->isr_pin_.digital_read();
  this->startup_ms_ = millis();
  this->last_publish_ms_ = millis();
  this->last_edge_us_ = micros();

  this->previous_packet_a_.fill(0);
  this->previous_packet_b_.fill(0);
  this->command_template_.fill(0);
  this->pending_power_command_.fill(0);

  this->reset_decoder_();

  this->pin_->attach_interrupt(
      &Ecolo80Component::gpio_interrupt,
      this,
      gpio::INTERRUPT_ANY_EDGE
  );

  ESP_LOGCONFIG(TAG, "Receiver interrupt attached");
  ESP_LOGCONFIG(TAG, "RMT transmitter ready; idle output LOW");
  this->publish_consumption_state_(false);
  this->last_consumption_publish_ms_ = millis();
}

void Ecolo80Component::dump_config() {
  ESP_LOGCONFIG(TAG, "ECOLO integration v15 final:");
  ESP_LOGCONFIG(TAG, "  Temperature click collection: 1200 ms");
  ESP_LOGCONFIG(TAG, "  Temperature TX copies: 2 at 500 ms spacing");
  ESP_LOGCONFIG(TAG, "  Temperature ACK timeout: 9000 ms");
  ESP_LOGCONFIG(TAG, "  Selected model: %s", this->model_name_.c_str());
  ESP_LOGCONFIG(
      TAG,
      "  Rated running current: %.1f A",
      this->rated_current_amps_
  );
  ESP_LOGCONFIG(
      TAG,
      "  Estimated running consumption: %.0f W at %.0f V",
      this->rated_current_amps_ * SUPPLY_VOLTAGE,
      SUPPLY_VOLTAGE
  );
  ESP_LOGCONFIG(
      TAG,
      "  Consumption refresh interval: %" PRIu32 " ms",
      CONSUMPTION_REFRESH_MS
  );
  LOG_PIN("  Receive pin: ", this->pin_);
  LOG_PIN("  RMT transmit pin: ", this->transmit_pin_);

  ESP_LOGCONFIG(TAG, "  Receiver: enabled");
  ESP_LOGCONFIG(TAG, "  Checksum validation: enabled");
  ESP_LOGCONFIG(TAG, "  RMT resolution: %" PRIu32 " Hz", TX_RESOLUTION_HZ);
  ESP_LOGCONFIG(TAG, "  RMT symbols per frame: %u", TX_SYMBOL_COUNT);
  ESP_LOGCONFIG(
      TAG,
      "  End-of-frame terminator: %" PRIu32 " us bus LOW",
      TX_TERMINATOR_LOW_US
  );
  ESP_LOGCONFIG(
      TAG,
      "  Post-controller-frame guard: %" PRIu32 " us",
      POST_CONTROLLER_FRAME_GUARD_US
  );
  ESP_LOGCONFIG(TAG, "  Power command repeat: %" PRIu32 " ms", POWER_REPEAT_MS);
  ESP_LOGCONFIG(TAG, "  Maximum command copies: %u", POWER_MAX_COPIES);
  ESP_LOGCONFIG(TAG, "  Physical TX echo decoding on GPIO4: enabled");
}

bool Ecolo80Component::setup_rmt_() {
  rmt_tx_channel_config_t channel_config{};
  channel_config.clk_src = RMT_CLK_SRC_DEFAULT;
  channel_config.gpio_num =
      static_cast<gpio_num_t>(this->transmit_pin_->get_pin());
  channel_config.mem_block_symbols = 128;
  channel_config.resolution_hz = TX_RESOLUTION_HZ;
  channel_config.trans_queue_depth = 2;
  channel_config.flags.invert_out = false;
  channel_config.flags.with_dma = false;

  esp_err_t result =
      rmt_new_tx_channel(
          &channel_config,
          &this->rmt_tx_channel_
      );

  if (result != ESP_OK) {
    ESP_LOGE(
        TAG,
        "rmt_new_tx_channel failed: %s",
        esp_err_to_name(result)
    );
    return false;
  }

  rmt_copy_encoder_config_t encoder_config{};

  result =
      rmt_new_copy_encoder(
          &encoder_config,
          &this->rmt_copy_encoder_
      );

  if (result != ESP_OK) {
    ESP_LOGE(
        TAG,
        "rmt_new_copy_encoder failed: %s",
        esp_err_to_name(result)
    );
    this->teardown_rmt_();
    return false;
  }

  rmt_tx_event_callbacks_t callbacks{};
  callbacks.on_trans_done =
      &Ecolo80Component::rmt_tx_done_callback_;

  result =
      rmt_tx_register_event_callbacks(
          this->rmt_tx_channel_,
          &callbacks,
          this
      );

  if (result != ESP_OK) {
    ESP_LOGE(
        TAG,
        "rmt_tx_register_event_callbacks failed: %s",
        esp_err_to_name(result)
    );
    this->teardown_rmt_();
    return false;
  }

  result = rmt_enable(this->rmt_tx_channel_);

  if (result != ESP_OK) {
    ESP_LOGE(
        TAG,
        "rmt_enable failed: %s",
        esp_err_to_name(result)
    );
    this->teardown_rmt_();
    return false;
  }

  return true;
}

void Ecolo80Component::teardown_rmt_() {
  if (this->rmt_tx_channel_ != nullptr) {
    rmt_disable(this->rmt_tx_channel_);
  }

  if (this->rmt_copy_encoder_ != nullptr) {
    rmt_del_encoder(this->rmt_copy_encoder_);
    this->rmt_copy_encoder_ = nullptr;
  }

  if (this->rmt_tx_channel_ != nullptr) {
    rmt_del_channel(this->rmt_tx_channel_);
    this->rmt_tx_channel_ = nullptr;
  }
}

bool IRAM_ATTR Ecolo80Component::rmt_tx_done_callback_(
    rmt_channel_handle_t channel,
    const rmt_tx_done_event_data_t *event_data,
    void *user_data
) {
  auto *component =
      static_cast<Ecolo80Component *>(user_data);

  component->rmt_tx_done_ = true;
  return false;
}

bool Ecolo80Component::bus_is_idle_() const {
  uint32_t last_edge;

  {
    InterruptLock lock;
    last_edge = this->last_edge_us_;
  }

  const uint32_t now_us = micros();
  const uint32_t edge_idle_us = now_us - last_edge;

  if (edge_idle_us < REQUIRED_IDLE_US) {
    return false;
  }

  /*
   * Do not begin a command immediately after Packet A/B. The physical
   * keypad leaves a much larger window around controller frames.
   */
  if (this->have_last_any_frame_) {
    const uint32_t since_frame_us =
        now_us - this->last_any_frame_us_;

    if (since_frame_us < POST_CONTROLLER_FRAME_GUARD_US) {
      return false;
    }
  }

  return true;
}

void Ecolo80Component::build_rmt_symbols_(
    const std::array<uint8_t, FRAME_BYTES> &frame
) {
  size_t symbol_index = 0;
  uint32_t planned_duration_us = 0;

  /*
   * GPIO HIGH turns on the 2N3904 and pulls the blue bus LOW.
   * GPIO LOW turns it off and releases the blue bus HIGH.
   */
  this->rmt_symbols_[symbol_index].level0 = 1;
  this->rmt_symbols_[symbol_index].duration0 = TX_GAP_LOW_US;
  this->rmt_symbols_[symbol_index].level1 = 0;
  this->rmt_symbols_[symbol_index].duration1 = TX_HEADER_HIGH_US;
  planned_duration_us += TX_GAP_LOW_US + TX_HEADER_HIGH_US;
  symbol_index++;

  for (uint8_t byte_index = 0;
       byte_index < FRAME_BYTES;
       byte_index++) {
    const uint8_t value = frame[byte_index];

    for (int bit_index = 7;
         bit_index >= 0;
         bit_index--) {
      const bool bit =
          (value & (1U << bit_index)) != 0;

      this->rmt_symbols_[symbol_index].level0 = 1;
      this->rmt_symbols_[symbol_index].duration0 =
          TX_BIT_LOW_US;

      this->rmt_symbols_[symbol_index].level1 = 0;
      this->rmt_symbols_[symbol_index].duration1 =
          bit ? TX_ONE_HIGH_US : TX_ZERO_HIGH_US;

      planned_duration_us +=
          TX_BIT_LOW_US +
          (bit ? TX_ONE_HIGH_US : TX_ZERO_HIGH_US);

      symbol_index++;
    }
  }

  /*
   * Critical final edge:
   *
   * The final data bit ends while the bus is released HIGH. Pulling the
   * bus LOW here gives the receiver the falling edge needed to measure
   * and latch that last HIGH pulse (including the checksum's final bit).
   * The second half explicitly releases the bus again.
   */
  this->rmt_symbols_[symbol_index].level0 = 1;
  this->rmt_symbols_[symbol_index].duration0 =
      TX_TERMINATOR_LOW_US;
  this->rmt_symbols_[symbol_index].level1 = 0;
  this->rmt_symbols_[symbol_index].duration1 =
      TX_TERMINATOR_RELEASE_US;

  planned_duration_us +=
      TX_TERMINATOR_LOW_US +
      TX_TERMINATOR_RELEASE_US;

  symbol_index++;

  this->rmt_planned_duration_us_ =
      planned_duration_us;

  if (symbol_index != TX_SYMBOL_COUNT) {
    ESP_LOGE(
        TAG,
        "RMT symbol builder mismatch: built=%u expected=%u",
        static_cast<unsigned>(symbol_index),
        TX_SYMBOL_COUNT
    );
  }
}

bool Ecolo80Component::start_rmt_frame_(
    const std::array<uint8_t, FRAME_BYTES> &frame
) {
  if (this->rmt_tx_channel_ == nullptr ||
      this->rmt_copy_encoder_ == nullptr) {
    ESP_LOGE(TAG, "RMT transmit rejected: transmitter unavailable");
    return false;
  }

  if (this->rmt_transmitting_) {
    return false;
  }

  if (!this->bus_is_idle_()) {
    return false;
  }

  uint32_t last_edge;

  {
    InterruptLock lock;
    last_edge = this->last_edge_us_;
  }

  this->rmt_idle_before_tx_us_ =
      micros() - last_edge;

  this->build_rmt_symbols_(frame);

  {
    InterruptLock lock;
    this->rmt_transmitting_ = true;
    this->rmt_tx_done_ = false;
    this->buffer_overflow_ = false;
  }

  /*
   * Keep GPIO4 reception active so the exact waveform returning from
   * the blue wire is decoded as a physical TX echo.
   */
  this->reset_decoder_();

  rmt_transmit_config_t transmit_config{};
  transmit_config.loop_count = 0;
  transmit_config.flags.eot_level = 0;
  transmit_config.flags.queue_nonblocking = true;

  this->tx_echo_expected_ = true;
  this->tx_echo_seen_for_copy_ = false;
  this->tx_echo_expected_copy_ =
      static_cast<uint8_t>(this->power_copy_index_ + 1U);
  this->tx_echo_deadline_ms_ = millis() + 1500UL;

  this->rmt_tx_start_us_ = micros();

  const esp_err_t result =
      rmt_transmit(
          this->rmt_tx_channel_,
          this->rmt_copy_encoder_,
          this->rmt_symbols_.data(),
          sizeof(this->rmt_symbols_),
          &transmit_config
      );

  if (result != ESP_OK) {
    {
      InterruptLock lock;
      this->rmt_transmitting_ = false;
      this->rmt_tx_done_ = false;
    }

    this->tx_echo_expected_ = false;
    this->tx_echo_seen_for_copy_ = false;

    ESP_LOGE(
        TAG,
        "rmt_transmit failed: %s",
        esp_err_to_name(result)
    );

    return false;
  }

  return true;
}

void Ecolo80Component::finish_rmt_frame_() {
  const uint32_t finish_us = micros();
  const uint32_t elapsed_us =
      finish_us - this->rmt_tx_start_us_;

  {
    InterruptLock lock;

    /*
     * Do not clear the RX buffer here. It contains the physical echo
     * edges generated by this RMT transmission.
     */
    this->buffer_overflow_ = false;
    this->last_level_ = this->isr_pin_.digital_read();

    this->rmt_tx_done_ = false;
    this->rmt_transmitting_ = false;
  }

  ESP_LOGI(
      TAG,
      "RMT FRAME DIAGNOSTIC: planned=%" PRIu32
      " us actual=%" PRIu32
      " us idle_before=%" PRIu32
      " us terminator_low=%" PRIu32 " us",
      this->rmt_planned_duration_us_,
      elapsed_us,
      this->rmt_idle_before_tx_us_,
      TX_TERMINATOR_LOW_US
  );

  ESP_LOGI(
      TAG,
      "RMT RX echo remains enabled; waiting for decoded 0x33 frame"
  );
}



void Ecolo80Component::request_power(bool power_on) {
  if (!this->command_template_available_()) {
    ESP_LOGW(TAG, "Power command rejected: no valid Packet A template yet");
    return;
  }

  if (this->power_sequence_active_ ||
      this->target_sequence_active_ ||
      this->rmt_transmitting_ ||
      this->desired_target_active_) {
    ESP_LOGW(TAG, "Power command rejected: another sequence is active");
    return;
  }

  if (this->have_power_state_ &&
      this->last_power_state_ == power_on) {
    ESP_LOGI(
        TAG,
        "Power command not needed: heater is already %s",
        power_on ? "ON" : "OFF"
    );
    return;
  }

  this->pending_power_command_ =
      this->build_power_command_(power_on);

  if (!this->checksum_valid_(
          this->pending_power_command_)) {
    ESP_LOGE(TAG, "Power command rejected: generated checksum is invalid");
    return;
  }

  this->pending_tx_command_ =
      this->pending_power_command_;

  this->requested_power_state_ = power_on;
  this->power_copy_index_ = 0;
  this->next_power_copy_ms_ = millis();
  this->power_ack_deadline_ms_ =
      millis() + POWER_ACK_TIMEOUT_MS;
  this->power_sequence_active_ = true;

  ESP_LOGI(
      TAG,
      "RMT POWER %s QUEUED: %s",
      power_on ? "ON" : "OFF",
      this->frame_to_hex_(
          this->pending_tx_command_
      ).c_str()
  );
}



void Ecolo80Component::request_temperature_up() {
  if (!this->have_target_code_) {
    ESP_LOGW(TAG, "Temperature UP rejected: current target is not known yet");
    return;
  }

  float base_target =
      this->decode_temperature_f_(this->last_target_code_);

  if (this->desired_target_active_) {
    base_target = this->desired_target_temperature_f_;
  } else if (this->target_sequence_active_) {
    base_target = this->requested_target_temperature_f_;
  }

  const float desired =
      std::min(104.0f, base_target + 1.0f);

  if (desired <= base_target) {
    ESP_LOGI(TAG, "Temperature UP ignored: already at maximum 104 F");
    return;
  }

  this->desired_target_temperature_f_ = desired;
  this->desired_target_active_ = true;
  this->desired_target_send_after_ms_ =
      millis() + 1200UL;

  ESP_LOGI(
      TAG,
      "Temperature desired target updated to %.0f F",
      this->desired_target_temperature_f_
  );

  this->service_desired_target_queue_();
}

void Ecolo80Component::request_temperature_down() {
  if (!this->have_target_code_) {
    ESP_LOGW(TAG, "Temperature DOWN rejected: current target is not known yet");
    return;
  }

  float base_target =
      this->decode_temperature_f_(this->last_target_code_);

  if (this->desired_target_active_) {
    base_target = this->desired_target_temperature_f_;
  } else if (this->target_sequence_active_) {
    base_target = this->requested_target_temperature_f_;
  }

  const float desired =
      std::max(59.0f, base_target - 1.0f);

  if (desired >= base_target) {
    ESP_LOGI(TAG, "Temperature DOWN ignored: already at minimum 59 F");
    return;
  }

  this->desired_target_temperature_f_ = desired;
  this->desired_target_active_ = true;
  this->desired_target_send_after_ms_ =
      millis() + 1200UL;

  ESP_LOGI(
      TAG,
      "Temperature desired target updated to %.0f F",
      this->desired_target_temperature_f_
  );

  this->service_desired_target_queue_();
}

void Ecolo80Component::service_desired_target_queue_() {
  if (!this->desired_target_active_ ||
      this->target_sequence_active_ ||
      this->power_sequence_active_ ||
      this->rmt_transmitting_ ||
      !this->have_target_code_) {
    return;
  }

  const uint32_t now_ms = millis();

  if (static_cast<int32_t>(
          now_ms -
          this->desired_target_send_after_ms_
      ) < 0) {
    return;
  }

  const float current_target =
      this->decode_temperature_f_(
          this->last_target_code_
      );

  if (fabsf(
          current_target -
          this->desired_target_temperature_f_
      ) < 0.1f) {
    ESP_LOGI(
        TAG,
        "Desired temperature reached: %.0f F",
        current_target
    );

    this->desired_target_active_ = false;
    return;
  }

  const float final_target =
      this->desired_target_temperature_f_;

  ESP_LOGI(
      TAG,
      "Sending final desired target directly: %.0f F",
      final_target
  );

  this->request_target_temperature(
      final_target
  );
}


void Ecolo80Component::request_target_temperature(
    float target_temperature_f
) {
  if (!this->command_template_available_()) {
    ESP_LOGW(TAG, "Target command rejected: no valid Packet A template yet");
    return;
  }

  if (this->power_sequence_active_ ||
      this->target_sequence_active_ ||
      this->rmt_transmitting_) {
    ESP_LOGW(TAG, "Target command rejected: another sequence is active");
    return;
  }

  const float requested =
      roundf(target_temperature_f);

  if (requested < 59.0f || requested > 104.0f) {
    ESP_LOGW(
        TAG,
        "Target command rejected: %.1f F is outside 59-104 F",
        requested
    );
    return;
  }

  if (this->have_target_code_) {
    const float current_target =
        this->decode_temperature_f_(
            this->last_target_code_
        );

    if (fabsf(current_target - requested) < 0.1f) {
      ESP_LOGI(
          TAG,
          "Target command not needed: heater is already set to %.0f F",
          requested
      );

      return;
    }
  }

  this->pending_target_command_ =
      this->build_target_command_(requested);

  if (!this->checksum_valid_(
          this->pending_target_command_)) {
    ESP_LOGE(TAG, "Target command rejected: generated checksum is invalid");
    return;
  }

  this->pending_tx_command_ =
      this->pending_target_command_;

  this->requested_target_temperature_f_ =
      requested;
  this->target_copy_index_ = 0;
  this->next_target_copy_ms_ = millis();
  this->target_ack_deadline_ms_ =
      millis() + TARGET_ACK_TIMEOUT_MS;
  this->target_sequence_active_ = true;

  ESP_LOGI(
      TAG,
      "RMT TARGET %.0f F QUEUED: %s",
      requested,
      this->frame_to_hex_(
          this->pending_target_command_
      ).c_str()
  );
}


void Ecolo80Component::gpio_interrupt(
    Ecolo80Component *component
) {
  component->handle_interrupt_();
}

void IRAM_ATTR Ecolo80Component::handle_interrupt_() {
  const uint32_t now = micros();
  const bool new_level =
      this->isr_pin_.digital_read();

  const uint32_t duration =
      now - this->last_edge_us_;

  const uint16_t next_index =
      static_cast<uint16_t>(
          (this->write_index_ + 1U) %
          BUFFER_SIZE
      );

  if (next_index == this->read_index_) {
    this->buffer_overflow_ = true;
  } else {
    this->buffer_[this->write_index_].duration_us =
        duration;

    this->buffer_[this->write_index_].level =
        this->last_level_;

    this->write_index_ = next_index;
  }

  this->last_level_ = new_level;
  this->last_edge_us_ = now;

  this->pending_edges_++;
  this->total_edges_++;
}

void Ecolo80Component::reset_decoder_() {
  this->decode_state_ =
      DecodeState::WAIT_GAP;

  this->bit_count_ = 0;
  this->frame_.fill(0);
}

void Ecolo80Component::begin_frame_() {
  this->bit_count_ = 0;
  this->frame_.fill(0);

  this->decode_state_ =
      DecodeState::WAIT_BIT_LOW;
}

void Ecolo80Component::append_bit_(bool bit) {
  if (this->bit_count_ >= FRAME_BITS) {
    this->rejected_frame_count_++;
    this->reset_decoder_();
    return;
  }

  const uint8_t byte_index =
      static_cast<uint8_t>(
          this->bit_count_ / 8U
      );

  this->frame_[byte_index] =
      static_cast<uint8_t>(
          (this->frame_[byte_index] << 1U) |
          (bit ? 1U : 0U)
      );

  this->bit_count_++;

  if (this->bit_count_ == FRAME_BITS) {
    this->complete_frame_();
  } else {
    this->decode_state_ =
        DecodeState::WAIT_BIT_LOW;
  }
}

uint8_t Ecolo80Component::reverse_bits_(
    uint8_t value
) const {
  value = static_cast<uint8_t>(
      ((value & 0xF0U) >> 4U) |
      ((value & 0x0FU) << 4U)
  );

  value = static_cast<uint8_t>(
      ((value & 0xCCU) >> 2U) |
      ((value & 0x33U) << 2U)
  );

  value = static_cast<uint8_t>(
      ((value & 0xAAU) >> 1U) |
      ((value & 0x55U) << 1U)
  );

  return value;
}

float Ecolo80Component::decode_temperature_f_(
    uint8_t code
) const {
  const float celsius =
      static_cast<float>(
          this->reverse_bits_(code)
      ) / 2.0f;

  return roundf(
      (celsius * 9.0f / 5.0f) + 32.0f
  );
}

uint8_t Ecolo80Component::encode_temperature_f_(
    float temperature_f
) const {
  const float celsius =
      (temperature_f - 32.0f) *
      5.0f / 9.0f;

  int half_degrees =
      static_cast<int>(
          roundf(celsius * 2.0f)
      );

  if (half_degrees < 0) {
    half_degrees = 0;
  }

  if (half_degrees > 255) {
    half_degrees = 255;
  }

  return this->reverse_bits_(
      static_cast<uint8_t>(half_degrees)
  );
}

uint8_t Ecolo80Component::calculate_checksum_(
    const std::array<uint8_t, FRAME_BYTES> &frame
) const {
  uint16_t sum = 0;

  for (uint8_t i = 1; i < 13; i++) {
    sum += this->reverse_bits_(frame[i]);
  }

  return this->reverse_bits_(
      static_cast<uint8_t>(
          sum & 0xFFU
      )
  );
}

bool Ecolo80Component::checksum_valid_(
    const std::array<uint8_t, FRAME_BYTES> &frame
) const {
  return this->calculate_checksum_(frame) ==
         frame[13];
}

std::string Ecolo80Component::frame_to_hex_(
    const std::array<uint8_t, FRAME_BYTES> &frame
) const {
  char output[(FRAME_BYTES * 3U) + 1U];
  size_t position = 0;

  for (uint8_t i = 0; i < FRAME_BYTES; i++) {
    const int written =
        snprintf(
            output + position,
            sizeof(output) - position,
            (i == FRAME_BYTES - 1U)
                ? "%02X"
                : "%02X ",
            frame[i]
        );

    if (written <= 0) {
      break;
    }

    position +=
        static_cast<size_t>(written);

    if (position >= sizeof(output)) {
      position =
          sizeof(output) - 1U;
      break;
    }
  }

  output[position] = '\0';
  return std::string(output);
}

std::string
Ecolo80Component::current_frame_to_hex_() const {
  return this->frame_to_hex_(
      this->frame_
  );
}

const char *Ecolo80Component::identify_frame_type_()
    const {
  if (this->frame_[0] == 0x4B) {
    return "Packet A";
  }

  if (this->frame_[0] == 0xBB) {
    return "Packet B";
  }

  if (this->frame_[0] == 0x33) {
    return "Keypad";
  }

  return "Unknown";
}

const char *Ecolo80Component::describe_byte_(
    uint8_t packet_type,
    uint8_t byte_index
) const {
  if (packet_type == 0x4B) {
    if (byte_index == 0) {
      return "Packet A header";
    }

    if (byte_index == 2) {
      return "Target temperature";
    }

    if (byte_index == 7) {
      return "Power/status flags";
    }

    if (byte_index == 13) {
      return "Packet A checksum";
    }

    return "Unknown Packet A field";
  }

  if (packet_type == 0xBB) {
    if (byte_index == 0) {
      return "Packet B header";
    }

    if (byte_index == 1) {
      return "Water temperature";
    }

    if (byte_index == 13) {
      return "Packet B checksum";
    }

    return "Unknown Packet B field";
  }

  return "Unknown field";
}

void Ecolo80Component::analyze_packet_changes_(
    uint8_t packet_type,
    const std::array<uint8_t, FRAME_BYTES>
        &previous_packet,
    bool have_previous_packet
) {
  if (!have_previous_packet) {
    return;
  }

  uint8_t changed_count = 0;

  for (uint8_t i = 0;
       i < FRAME_BYTES;
       i++) {
    if (previous_packet[i] !=
        this->frame_[i]) {
      changed_count++;
    }
  }

  if (changed_count == 0) {
    return;
  }

  const char *packet_name =
      packet_type == 0x4B
          ? "PACKET A"
          : "PACKET B";

  ESP_LOGI(
      TAG,
      "=================================================="
  );

  ESP_LOGI(
      TAG,
      "%s CHANGE DETECTED: %u byte(s) changed",
      packet_name,
      changed_count
  );

  for (uint8_t i = 0;
       i < FRAME_BYTES;
       i++) {
    if (previous_packet[i] ==
        this->frame_[i]) {
      continue;
    }

    ESP_LOGI(
        TAG,
        "%s byte %u: 0x%02X -> 0x%02X (%s)",
        packet_name,
        i,
        previous_packet[i],
        this->frame_[i],
        this->describe_byte_(
            packet_type,
            i
        )
    );
  }

  ESP_LOGI(
      TAG,
      "=================================================="
  );
}

bool Ecolo80Component::command_template_available_()
    const {
  return this->have_command_template_;
}

std::array<uint8_t, Ecolo80Component::FRAME_BYTES>
Ecolo80Component::build_command_from_status_() const {
  std::array<uint8_t, FRAME_BYTES> command{};

  if (!this->command_template_available_()) {
    return command;
  }

  command = this->command_template_;
  this->finalize_command_(command);

  return command;
}

std::array<uint8_t, Ecolo80Component::FRAME_BYTES>
Ecolo80Component::build_power_command_(
    bool power_on
) const {
  std::array<uint8_t, FRAME_BYTES> command =
      this->build_command_from_status_();

  if (!this->command_template_available_()) {
    return command;
  }

  if (power_on) {
    command[7] =
        static_cast<uint8_t>(
            command[7] | 0x02U
        );
  } else {
    command[7] =
        static_cast<uint8_t>(
            command[7] & ~0x02U
        );
  }

  this->finalize_command_(command);
  return command;
}

std::array<uint8_t, Ecolo80Component::FRAME_BYTES>
Ecolo80Component::build_target_command_(
    float target_temperature_f
) const {
  std::array<uint8_t, FRAME_BYTES> command =
      this->build_command_from_status_();

  if (!this->command_template_available_()) {
    return command;
  }

  command[2] =
      this->encode_temperature_f_(
          target_temperature_f
      );

  this->finalize_command_(command);
  return command;
}

std::array<uint8_t, Ecolo80Component::FRAME_BYTES>
Ecolo80Component::build_target_step_command_(
    int8_t half_degree_steps
) const {
  std::array<uint8_t, FRAME_BYTES> command =
      this->build_command_from_status_();

  if (!this->command_template_available_()) {
    return command;
  }

  int half_degrees =
      static_cast<int>(
          this->reverse_bits_(
              command[2]
          )
      );

  half_degrees +=
      static_cast<int>(
          half_degree_steps
      );

  if (half_degrees < 0) {
    half_degrees = 0;
  }

  if (half_degrees > 255) {
    half_degrees = 255;
  }

  command[2] =
      this->reverse_bits_(
          static_cast<uint8_t>(
              half_degrees
          )
      );

  this->finalize_command_(command);
  return command;
}

void Ecolo80Component::finalize_command_(
    std::array<uint8_t, FRAME_BYTES> &command
) const {
  command[0] = 0x33;

  command[13] =
      this->calculate_checksum_(command);
}

void Ecolo80Component::log_command_preview_(
    const char *description,
    const std::array<uint8_t, FRAME_BYTES>
        &command
) const {
  ESP_LOGI(
      TAG,
      "COMMAND PREVIEW - %s: %s",
      description,
      this->frame_to_hex_(command).c_str()
  );

  ESP_LOGI(
      TAG,
      "COMMAND PREVIEW CHECKSUM: %s",
      this->checksum_valid_(command)
          ? "VALID"
          : "INVALID"
  );
}

void Ecolo80Component::log_command_builder_examples_() {
  if (!this->command_template_available_() ||
      this->command_examples_logged_) {
    return;
  }

  this->command_examples_logged_ = true;

  ESP_LOGI(
      TAG,
      "=================================================="
  );

  ESP_LOGI(
      TAG,
      "RMT COMMAND BUILDER READY"
  );

  this->log_command_preview_(
      "Power ON",
      this->build_power_command_(true)
  );

  this->log_command_preview_(
      "Power OFF",
      this->build_power_command_(false)
  );

  ESP_LOGI(
      TAG,
      "=================================================="
  );
}

void Ecolo80Component::log_frame_timing_(
    uint8_t frame_type,
    uint32_t frame_time_us
) {
  const char *name = "Unknown";

  if (frame_type == 0x4B) {
    name = "Packet A";
  } else if (frame_type == 0xBB) {
    name = "Packet B";
  } else if (frame_type == 0x33) {
    name = "Keypad";
  }

  ESP_LOGI(
      TAG,
      "TIMING %s @ %" PRIu32 " us",
      name,
      frame_time_us
  );

  if (this->have_last_any_frame_) {
    ESP_LOGI(
        TAG,
        "TIMING   since previous frame: %"
        PRIu32 " us (%.3f ms)",
        frame_time_us -
            this->last_any_frame_us_,
        static_cast<double>(
            frame_time_us -
            this->last_any_frame_us_
        ) / 1000.0
    );
  }

  if (frame_type == 0x33 &&
      this->have_last_keypad_time_) {
    ESP_LOGI(
        TAG,
        "TIMING   since previous Keypad: %"
        PRIu32 " us (%.3f ms)",
        frame_time_us -
            this->last_keypad_us_,
        static_cast<double>(
            frame_time_us -
            this->last_keypad_us_
        ) / 1000.0
    );
  }

  this->last_any_frame_us_ = frame_time_us;
  this->have_last_any_frame_ = true;

  if (frame_type == 0x4B) {
    this->last_packet_a_us_ = frame_time_us;
    this->have_last_packet_a_time_ = true;
  } else if (frame_type == 0xBB) {
    this->last_packet_b_us_ = frame_time_us;
    this->have_last_packet_b_time_ = true;
  } else if (frame_type == 0x33) {
    this->last_keypad_us_ = frame_time_us;
    this->have_last_keypad_time_ = true;
  }
}


void Ecolo80Component::analyze_tx_echo_(
    const std::array<uint8_t, FRAME_BYTES> &frame
) {
  if (!this->tx_echo_expected_) {
    return;
  }

  this->tx_echo_seen_for_copy_ = true;
  this->tx_echo_expected_ = false;

  bool exact_match = true;
  uint8_t mismatch_count = 0;

  for (uint8_t i = 0; i < FRAME_BYTES; i++) {
    if (frame[i] != this->pending_tx_command_[i]) {
      exact_match = false;
      mismatch_count++;
    }
  }

  if (exact_match) {
    this->tx_echo_valid_count_++;

    ESP_LOGI(
        TAG,
        "TX ECHO VALID copy %u: %s",
        this->tx_echo_expected_copy_,
        this->frame_to_hex_(frame).c_str()
    );

    ESP_LOGI(
        TAG,
        "TX ECHO checksum=VALID exact_match=YES valid_count=%"
        PRIu32,
        this->tx_echo_valid_count_
    );

    return;
  }

  this->tx_echo_mismatch_count_++;

  ESP_LOGW(
      TAG,
      "TX ECHO MISMATCH copy %u: %u byte(s) differ",
      this->tx_echo_expected_copy_,
      mismatch_count
  );

  ESP_LOGW(
      TAG,
      "TX ECHO expected: %s",
      this->frame_to_hex_(
          this->pending_tx_command_
      ).c_str()
  );

  ESP_LOGW(
      TAG,
      "TX ECHO received: %s",
      this->frame_to_hex_(frame).c_str()
  );

  for (uint8_t i = 0; i < FRAME_BYTES; i++) {
    if (frame[i] == this->pending_tx_command_[i]) {
      continue;
    }

    ESP_LOGW(
        TAG,
        "TX ECHO byte %u expected=0x%02X received=0x%02X",
        i,
        this->pending_tx_command_[i],
        frame[i]
    );
  }
}

void Ecolo80Component::apply_model_index_(size_t index) {
  static const char *const MODEL_NAMES[] = {
      "ECOLO 50",
      "ECOLO 65",
      "ECOLO 80",
      "ECOLO 100",
      "ECOLO 120",
  };

  static const float MODEL_CURRENTS[] = {
      10.0f,
      12.0f,
      18.0f,
      21.0f,
      24.0f,
  };

  if (index >= 5) {
    index = 2;
  }

  this->model_index_ = index;
  this->model_name_ = MODEL_NAMES[index];
  this->rated_current_amps_ = MODEL_CURRENTS[index];

  ESP_LOGI(
      TAG,
      "MODEL SELECTED: %s = %.1f A / %.0f W",
      this->model_name_.c_str(),
      this->rated_current_amps_,
      this->rated_current_amps_ * SUPPLY_VOLTAGE
  );

  if (this->have_heating_state_) {
    this->publish_consumption_state_(
        this->last_heating_state_
    );
  } else {
    this->publish_consumption_state_(false);
  }

  this->last_consumption_publish_ms_ = millis();
}

void Ecolo80Component::set_model_index(size_t index, bool save) {
  this->apply_model_index_(index);

  if (save) {
    const uint8_t stored_index =
        static_cast<uint8_t>(
            this->model_index_
        );

    if (!this->model_preference_.save(
            &stored_index)) {
      ESP_LOGW(TAG, "Unable to save ECOLO model selection");
    }
  }
}


void Ecolo80Component::publish_consumption_state_(bool heating) {
  const float current_amps =
      heating ? this->rated_current_amps_ : 0.0f;

  const float power_watts =
      current_amps * SUPPLY_VOLTAGE;

  if (this->current_consumption_sensor_ != nullptr) {
    this->current_consumption_sensor_->publish_state(
        current_amps
    );
  }

  if (this->power_consumption_sensor_ != nullptr) {
    this->power_consumption_sensor_->publish_state(
        power_watts
    );
  }

  ESP_LOGD(
      TAG,
      "CONSUMPTION: %s = %.1f A / %.0f W (%s)",
      heating ? "RUNNING" : "IDLE",
      current_amps,
      power_watts,
      this->model_name_.c_str()
  );
}



void Ecolo80Component::complete_frame_() {
  const std::string frame_hex =
      this->current_frame_to_hex_();

  const uint8_t expected_checksum =
      this->calculate_checksum_(
          this->frame_
      );

  if (!this->checksum_valid_(
          this->frame_)) {
    this->checksum_error_count_++;
    this->rejected_frame_count_++;

    ESP_LOGW(
        TAG,
        "CHECKSUM ERROR: received=0x%02X "
        "expected=0x%02X frame=%s",
        this->frame_[13],
        expected_checksum,
        frame_hex.c_str()
    );

    this->reset_decoder_();
    return;
  }

  this->valid_frame_count_++;

  const uint32_t frame_time_us = micros();

  const char *frame_type =
      this->identify_frame_type_();

  this->log_frame_timing_(
      this->frame_[0],
      frame_time_us
  );

  ESP_LOGI(
      TAG,
      "%s received [frame=%" PRIu32 "]: %s",
      frame_type,
      this->valid_frame_count_,
      frame_hex.c_str()
  );

  if (this->frame_[0] == 0x4B) {
    this->analyze_packet_changes_(
        0x4B,
        this->previous_packet_a_,
        this->have_previous_packet_a_
    );

    this->previous_packet_a_ =
        this->frame_;

    this->have_previous_packet_a_ =
        true;

    this->command_template_ =
        this->frame_;

    this->have_command_template_ =
        true;

    const uint8_t target_code =
        this->frame_[2];

    if (!this->have_target_code_ ||
        target_code !=
            this->last_target_code_) {
      this->last_target_code_ =
          target_code;

      this->have_target_code_ =
          true;

      const float decoded_target =
          this->decode_temperature_f_(
              target_code
          );

      if (this->target_temperature_sensor_ != nullptr) {
        this->target_temperature_sensor_->publish_state(
            decoded_target
        );
      }

    }

    const float decoded_target_for_ack =
        this->decode_temperature_f_(
            target_code
        );

    if (this->target_sequence_active_ &&
        fabsf(
            decoded_target_for_ack -
            this->requested_target_temperature_f_
        ) < 0.1f) {
      this->target_sequence_active_ = false;

      ESP_LOGI(
          TAG,
          "RMT TARGET %.0f F ACKNOWLEDGED after %u copy/copies",
          this->requested_target_temperature_f_,
          this->target_copy_index_
      );

      if (fabsf(
              this->requested_target_temperature_f_ -
              this->desired_target_temperature_f_
          ) < 0.1f) {
        this->desired_target_active_ = false;

        ESP_LOGI(
            TAG,
            "Final desired target reached: %.0f F",
            this->requested_target_temperature_f_
        );
      } else {
        this->desired_target_send_after_ms_ =
            millis() + 200UL;

        this->service_desired_target_queue_();
      }
    }

    const bool power_on =
        (this->frame_[7] & 0x02U) != 0;

    if (!this->have_power_state_ ||
        power_on !=
            this->last_power_state_) {
      this->last_power_state_ =
          power_on;

      this->have_power_state_ =
          true;

      if (this->power_switch_ != nullptr) {
        this->power_switch_->publish_state(
            power_on
        );
      }
    }

    if (this->power_sequence_active_ &&
        power_on ==
            this->requested_power_state_) {
      this->power_sequence_active_ = false;

      ESP_LOGI(
          TAG,
          "RMT POWER %s ACKNOWLEDGED after %u copy/copies",
          power_on ? "ON" : "OFF",
          this->power_copy_index_
      );
    }
  }

  if (this->frame_[0] == 0xBB) {
    this->analyze_packet_changes_(
        0xBB,
        this->previous_packet_b_,
        this->have_previous_packet_b_
    );

    this->previous_packet_b_ =
        this->frame_;

    this->have_previous_packet_b_ =
        true;

    const uint8_t operating_flags =
        this->frame_[10];

    const bool heating =
        (operating_flags & 0x08U) != 0;

    if (!this->have_heating_state_ ||
        heating != this->last_heating_state_) {
      this->last_heating_state_ =
          heating;

      this->have_heating_state_ =
          true;

      ESP_LOGI(
          TAG,
          "HEATING STATUS: %s (Packet B byte 10 = 0x%02X)",
          heating ? "HEATING" : "IDLE",
          operating_flags
      );

      if (this->heating_sensor_ != nullptr) {
        this->heating_sensor_->publish_state(
            heating
        );
      }

      this->publish_consumption_state_(
          heating
      );

      this->last_consumption_publish_ms_ = millis();
    }

    const uint8_t current_code =
        this->frame_[1];

    if (!this->have_current_temperature_ ||
        current_code !=
            this->last_current_temperature_code_) {
      this->last_current_temperature_code_ =
          current_code;

      this->have_current_temperature_ =
          true;

      if (this->current_temperature_sensor_ != nullptr) {
        this->current_temperature_sensor_->publish_state(
            this->decode_temperature_f_(
                current_code
            )
        );
      }
    }
  }

  if (this->frame_[0] == 0x33) {
    this->analyze_tx_echo_(this->frame_);

    ESP_LOGI(
        TAG,
        "KEYPAD ACTIVITY: %s",
        frame_hex.c_str()
    );
  }

  this->reset_decoder_();
}

void Ecolo80Component::process_event_(
    const EdgeEvent &event
) {
  const uint32_t duration =
      event.duration_us;

  if (!event.level &&
      duration >= GAP_MIN_US &&
      duration <= GAP_MAX_US) {
    this->frame_.fill(0);
    this->bit_count_ = 0;

    this->decode_state_ =
        DecodeState::WAIT_HEADER;

    return;
  }

  switch (this->decode_state_) {
    case DecodeState::WAIT_GAP:
      return;

    case DecodeState::WAIT_HEADER:
      if (event.level &&
          duration >= HEADER_MIN_US &&
          duration <= HEADER_MAX_US) {
        this->begin_frame_();
      } else {
        this->rejected_frame_count_++;
        this->reset_decoder_();
      }

      return;

    case DecodeState::WAIT_BIT_LOW:
      if (!event.level &&
          duration >= BIT_LOW_MIN_US &&
          duration <= BIT_LOW_MAX_US) {
        this->decode_state_ =
            DecodeState::WAIT_BIT_HIGH;
      } else {
        this->rejected_frame_count_++;
        this->reset_decoder_();
      }

      return;

    case DecodeState::WAIT_BIT_HIGH:
      if (!event.level) {
        this->rejected_frame_count_++;
        this->reset_decoder_();
        return;
      }

      if (duration >= BIT_ZERO_MIN_US &&
          duration <= BIT_ZERO_MAX_US) {
        this->append_bit_(false);
        return;
      }

      if (duration >= BIT_ONE_MIN_US &&
          duration <= BIT_ONE_MAX_US) {
        this->append_bit_(true);
        return;
      }

      this->rejected_frame_count_++;
      this->reset_decoder_();
      return;
  }
}

void Ecolo80Component::loop() {
  while (true) {
    EdgeEvent event;
    bool have_event = false;
    bool overflow = false;

    {
      InterruptLock lock;

      overflow =
          this->buffer_overflow_;

      this->buffer_overflow_ =
          false;

      if (this->read_index_ !=
          this->write_index_) {
        event.duration_us =
            this->buffer_[
                this->read_index_
            ].duration_us;

        event.level =
            this->buffer_[
                this->read_index_
            ].level;

        this->read_index_ =
            static_cast<uint16_t>(
                (this->read_index_ + 1U) %
                BUFFER_SIZE
            );

        have_event = true;
      }
    }

    if (overflow) {
      ESP_LOGW(
          TAG,
          "Edge buffer overflow; resetting decoder"
      );

      this->reset_decoder_();
    }

    if (!have_event) {
      break;
    }

    this->process_event_(event);
  }

  const uint32_t now_ms = millis();

  if (static_cast<uint32_t>(
          now_ms - this->last_consumption_publish_ms_
      ) >= CONSUMPTION_REFRESH_MS) {
    this->last_consumption_publish_ms_ = now_ms;

    this->publish_consumption_state_(
        this->have_heating_state_
            ? this->last_heating_state_
            : false
    );
  }

  this->service_desired_target_queue_();

  if (this->tx_echo_expected_ &&
      static_cast<int32_t>(
          now_ms - this->tx_echo_deadline_ms_
      ) >= 0) {
    this->tx_echo_expected_ = false;
    this->tx_echo_mismatch_count_++;

    ESP_LOGW(
        TAG,
        "TX ECHO NOT DECODED for copy %u within 1500 ms",
        this->tx_echo_expected_copy_
    );

    ESP_LOGW(
        TAG,
        "TX ECHO diagnostic totals: valid=%" PRIu32
        " missing_or_mismatch=%" PRIu32,
        this->tx_echo_valid_count_,
        this->tx_echo_mismatch_count_
    );
  }

  if (this->rmt_tx_done_) {
    this->finish_rmt_frame_();

      if (this->power_sequence_active_) {
        ESP_LOGI(
            TAG,
            "RMT POWER %s copy %u/%u complete",
            this->requested_power_state_ ? "ON" : "OFF",
            this->power_copy_index_,
            POWER_MAX_COPIES
        );
      } else if (this->target_sequence_active_) {
        ESP_LOGI(
            TAG,
            "RMT TARGET %.0f F copy %u/%u complete",
            this->requested_target_temperature_f_,
            this->target_copy_index_,
            TARGET_MAX_COPIES
        );
      }
  }

  if (this->power_sequence_active_) {
    if (static_cast<int32_t>(
            now_ms -
            this->power_ack_deadline_ms_
        ) >= 0) {
      this->power_sequence_active_ = false;

      ESP_LOGW(
          TAG,
          "RMT POWER %s NOT ACKNOWLEDGED within %"
          PRIu32 " ms",
          this->requested_power_state_
              ? "ON"
              : "OFF",
          POWER_ACK_TIMEOUT_MS
      );

      if (this->power_switch_ != nullptr &&
          this->have_power_state_) {
        this->power_switch_->publish_state(
            this->last_power_state_
        );
      }
    } else if (!this->rmt_transmitting_ &&
               this->power_copy_index_ <
                   POWER_MAX_COPIES &&
               static_cast<int32_t>(
                   now_ms -
                   this->next_power_copy_ms_
               ) >= 0 &&
               this->bus_is_idle_()) {
      const uint8_t copy_number =
          static_cast<uint8_t>(
              this->power_copy_index_ + 1U
          );

      this->current_tx_start_ms_ = now_ms;

      this->pending_tx_command_ =
          this->pending_power_command_;

      if (this->start_rmt_frame_(
              this->pending_power_command_)) {
        this->power_copy_index_ =
            copy_number;

        this->next_power_copy_ms_ =
            this->current_tx_start_ms_ +
            POWER_REPEAT_MS;

        ESP_LOGI(
            TAG,
            "RMT POWER %s copy %u/%u started",
            this->requested_power_state_
                ? "ON"
                : "OFF",
            copy_number,
            POWER_MAX_COPIES
        );
      }
    }
  }


  if (this->target_sequence_active_) {
    if (static_cast<int32_t>(
            now_ms -
            this->target_ack_deadline_ms_
        ) >= 0) {
      this->target_sequence_active_ = false;

      ESP_LOGW(
          TAG,
          "RMT TARGET %.0f F NOT ACKNOWLEDGED within %"
          PRIu32 " ms",
          this->requested_target_temperature_f_,
          TARGET_ACK_TIMEOUT_MS
      );

      this->desired_target_active_ = false;

    } else if (!this->rmt_transmitting_ &&
               this->target_copy_index_ <
                   TARGET_MAX_COPIES &&
               static_cast<int32_t>(
                   now_ms -
                   this->next_target_copy_ms_
               ) >= 0 &&
               this->bus_is_idle_()) {
      const uint8_t copy_number =
          static_cast<uint8_t>(
              this->target_copy_index_ + 1U
          );

      this->current_tx_start_ms_ = now_ms;
      this->pending_tx_command_ =
          this->pending_target_command_;

      if (this->start_rmt_frame_(
              this->pending_target_command_)) {
        this->target_copy_index_ =
            copy_number;

        this->next_target_copy_ms_ =
            this->current_tx_start_ms_ +
            TARGET_REPEAT_MS;

        ESP_LOGI(
            TAG,
            "RMT TARGET %.0f F copy %u/%u started",
            this->requested_target_temperature_f_,
            copy_number,
            TARGET_MAX_COPIES
        );
      }
    }
  }

  if (!this->command_examples_logged_ &&
      this->command_template_available_() &&
      (now_ms - this->startup_ms_) >=
          COMMAND_PREVIEW_DELAY_MS) {
    this->log_command_builder_examples_();
  }

  if ((now_ms -
       this->last_publish_ms_) <
      5000UL) {
    return;
  }

  this->last_publish_ms_ =
      now_ms;

  uint32_t edges;
  uint32_t total;
  uint32_t last_edge;

  {
    InterruptLock lock;

    edges =
        this->pending_edges_;

    total =
        this->total_edges_;

    last_edge =
        this->last_edge_us_;

    this->pending_edges_ = 0;
  }

  const uint32_t edge_age_ms =
      static_cast<uint32_t>(
          (micros() - last_edge) /
          1000UL
      );

  ESP_LOGD(
      TAG,
      "Edges=%" PRIu32
      " total=%" PRIu32
      " valid=%" PRIu32
      " rejected=%" PRIu32
      " checksum_errors=%" PRIu32,
      edges,
      total,
      this->valid_frame_count_,
      this->rejected_frame_count_,
      this->checksum_error_count_
  );
}

}  // namespace ecolo80
}  // namespace esphome
