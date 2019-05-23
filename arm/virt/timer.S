.global timer_read_count
.global timer_read_cmp
.global timer_read_ctrl
.global timer_read_freq
.global timer_write_cmp
.global timer_write_ctrl

timer_read_count:
	mrrc p15, 0, r0, r1, c14
	bx lr

timer_read_cmp:
	mrrc p15, 2, r0, r1, c14
	bx lr

timer_write_cmp:
	mcrr p15, 2, r0, r1, c14
	bx lr

timer_read_ctrl:
	mrc p15, 0, r0, c14, c2, 1
	bx lr

timer_write_ctrl:
	mcr p15, 0, r0, c14, c2, 1
	bx lr

timer_read_freq:
	mrc p15, 0, r0, c14, c0, 0
	bx lr

