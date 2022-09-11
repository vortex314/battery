#include <stdlib.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/cdc.h>
#include <libopencm3/cm3/systick.h>
#include <string.h>
#include <limero.h>
#include <Usb.h>
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
Log logger;
RedisSpineCbor redis(mainThread);

extern "C" void uartSendBytes(uint8_t *buf, size_t len, uint32_t)
{
	usb.txdLine.on(Bytes(buf, buf+len));
}

void *__dso_handle = 0;

void usbWriter(char *buffer, uint32_t bufLength)
{
	usb.txdLine.on(Bytes(buffer,buffer+bufLength));
}

void __putchar(char c)
{
	usbWriter(&c, 1);
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
	gpio_set_mode(LED_PORT, GPIO_MODE_OUTPUT_2_MHZ,
				  GPIO_CNF_OUTPUT_PUSHPULL, LED_PIN);
	ticker >> [](const TimerMsg &)
	{
		gpio_toggle(LED_PORT, LED_PIN);
	};
	redis.setNode("battery");
	usb.rxdLine >> [&](const Bytes& frag){ extractFrame(frag,redis.rxdFrame); };
	redis.txdFrame >> usb.txdLine;
	usb.connected >>  [&](const bool& b){ticker.interval(b?500:100);};

	mainThread.run();
}

void extractFrame(const Bytes &bs, Sink<Bytes> &flow)
{
	static ProtocolDecoder decoder(128);

	for (auto b : bs)
	{
		if (b == PPP_FLAG_CHAR)
		{
			if (decoder.size() > 2 && decoder.ok() && decoder.checkCrc())
			{
				flow.on(decoder);
			}
			decoder.reset();
		}
		else
		{
			decoder.addUnEscaped(b);
		}
	}
}
