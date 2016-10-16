CROSS_COMPILE ?= arm-none-eabi-
CC = $(CROSS_COMPILE)gcc
OBJCOPY = $(CROSS_COMPILE)objcopy

LIST_END=
	#-DBOARD_IDENT="\"Black_Magic Probe_generic_stm32f103c8\"" \


CFLAGS += -mcpu=cortex-m3 -mthumb \
	-DSTM32F1 -DDISCOVERY_STLINK -I../libopencm3/include \
	-DUSB_DP_PORT=GPIOA -DUSB_DP_PIN=GPIO12 \
	-I platforms/stm32


LDFLAGS = --specs=nano.specs \
	-lopencm3_stm32f1 -Wl,--defsym,_stack=0x20005000 \
	-Wl,-T,platforms/stm32/stlink.ld -nostartfiles -lc -lnosys \
	-Wl,-Map=mapfile -mthumb -mcpu=cortex-m3 -Wl,-gc-sections \
	-L../libopencm3/lib

VPATH += platforms/stm32

SRC += 	cdcacm.c	\
	usbuart.c	\
	serialno.c	\
	timing.c	\
$(LIST_END)

all:	blackmagic.bin

host_clean:
	-$(Q)$(RM) blackmagic.bin
