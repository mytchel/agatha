PROG = pong

all: $(PROG).elf

include ../../rules.mk

CLEAN = \
	$(PROG).elf \
	$(PROG).bin \
	$(PROG).list

SRC_A = \
	../../lib/libc/sys.S

SRC_C = \
	main.c                         \
	../../lib/libc/util.c          \
	../../lib/libstring/conv.c     \
	../../lib/libstring/scanf.c    \
	../../lib/libstring/string.c


OBJECTS = \
	$(SRC_A:%.S=%.o) \
	$(SRC_C:%.c=%.o)

CLEAN += $(OBJECTS)

$(PROG).elf: $(USER.LD) $(OBJECTS)
	@echo LD $@
	$(LD) $(LDFLAGS) \
		-T $(USER.LD) -Ttext $(USER_ADDR) \
		-o $@ $(OBJECTS) \
		-lgcc

clean:
	@echo Clean
	rm -f $(CLEAN)

