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
#ifndef __PLATFORM_H
#define __PLATFORM_H

#include "gpio.h"
#include "timing.h"
#include "version.h"

#include <libopencm3/cm3/common.h>
#include <libopencm3/stm32/f1/memorymap.h>
#include <libopencm3/usb/usbd.h>

#define BOARD_IDENT       "Black Magic Probe (STM32F103C8), (Firmware " FIRMWARE_VERSION ")"
#define BOARD_IDENT_DFU   "Black Magic (Upgrade) for STM32F103C8, (Firmware " FIRMWARE_VERSION ")"
#define BOARD_IDENT_UPD   "Black Magic (DFU Upgrade) for STM32F103C8, (Firmware " FIRMWARE_VERSION ")"
#define DFU_IDENT         "Black Magic Firmware Upgrade (STM32F103C8)"
#define DFU_IFACE_STRING  "@Internal Flash   /0x08000000/8*001Ka,56*001Kg"
#define UPD_IFACE_STRING  "@Internal Flash   /0x08000000/8*001Kg"

/* Important pin mappings for STM32 implementation:
 *
 * LED0 = PC13	(Red LED : State)
 *
 *  TMS = PA4 (SWDIO)
 *  TCK = PA5 (SWCLK)
 *  TDO = PA6 (input)
 *  TDI = PA7
 *
 *   RX = PA3 (Virtual COM Port) 
 *   TX = PA2 (Virtual COM Port)
 *
 */

/* Hardware definitions... */
#define TDI_PORT	GPIOA
#define TMS_PORT	GPIOA
#define TCK_PORT	GPIOA
#define TDO_PORT	GPIOA

#define TMS_PIN		GPIO4 /* SWDIO */
#define TCK_PIN		GPIO5 /* SWCLK */
#define TDO_PIN		GPIO6
#define TDI_PIN		GPIO7

#define SWDIO_PORT	TMS_PORT
#define SWCLK_PORT	TCK_PORT
#define SWDIO_PIN	TMS_PIN
#define SWCLK_PIN	TCK_PIN

#define SRST_PORT	GPIOB
#define SRST_PIN_V1	GPIO1
#define SRST_PIN_V2	GPIO0

#define LED_PORT	GPIOC
#define LED_IDLE_RUN	GPIO13

/* Use PC14 for a "dummy" uart led. So we can observere at least with scope*/
#define LED_PORT_UART	GPIOC
#define LED_UART	GPIO14

#define USBUSART_PORT	GPIOA
#define USBUSART_TX_PIN	GPIO2  /* TX2 */
#if 0 /* documentation only */
#define USBUSART_RX_PIN	GPIO3  /* RX2 */
#endif

#define TMS_SET_MODE() \
	gpio_set_mode(TMS_PORT, GPIO_MODE_OUTPUT_50_MHZ, \
	              GPIO_CNF_OUTPUT_PUSHPULL, TMS_PIN);
#define SWDIO_MODE_FLOAT() \
	gpio_set_mode(SWDIO_PORT, GPIO_MODE_INPUT, \
	              GPIO_CNF_INPUT_FLOAT, SWDIO_PIN);
#define SWDIO_MODE_DRIVE() \
	gpio_set_mode(SWDIO_PORT, GPIO_MODE_OUTPUT_50_MHZ, \
	              GPIO_CNF_OUTPUT_PUSHPULL, SWDIO_PIN);

#define UART_PIN_SETUP() \
	gpio_set_mode(USBUSART_PORT, GPIO_MODE_OUTPUT_2_MHZ, \
	              GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, USBUSART_TX_PIN);

#define SRST_SET_VAL(x) \
	platform_srst_set_val(x)

#define USB_DRIVER      stm32f103_usb_driver
#define USB_IRQ	        NVIC_USB_LP_CAN_RX0_IRQ
#define USB_ISR	        usb_lp_can_rx0_isr
/* Interrupt priorities.  Low numbers are high priority.
 * For now USART2 preempts USB which may spin while buffer is drained.
 * TIM3 is used for traceswo capture and must be highest priority.
 */
#define IRQ_PRI_USB		(2 << 4)
#define IRQ_PRI_USBUSART	(1 << 4)
#define IRQ_PRI_USBUSART_TIM	(3 << 4)
#define IRQ_PRI_USB_VBUS	(14 << 4)
#define IRQ_PRI_TIM3		(0 << 4)

#define USBUSART USART2
#define USBUSART_CR1 USART2_CR1
#define USBUSART_IRQ NVIC_USART2_IRQ
#define USBUSART_CLK RCC_USART2
#define USBUSART_PORT GPIOA
#if 0
#define USBUSART_TX_PIN GPIO2
#endif
#define USBUSART_ISR usart2_isr
#define USBUSART_TIM TIM4
#define USBUSART_TIM_CLK_EN() rcc_periph_clock_enable(RCC_TIM4)
#define USBUSART_TIM_IRQ NVIC_TIM4_IRQ
#define USBUSART_TIM_ISR tim4_isr

#define DEBUG(...)

#define SET_RUN_STATE(state)	{running_status = (state);}
#define SET_IDLE_STATE(state)	{gpio_set_val(LED_PORT, LED_IDLE_RUN, state);}
#define SET_ERROR_STATE(x)

/* Use newlib provided integer only stdio functions */
#define sscanf siscanf
#define sprintf siprintf
#define vasprintf vasiprintf

#endif

