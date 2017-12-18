CLEAN += arm/proc0/proc0.elf \
         arm/proc0/proc0.bin \
         arm/proc0/proc0.list

PROC0_SRC_A := \
   lib/libc/sys.S

PROC0_SRC_C := \
  arm/proc0/main.c              \
  arm/proc0/mem.c               \
  arm/proc0/proc.c              \
  lib/libfdt/fdt.c              \
  lib/libstring/conv.c          \
  lib/libstring/scanf.c         \
  lib/libstring/string.c        \
  lib/libc/util.c
				

PROC0_OBJECTS := \
  $(PROC0_SRC_A:%.S=%.o) \
  $(PROC0_SRC_C:%.c=%.o)

CLEAN += $(PROC0_OBJECTS)

BUNDLE = arm/uart

include $(foreach b,$(BUNDLE),$b/Makefile)

bundle.bin bundle.list: \
  $(foreach b,$(BUNDLE),$b/$(notdir $b).elf) \
  $(foreach b,$(BUNDLE),$b/$(notdir $b).bin)
	sh bundle.sh bundle.bin bundle.list $(BUNDLE)

bundle.bo: bundle.bin

arm/proc0/proc0.elf: arm/user.ld bundle.bo $(PROC0_OBJECTS)
	$(LD) $(LDFLAGS) \
		-T arm/user.ld -Ttext $(USER_ADDR) \
		-o $@ $(PROC0_OBJECTS) bundle.bo \
		-lgcc

