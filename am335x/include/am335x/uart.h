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

#ifndef _AM335X_UART_H_
#define _AM335X_UART_H_

#define UART0         	0x44E09000
#define UART0_intr      72

typedef struct uart_regs *uart_regs_t;

struct uart_regs {
  uint32_t hr;
  uint32_t ier;
  uint32_t iir;
  uint32_t lcr;
  uint32_t mcr;
  uint32_t lsr;
  uint32_t tcr;
  uint32_t spr;
  uint32_t mdr1;
  uint32_t mdr2;
  uint32_t txfll;
  uint32_t txflh;
  uint32_t rxfll;
  uint32_t rxflh;
  uint32_t blr;
  uint32_t uasr;
  uint32_t scr;
  uint32_t ssr;
  uint32_t eblr;
  uint32_t mvr;
  uint32_t sysc;
  uint32_t syss;
  uint32_t wer;
  uint32_t cfps;
  uint32_t rxfifo_lvl;
  uint32_t txfifo_lvl;
  uint32_t ier2;
  uint32_t isr2;
  uint32_t freq_sel;
  uint32_t mdr3;
  uint32_t tx_dma_threshold;
};

#endif
