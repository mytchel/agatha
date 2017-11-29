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

#ifndef _SYS_H_
#define _SYS_H_

#define ASSERT_MESSAGE_SIZE(m) \
    STATIC_ASSERT(sizeof(struct m) <= MESSAGE_LEN, \
                  too_big##m)

typedef enum {
	MESSAGE_addr,
	MESSAGE_proc_init,
} message_t;



typedef struct addr_req *addr_req_t;
typedef struct addr_resp *addr_resp_t;

struct addr_req {
	message_t type; /* = MESSAGE_addr */
	reg_t addr;
	size_t len;
};

ASSERT_MESSAGE_SIZE(addr_req);

struct addr_resp {
	message_t type; /* = MESSAGE_addr */
	reg_t addr;
};

ASSERT_MESSAGE_SIZE(addr_resp);

typedef struct proc_init_req *proc_init_req_t;

struct proc_init_req {
	message_t type; /* = MESSAGE_proc_init */
	reg_t pc;
	reg_t sp;
};

ASSERT_MESSAGE_SIZE(proc_init_req);

struct proc_init_resp {
	message_t type; /* = MESSAGE_proc_init */
};

ASSERT_MESSAGE_SIZE(proc_init_resp);

#endif
