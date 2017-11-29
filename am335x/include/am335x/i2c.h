/*
 *
 * Copyright (c) 2017 Mytchel Hammond <mytch@lackname.org>
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _AM335X_I2C_H_
#define _AM335X_I2C_H_

#define I2C0  0x44e0b000
#define I2C1  0x4802a000
#define I2C2  0x4819c000

#define I2C0_intr 70
#define I2C1_intr 71
#define I2C2_intr 30

typedef struct i2c_regs *i2c_regs_t;

struct i2c_regs {
	uint32_t revnb_lo;
	uint32_t revnb_hi;
	uint32_t pad0[2];
	uint32_t sysc;
	uint32_t pad1[4];
	uint32_t irqstatus_raw;
	uint32_t irqstatus;
	uint32_t irqenable_set;
	uint32_t irqenable_clr;
	uint32_t we;
	uint32_t dmarxenable_set;
	uint32_t dmatxenable_set;
	uint32_t dmarxenable_clr;
	uint32_t dmatxenable_clr;
	uint32_t dmarxwake_en;
	uint32_t dmatxwake_en;
	uint32_t pad2[16];
	uint32_t syss;
	uint32_t cnt;
	uint32_t data;
	uint32_t pad3[1];
	uint32_t con;
	uint32_t oa;
	uint32_t sa;
	uint32_t psc;
	uint32_t scll;
	uint32_t systest;
	uint32_t bufstat;
	uint32_t oa1;
	uint32_t oa2;
	uint32_t oa3;
	uint32_t actoa;
	uint32_t sblock;
};

struct 