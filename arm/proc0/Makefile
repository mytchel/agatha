CLEAN += arm/proc0/proc0.elf \
         arm/proc0/proc0.bin \
         arm/proc0/proc0.list

PROC0_SRC_A := \
	lib/libc/sys.S

PROC0_SRC_C := \
	arm/proc0/main.c              \
	arm/proc0/proc.c              \
	arm/proc0/mem.c               \
	arm/proc0/mmu.c               \
	lib/libfdt/fdt.c              \
	lib/libstring/conv.c          \
	lib/libstring/scanf.c         \
	lib/libstring/string.c        \
	lib/libc/util.c
				

PROC0_OBJECTS := \
  $(PROC0_SRC_A:%.S=%.o) \
  $(PROC0_SRC_C:%.c=%.o)

CLEAN += $(PROC0_OBJECTS)

BUNDLE = drivers/test
#drivers/omap/uart drivers/omap/mmc drivers/test

arm/proc0/bundle.bin arm/proc0/bundle.list: \
  $(foreach b,$(BUNDLE),$b/$(notdir $b).elf) \
  $(foreach b,$(BUNDLE),$b/$(notdir $b).bin)
	@echo Make Bundle
	@sh bundle.sh arm/proc0/bundle.bin arm/proc0/bundle.list $(BUNDLE)

arm/proc0/bundle.bo: arm/proc0/bundle.bin

CLEAN += arm/proc0/bundle.bin arm/proc0/bundle.list arm/proc0/bundle.bo 

arm/proc0/proc0.elf: arm/user.ld arm/proc0/bundle.bo $(PROC0_OBJECTS)
	@echo LD $@
	@$(LD) $(LDFLAGS) \
		-T arm/proc0/linker.ld -Ttext $(USER_ADDR) \
		-o $@ $(PROC0_OBJECTS) arm/proc0/bundle.bo \
		-lgcc

include $(foreach b,$(BUNDLE),$b/Makefile)

