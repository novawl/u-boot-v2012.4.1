/*
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * (C) Copyright 2002, 2010
 * David Mueller, ELSOFT AG, <d.mueller@elsoft.ch>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/arch/s3c2440.h>


#define BUSY	1
#define TACLS   0
#define TWRPH0  3
#define TWRPH1  0
#define NAND_SECTOR_SIZE    512
#define NAND_BLOCK_MASK     (NAND_SECTOR_SIZE - 1)

#define delay(x)		do { int i; \
						  for (i = 0; i < (x); i++); } while(0)


static void wait_idle(void)
{
	struct s3c2440_nand *nand = (struct s3c2440_nand *) 0x4E000000;
	volatile unsigned char *p = (volatile unsigned char *) &nand->nfstat;

	while(!(*p & BUSY))
		delay(10);
}

static void nand_select_chip(void)
{
	struct s3c2440_nand *nand = (struct s3c2440_nand *) 0x4E000000;

	nand->nfcont &= ~(1<<1);
	delay(10);
}

static void nand_deselect_chip(void)
{
	struct s3c2440_nand *nand = (struct s3c2440_nand *) 0x4E000000;

	nand->nfcont |= (1<<1);
}

static void write_cmd(int cmd)
{
	struct s3c2440_nand *nand = (struct s3c2440_nand *) 0x4E000000;

	volatile unsigned char *p = (volatile unsigned char *) &nand->nfcmd;
	*p = cmd;
}

static void write_addr(unsigned int addr)
{
	struct s3c2440_nand *nand = (struct s3c2440_nand *) 0x4E000000;
	volatile unsigned char *p = (volatile unsigned char *) &nand->nfaddr;

	*p = addr & 0xff;
	delay(10);
	*p = (addr >> 9) & 0xff;
	delay(10);
	*p = (addr >> 17) & 0xff;
	delay(10);
	*p = (addr >> 25) & 0xff;
	delay(10);
}

static unsigned char read_data(void)
{
	struct s3c2440_nand *nand = (struct s3c2440_nand *) 0x4E000000;
	volatile unsigned char *p = (volatile unsigned char *) &nand->nfdata;

	return *p;
}

static void micro2440_nand_init(void)
{
	struct s3c2440_nand *nand = (struct s3c2440_nand *) 0x4E000000;

	nand->nfconf = (TACLS<<12)|(TWRPH0<<8)|(TWRPH1<<4);
	nand->nfcont = (1<<4)|(1<<1)|(1<<0);

	nand_select_chip();
	write_cmd(0xff);
	wait_idle();
	nand_deselect_chip();
}

static void micro2440_nand_read(unsigned char *buf, unsigned long start_addr, int size)
{
	int i, j;

	if (start_addr & NAND_BLOCK_MASK)
		return;

	if (size & NAND_BLOCK_MASK)
		size = (size + NAND_BLOCK_MASK)&~(NAND_BLOCK_MASK);

	nand_select_chip();
	for(i=start_addr; i < (start_addr + size);) {
		write_cmd(0);
		write_addr(i);
		wait_idle();

		for(j=0; j < NAND_SECTOR_SIZE; j++, i++)
			*buf++ = read_data();
	}

	nand_deselect_chip();
}

static int bBootFrmNORFlash(void)
{
	volatile unsigned int *pdw = (volatile unsigned int *)0;
	unsigned int dwVal;

	dwVal = *pdw;
	*pdw = 0x12345678;
	if (*pdw != 0x12345678) {
		return 1;
	} else {
		*pdw = dwVal;
		return 0;
	}
}

int CopyCode2Ram(unsigned long start_addr, unsigned char *buf, int size)
{
	unsigned int *pdwDest;
	unsigned int *pdwSrc;
	int i;

	if (bBootFrmNORFlash()) {
		pdwDest = (unsigned int *)buf;
		pdwSrc  = (unsigned int *)start_addr;
		for (i = 0; i < size / 4; i++)
			pdwDest[i] = pdwSrc[i];
	} else {
		micro2440_nand_init();
		micro2440_nand_read(buf, start_addr, size);
	}

	return 0;
}
