#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/gpio.h>
#include <stdint.h>

class Adc {
    public :
    void init();
    uint16_t read(uint8_t channel);
};