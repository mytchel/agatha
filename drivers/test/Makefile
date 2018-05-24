CLEAN += \
	$(BASE)/drivers/test/test.elf \
	$(BASE)/drivers/test/test.bin \
	$(BASE)/drivers/test/test.list

TEST_SRC_A = \
	$(BASE)/lib/libc/sys.S

TEST_SRC_C = \
	$(BASE)/drivers/test/main.c          \
	$(BASE)/lib/libc/util.c              \
	$(BASE)/lib/libstring/conv.c         \
	$(BASE)/lib/libstring/scanf.c        \
	$(BASE)/lib/libstring/string.c


TEST_OBJECTS = \
	$(TEST_SRC_A:%.S=%.o) \
	$(TEST_SRC_C:%.c=%.o)

CLEAN += $(TEST_OBJECTS)
CLEAN += $(BASE)/drivers/test/test.elf
CLEAN += $(BASE)/drivers/test/test.bin

$(BASE)/drivers/test/test.elf: $(USER.LD) $(TEST_OBJECTS)
	@echo LD $@
	@$(LD) $(LDFLAGS) \
		-T $(USER.LD) -Ttext $(USER_ADDR) \
		-o $@ $(TEST_OBJECTS) \
		-lgcc

