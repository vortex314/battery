#include <Adc.h>

uint8_t channel_array[] = {1, 1, ADC_CHANNEL_TEMP};

void Adc::init()
{
	rcc_periph_clock_enable(RCC_ADC1);
	rcc_periph_clock_enable(RCC_GPIOA);

	gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_ANALOG, GPIO0);
	gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_ANALOG, GPIO1);

	/* Make sure the ADC doesn't run during config. */
	adc_power_off(ADC1);

	/* We configure everything for one single conversion. */
	adc_disable_scan_mode(ADC1);
	adc_set_single_conversion_mode(ADC1);
	adc_disable_external_trigger_regular(ADC1);
	adc_set_right_aligned(ADC1);
	adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_28DOT5CYC);
	adc_enable_temperature_sensor();
	
	adc_power_on(ADC1);

	/* Wait for ADC starting up. */
	int i;
	for (i = 0; i < 800000; i++) /* Wait a bit. */
		__asm__("nop");

	adc_reset_calibration(ADC1);
	adc_calibrate(ADC1);
}

uint16_t Adc::read(uint8_t channel)
{
	uint8_t channel_array[16];
	channel_array[0] = channel;
	adc_set_regular_sequence(ADC1, 1, channel_array);
	adc_start_conversion_direct(ADC1);
	while (!adc_eoc(ADC1))
		;
	return adc_read_regular(ADC1);
}
