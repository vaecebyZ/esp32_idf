
#include "driver/gpio.h"
#define LED_GPIO GPIO_NUM_27
#define ON 1
#define OFF 0

// 初始化GPIO
void init_gpio()
{
  gpio_config_t led_cfg = {
      .pin_bit_mask = (1 << LED_GPIO),
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .mode = GPIO_MODE_OUTPUT,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&led_cfg);
}

void set_led_on()
{
  gpio_set_level(LED_GPIO, ON);
}

void set_led_off()
{
  gpio_set_level(LED_GPIO, OFF);
}