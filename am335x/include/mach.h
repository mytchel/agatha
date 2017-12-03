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

#define PAGE_SHIFT 	 12
#define PAGE_SIZE	 (1UL << PAGE_SHIFT)
#define PAGE_MASK	 (~(PAGE_SIZE - 1))

#define PAGE_ALIGN(x)    ((((reg_t) x) + PAGE_SIZE - 1) & PAGE_MASK)
#define PAGE_ALIGN_DN(x) ((((reg_t) x)) & PAGE_MASK)

#define SECTION_SHIFT  20
#define SECTION_SIZE (1UL << SECTION_SHIFT)
#define SECTION_MASK (~(SECTION_SIZE - 1))

#define SECTION_ALIGN(x)    ((((reg_t) x) + SECTION_SIZE - 1) & SECTION_MASK)
#define SECTION_ALIGN_DN(x) ((((reg_t) x)) & SECTION_MASK)

#define F_TYPE_IO    1
#define F_TYPE_MEM   2

/* Kind of mapping. */
#define F_MAP_TYPE_PAGE          (0<<F_MAP_TYPE_SHIFT)
#define F_MAP_TYPE_SECTION       (1<<F_MAP_TYPE_SHIFT)
#define F_MAP_TYPE_TABLE_SHIFT   (F_MAP_TYPE_SHIFT+1)
#define F_MAP_TYPE_TABLE_L1      (0<<F_MAP_TYPE_TABLE_SHIFT)
#define F_MAP_TYPE_TABLE_L2      (1<<F_MAP_TYPE_TABLE_SHIFT)

typedef struct label label_t;

struct label {
  uint32_t psr, sp, lr;
  uint32_t regs[13];
  uint32_t pc;
} __attribute__((__packed__));

