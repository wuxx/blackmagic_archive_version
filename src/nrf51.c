/*
 * This file is part of the Black Magic Debug project.
 *
 * Copyright (C) 2014  Mike Walters <mike@flomp.net>
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

/* This file implements nRF51 target specific functions for detecting
 * the device, providing the XML memory map and Flash memory programming.
 */

#include "general.h"
#include "adiv5.h"
#include "target.h"
#include "command.h"
#include "gdb_packet.h"
#include "cortexm.h"

static int nrf51_flash_erase(struct target_flash *f, uint32_t addr, size_t len);
static int nrf51_flash_write(struct target_flash *f,
                             uint32_t dest, const void *src, size_t len);

static bool nrf51_cmd_erase_all(target *t);
static bool nrf51_cmd_read_hwid(target *t);
static bool nrf51_cmd_read_fwid(target *t);
static bool nrf51_cmd_read_deviceid(target *t);
static bool nrf51_cmd_read_deviceaddr(target *t);
static bool nrf51_cmd_read_help();
static bool nrf51_cmd_read(target *t, int argc, const char *argv[]);

const struct command_s nrf51_cmd_list[] = {
	{"erase_mass", (cmd_handler)nrf51_cmd_erase_all, "Erase entire flash memory"},
	{"read", (cmd_handler)nrf51_cmd_read, "Read device parameters"},
	{NULL, NULL, NULL}
};
const struct command_s nrf51_read_cmd_list[] = {
	{"help", (cmd_handler)nrf51_cmd_read_help, "Display help for read commands"},
	{"hwid", (cmd_handler)nrf51_cmd_read_hwid, "Read hardware identification number"},
	{"fwid", (cmd_handler)nrf51_cmd_read_fwid, "Read pre-loaded firmware ID"},
	{"deviceid", (cmd_handler)nrf51_cmd_read_deviceid, "Read unique device ID"},
	{"deviceaddr", (cmd_handler)nrf51_cmd_read_deviceaddr, "Read device address"},
	{NULL, NULL, NULL}
};

/* Non-Volatile Memory Controller (NVMC) Registers */
#define NRF51_NVMC					0x4001E000
#define NRF51_NVMC_READY			(NRF51_NVMC + 0x400)
#define NRF51_NVMC_CONFIG			(NRF51_NVMC + 0x504)
#define NRF51_NVMC_ERASEPAGE		(NRF51_NVMC + 0x508)
#define NRF51_NVMC_ERASEALL			(NRF51_NVMC + 0x50C)
#define NRF51_NVMC_ERASEUICR		(NRF51_NVMC + 0x514)

#define NRF51_NVMC_CONFIG_REN		0x0						// Read only access
#define NRF51_NVMC_CONFIG_WEN		0x1						// Write enable
#define NRF51_NVMC_CONFIG_EEN		0x2						// Erase enable

/* Factory Information Configuration Registers (FICR) */
#define NRF51_FICR				0x10000000
#define NRF51_FICR_CODEPAGESIZE			(NRF51_FICR + 0x010)
#define NRF51_FICR_CODESIZE				(NRF51_FICR + 0x014)
#define NRF51_FICR_CONFIGID				(NRF51_FICR + 0x05C)
#define NRF51_FICR_DEVICEID_LOW			(NRF51_FICR + 0x060)
#define NRF51_FICR_DEVICEID_HIGH		(NRF51_FICR + 0x064)
#define NRF51_FICR_DEVICEADDRTYPE		(NRF51_FICR + 0x0A0)
#define NRF51_FICR_DEVICEADDR_LOW		(NRF51_FICR + 0x0A4)
#define NRF51_FICR_DEVICEADDR_HIGH		(NRF51_FICR + 0x0A8)

/* User Information Configuration Registers (UICR) */
#define NRF51_UICR				0x10001000

#define NRF51_PAGE_SIZE 1024

#define SRAM_BASE          0x20000000
#define STUB_BUFFER_BASE   (SRAM_BASE + 0x28)

static const uint16_t nrf51_flash_write_stub[] = {
#include "../flashstub/nrf51.stub"
};

static void nrf51_add_flash(target *t,
                            uint32_t addr, size_t length, size_t erasesize)
{
	struct target_flash *f = calloc(1, sizeof(*f));
	f->start = addr;
	f->length = length;
	f->blocksize = erasesize;
	f->erase = nrf51_flash_erase;
	f->write = nrf51_flash_write;
	f->align = 4;
	f->erased = 0xff;
	target_add_flash(t, f);
}

char driverNameBuf[32];
bool nrf51_probe(target *t)
{
int ramsize=0x4000;//default to 16k
int flashsize=0x40000;// default to 256k as this is the most common
	
	t->idcode = target_mem_read32(t, NRF51_FICR_CONFIGID) & 0xFFFF;
	sprintf(driverNameBuf,"Nordic nRF51 0x%04x ",(unsigned int)t->idcode);
	
// Codes updated by Roger Clark. 20151212 from data in http://www.nordicsemi.com/eng/nordic/content_download/72555/1235675/file/nRF51_Series_Compatibility_Matrix_v2.1.pdf
// Note. For Build code values with a small x in them e.g. Ex0 
// x can be any letter from A to Z, or 0 to 9
	
	switch (t->idcode) {
	
	// IC Revision 1
	case 0x001D: //QF AA CA and QF AA C0
	case 0x001E: //QF AA CA
	case 0x0020: //CE AA BA		
	case 0x0024: //QF AA C0
	case 0x002F: //CE AA B0			
	case 0x0031: //CE AA A0A
		// default settings of 256k Flash 16k RAM	
		break;
	case 0x0026: //QF AB AA
	case 0x0027: //QF AB A0	
		flashsize = 0x20000;// 128k Flash (16k RAM)
		break;
	
	// IC Revision 2
	case 0x002A: //QF AA FA0 	
	case 0x002D: //QF AA DAA
	case 0x002E: //QF AA Ex0 	
	case 0x003C: //QF AA Gx0 	
	case 0x0040:// CE AA CA0 
	case 0x0044: //QF AA GC0	
	case 0x0047:// CE AA DA0 
	case 0x004D:// CE AA Dx0 	
	case 0x0050:// CE AA Bx0 	
		// default settings of 256k Flash 16k RAM
		break;
	case 0x0061: //QF AB A00
	case 0x004C: //QF AB Bx0	
		flashsize = 0x20000;// 128k Flash (16k RAM)
		break;

		
	// IC Revision 3
	case 0x0072:// QF AA Hx0
	case 0x0073:// QF AA Fx0
	case 0x0079:// CE AA Ex0	
	case 0x007A:// CE AA Cx0	
		// default settings of 256k Flash 16k RAM
		break;
	case 0x007B:// QF AB Cx0
	case 0x007C:// QF AB Bx0
	case 0x007D:// CD AB Ax0
	case 0x007E:// CD AB Ax0	
	case 0x0058:// CD AA Ax0		
		flashsize = 0x20000;// 128k Flash (16k RAM)
		break;
	case 0x0083:// QF AC Ax0
	
	case 0x0084:// not listed in the spec but seems to be QF AC A1
	
	case 0x0085:// QF AC Ax0
	
	case 0x0086:// not listed (don't know what markings, but suspect QF AC Ax0
	
	
	
	case 0x0087:// QF AC Ax0	
	case 0x0088:// CF AC Ax0	
		ramsize=0x8000;// 32k RAM (256k Flash)
		break;
	case 0x008F:// QFAAH0 ??
		break;
	default:
		return false;// not a recognised code
		break;// redundant break.
	}

	t->driver = driverNameBuf;//"Nordic nRF51..";
	target_add_ram(t, 0x20000000, ramsize);
	nrf51_add_flash(t, 0x00000000, flashsize, NRF51_PAGE_SIZE);
	nrf51_add_flash(t, NRF51_UICR, 0x100, 0x100);
	nrf51_add_flash(t, NRF51_FICR, 0x100, 0x100);
	
	target_add_commands(t, nrf51_cmd_list, "nRF51");	
	
	return true;
}

static int nrf51_flash_erase(struct target_flash *f, uint32_t addr, size_t len)
{
	target *t = f->t;
	/* Enable erase */
	target_mem_write32(t, NRF51_NVMC_CONFIG, NRF51_NVMC_CONFIG_EEN);

	/* Poll for NVMC_READY */
	while (target_mem_read32(t, NRF51_NVMC_READY) == 0)
		if(target_check_error(t))
			return -1;

	while (len) {
		if (addr == NRF51_UICR) { // Special Case
			/* Write to the ERASE_UICR register to erase */
			target_mem_write32(t, NRF51_NVMC_ERASEUICR, 0x1);

		} else { // Standard Flash Page
			/* Write address of first word in page to erase it */
			target_mem_write32(t, NRF51_NVMC_ERASEPAGE, addr);
		}

		/* Poll for NVMC_READY */
		while (target_mem_read32(t, NRF51_NVMC_READY) == 0)
			if(target_check_error(t))
				return -1;

		addr += f->blocksize;
		len -= f->blocksize;
	}

	/* Return to read-only */
	target_mem_write32(t, NRF51_NVMC_CONFIG, NRF51_NVMC_CONFIG_REN);

	/* Poll for NVMC_READY */
	while (target_mem_read32(t, NRF51_NVMC_READY) == 0)
		if(target_check_error(t))
			return -1;

	return 0;
}

static int nrf51_flash_write(struct target_flash *f,
                             uint32_t dest, const void *src, size_t len)
{
	target *t = f->t;
	uint32_t data[2 + len/4];

	/* FIXME rewrite stub to use register args */

	/* Construct data buffer used by stub */
	data[0] = dest;
	data[1] = len;		/* length must always be a multiple of 4 */
	memcpy((uint8_t *)&data[2], src, len);

	/* Enable write */
	target_mem_write32(t, NRF51_NVMC_CONFIG, NRF51_NVMC_CONFIG_WEN);

	/* Poll for NVMC_READY */
	while (target_mem_read32(t, NRF51_NVMC_READY) == 0)
		if(target_check_error(t))
			return -1;

	/* Write stub and data to target ram and call stub */
	target_mem_write(t, SRAM_BASE, nrf51_flash_write_stub,
	                 sizeof(nrf51_flash_write_stub));
	target_mem_write(t, STUB_BUFFER_BASE, data, len + 8);
	cortexm_run_stub(t, SRAM_BASE, 0, 0, 0, 0);

	/* Return to read-only */
	target_mem_write32(t, NRF51_NVMC_CONFIG, NRF51_NVMC_CONFIG_REN);

	return 0;
}

static bool nrf51_cmd_erase_all(target *t)
{
	gdb_out("erase..\n");

	/* Enable erase */
	target_mem_write32(t, NRF51_NVMC_CONFIG, NRF51_NVMC_CONFIG_EEN);

	/* Poll for NVMC_READY */
	while (target_mem_read32(t, NRF51_NVMC_READY) == 0)
		if(target_check_error(t))
			return false;

	/* Erase all */
	target_mem_write32(t, NRF51_NVMC_ERASEALL, 1);

	/* Poll for NVMC_READY */
	while (target_mem_read32(t, NRF51_NVMC_READY) == 0)
		if(target_check_error(t))
			return false;

	return true;
}

static bool nrf51_cmd_read_hwid(target *t)
{
	uint32_t hwid = target_mem_read32(t, NRF51_FICR_CONFIGID) & 0xFFFF;
	gdb_outf("Hardware ID: 0x%04X\n", hwid);

	return true;
}
static bool nrf51_cmd_read_fwid(target *t)
{
	uint32_t fwid = (target_mem_read32(t, NRF51_FICR_CONFIGID) >> 16) & 0xFFFF;
	gdb_outf("Firmware ID: 0x%04X\n", fwid);

	return true;
}
static bool nrf51_cmd_read_deviceid(target *t)
{
	uint32_t deviceid_low = target_mem_read32(t, NRF51_FICR_DEVICEID_LOW);
	uint32_t deviceid_high = target_mem_read32(t, NRF51_FICR_DEVICEID_HIGH);

	gdb_outf("Device ID: 0x%08X%08X\n", deviceid_high, deviceid_low);

	return true;
}
static bool nrf51_cmd_read_deviceaddr(target *t)
{
	uint32_t addr_type = target_mem_read32(t, NRF51_FICR_DEVICEADDRTYPE);
	uint32_t addr_low = target_mem_read32(t, NRF51_FICR_DEVICEADDR_LOW);
	uint32_t addr_high = target_mem_read32(t, NRF51_FICR_DEVICEADDR_HIGH) & 0xFFFF;

	if ((addr_type & 1) == 0) {
		gdb_outf("Publicly Listed Address: 0x%04X%08X\n", addr_high, addr_low);
	} else {
		gdb_outf("Randomly Assigned Address: 0x%04X%08X\n", addr_high, addr_low);
	}

	return true;
}
static bool nrf51_cmd_read_help()
{
	const struct command_s *c;

	gdb_out("Read commands:\n");
	for(c = nrf51_read_cmd_list; c->cmd; c++)
		gdb_outf("\t%s -- %s\n", c->cmd, c->help);

	return true;
}
static bool nrf51_cmd_read(target *t, int argc, const char *argv[])
{
	const struct command_s *c;

	for(c = nrf51_read_cmd_list; c->cmd; c++) {
		/* Accept a partial match as GDB does.
		 * So 'mon ver' will match 'monitor version'
		 */
		if(!strncmp(argv[1], c->cmd, strlen(argv[1])))
			return !c->handler(t, argc - 1, &argv[1]);
	}

	return nrf51_cmd_read_help(t);
}

