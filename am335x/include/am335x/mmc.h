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

#ifndef _AM335X_MMC_H_
#define _AM335X_MMC_H_

#define MMC0  0x48060000
#define MMC1  0x481D8000
#define MMC2  0x47810000

#define MMC0_intr 64
#define MMC1_intr 28
#define MMC2_intr 29

struct mmchs_regs {
  uint32_t pad[68];
  uint32_t sysconfig;
  uint32_t sysstatus;
  uint32_t pad1[3];
  uint32_t csre;
  uint32_t systest;
  uint32_t con;
  uint32_t pwcnt;
  uint32_t pad2[51];
  uint32_t sdmasa;
  uint32_t blk;
  uint32_t arg;
  uint32_t cmd;
  uint32_t rsp10;
  uint32_t rsp32;
  uint32_t rsp54;
  uint32_t rsp76;
  uint32_t data;
  uint32_t pstate;
  uint32_t hctl;
  uint32_t sysctl;
  uint32_t stat;
  uint32_t ie;
  uint32_t ise;
  uint32_t ac12;
  uint32_t capa;
  uint32_t pad3[1];
  uint32_t curcapa;
  uint32_t pad4[1];
  uint32_t fe;
  uint32_t admaes;
  uint32_t admasal;
  uint32_t admasah;
  uint32_t pad5[39];
  uint32_t rev;
}; 

#endif
