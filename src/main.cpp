#include <stdlib.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/cdc.h>
#include <libopencm3/cm3/systick.h>
#include <string.h>
#include <limero.h>
#include <Usb.h>
#include <Adc.h>
#include <Log.h>
#include <RedisSpineCbor.h>

#ifdef BOARD_maple_mini
#define LED_PORT GPIOB
#define LED_PIN GPIO1
#endif

#ifdef BOARD_blue_pill
#define LED_PORT GPIOC
#define LED_PIN GPIO13
#endif
void extractFrame(const Bytes &fragment, Sink<Bytes> &flow);

Thread mainThread("main");
TimerSource ticker(mainThread, 100, true, "ticker");
TimerSource clocker(mainThread, 10000, true, "clocker");

Usb usb(mainThread);
Adc adc;
Log logger;
RedisSpineCbor redis(mainThread, "battery");

extern "C" void uartSendBytes(uint8_t *buf, size_t len, uint32_t)
{
	usb.txdLine.on(Bytes(buf, buf + len));
}

void *__dso_handle = 0;

void usbWriter(char *buffer, uint32_t bufLength)
{
	usb.txdLine.on(Bytes(buffer, buffer + bufLength));
}

extern "C" int _write(int, char *ptr, int len)
{
	usbWriter(ptr, len);
	return len;
}

int main(void)
{
	rcc_clock_setup_in_hse_8mhz_out_72mhz();
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOC);
	/* 72MHz / 8 => 9000000 counts per second */
	systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);
	/* 9000000/9000 = 1000 overflows per second - every 1ms one interrupt */
	/* SysTick interrupt every N clock pulses: set reload to N-1 */
	systick_set_reload(8999);
	systick_interrupt_enable();
	systick_counter_enable();

	usb.init();
	adc.init();
	gpio_set_mode(LED_PORT, GPIO_MODE_OUTPUT_2_MHZ,
				  GPIO_CNF_OUTPUT_PUSHPULL, LED_PIN);
	redis.connected >> [&](const bool &b)
	{ ticker.interval(b ? 500 : 100); };
	auto &pubVoltage = redis.publisher<float>("battery/voltage");
	auto &pubTemp = redis.publisher<float>("battery/temperature");
	auto &pubVref = redis.publisher<float>("battery/refVoltage");

	ticker >> [&](const TimerMsg &)
	{
		static int i = 0;
		i++;
		gpio_toggle(LED_PORT, LED_PIN);
		switch (i % 3)
		{
		case 0:
			pubVoltage.on(adc.read(0)*(3.3/4096));
			break;
		case 1:
			pubTemp.on(adc.read(ADC_CHANNEL_TEMP));
			break;
		case 2:
			pubVref.on(adc.read(ADC_CHANNEL_VREF)*(3.3/4096));
			break;
		}
	};
	usb.rxdLine >> redis.rxdFrame;
	redis.txdFrame >> usb.txdLine;

	mainThread.run();
}
