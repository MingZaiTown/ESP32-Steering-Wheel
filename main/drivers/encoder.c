#include "encoder.h"
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "esp_check.h"
#include <stdint.h>

pcnt_unit_handle_t pcnt_unit = NULL;

void enc_init(void) {
  /*********⬇️ 脉冲编码器相关初始化 ⬇️**********/
  pcnt_unit_config_t unit_config = {
      .high_limit = 32767,
      .low_limit = -32768,
  };
  ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));
  pcnt_glitch_filter_config_t filter_config = {
      .max_glitch_ns = 1000,
  };
  ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));
  pcnt_chan_config_t chan_a_config = {
      .edge_gpio_num = ENC_GPIO_PHASE_A,
      .level_gpio_num = ENC_GPIO_PHASE_B,
  };
  pcnt_channel_handle_t pcnt_chan_a = NULL;
  ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a));
  pcnt_chan_config_t chan_b_config = {
      .edge_gpio_num = ENC_GPIO_PHASE_B,
      .level_gpio_num = ENC_GPIO_PHASE_A,
  };
  pcnt_channel_handle_t pcnt_chan_b = NULL;
  ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_b_config, &pcnt_chan_b));

  ESP_ERROR_CHECK(pcnt_channel_set_edge_action(
      pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE,
      PCNT_CHANNEL_EDGE_ACTION_INCREASE));
  ESP_ERROR_CHECK(
      pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                    PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
  ESP_ERROR_CHECK(pcnt_channel_set_edge_action(
      pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
      PCNT_CHANNEL_EDGE_ACTION_DECREASE));
  ESP_ERROR_CHECK(
      pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                    PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

  ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
  ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
  ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));
  /*********⬆️ 脉冲编码器相关初始化 ⬆️**********/
}

int enc_get_count(void) {
  int count;
  pcnt_unit_get_count(pcnt_unit, &count);
  count = count > ENC_CNT_LIMIT ? ENC_CNT_LIMIT : 
        count < -ENC_CNT_LIMIT ? -ENC_CNT_LIMIT : count;
  return count;
}

int enc_get_count_test(void) {
  static int count = 0;
  static bool increase = true;
  if (increase)
    count++;
  else
    count--;
  if (increase && count >= ENC_CNT_LIMIT) {
    increase = false;
  } else if (!increase && count <= -ENC_CNT_LIMIT) {
    increase = true;
  }
  return count;
}

float enc_get_angle(void) {
  float angle = 0;
  int count = 0;
  pcnt_unit_get_count(pcnt_unit, &count);
  angle = (float)count / ENC_CNT_PER_CIRCLE;
  return angle;
}
