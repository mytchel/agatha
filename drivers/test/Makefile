all: test.elf

include ../../rules.mk

CLEAN = \
	test.elf \
	test.bin \
	test.list

TEST_SRC_A = \
	../../lib/libc/sys.S

TEST_SRC_C = \
	main.c          \
	../../lib/libc/util.c              \
	../../lib/libstring/conv.c         \
	../../lib/libstring/scanf.c        \
	../../lib/libstring/string.c


TEST_OBJECTS = \
	$(TEST_SRC_A:%.S=%.o) \
	$(TEST_SRC_C:%.c=%.o)

CLEAN += $(TEST_OBJECTS)

test.elf: $(USER.LD) $(TEST_OBJECTS)
	@echo LD $@
	$(LD) $(LDFLAGS) \
		-T $(USER.LD) -Ttext $(USER_ADDR) \
		-o $@ $(TEST_OBJECTS) \
		-lgcc

clean:
	@echo Clean
	rm -f $(CLEAN)

