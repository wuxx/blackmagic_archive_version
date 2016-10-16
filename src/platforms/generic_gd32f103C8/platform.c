/*
 * This file is part of the Black Magic Debug project.
 *
 * Copyright (C) 2011  Black Sphere Technologies Ltd.
 * Written by Gareth McMullin <gareth@blacksphere.co.nz>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* This file implements the platform specific functions for the STM32
 * implementation.
 */

#include "platform.h"
#include "general.h"
#include "cdcacm.h"
#include "usbuart.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/stm32/adc.h>

void platform_init(void)
{
//	rcc_clock_setup_in_hse_8mhz_out_72mhz();

	rcc_clock_setup_in_hse_12mhz_out_72mhz();	
	/* Enable peripherals */
	rcc_periph_clock_enable(RCC_USB);
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOC);
	rcc_periph_clock_enable(RCC_AFIO);
	rcc_periph_clock_enable(RCC_CRC);

	/* Setup GPIO ports */
	gpio_set_mode(SWDIO_PORT, GPIO_MODE_OUTPUT_10_MHZ,
	              GPIO_CNF_OUTPUT_PUSHPULL, SWDIO_PIN);
	gpio_set(SWDIO_PORT,SWDIO_PIN);// Roger Clark. Seems to halt on powerup unless this is Set instead of Cleared

	gpio_set_mode(SWCLK_PORT, GPIO_MODE_OUTPUT_10_MHZ,
	              GPIO_CNF_OUTPUT_PUSHPULL, SWCLK_PIN);
	gpio_set(SWCLK_PORT,SWCLK_PIN);// Roger Clark. Seems to halt on powerup unless this is Set instead of Cleared

	gpio_set_mode(LED_PORT, GPIO_MODE_OUTPUT_10_MHZ,
	              GPIO_CNF_OUTPUT_PUSHPULL, LED_IDLE_RUN);

	platform_timing_init();


/* toggle USB_DP (PA12) to reset USB port assumes a 1.5k pull up to 3.3v */
	gpio_set_mode(USB_DP_PORT, GPIO_MODE_OUTPUT_2_MHZ,
				  GPIO_CNF_OUTPUT_OPENDRAIN, USB_DP_PIN);
	gpio_clear(USB_DP_PORT,USB_DP_PIN);
	volatile unsigned x = 48000000/4/100; do { ; } while(--x);
	
	cdcacm_init();

	usbuart_init();
}

const char *platform_target_voltage(void)
{
	return "Not Implemented!";
}

void platform_request_boot(void)
{
}

void platform_srst_set_val(bool assert)
{
	(void)assert;
}

int platform_hwversion() {
	return 1;
}
