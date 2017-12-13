CLEAN += arm/proc0/proc0.elf \
         arm/proc0/proc0.bin \
         arm/proc0/proc0.list

PROC0_SRC_A := \
   lib/libc/sys.S

PROC0_SRC_C := \
  arm/proc0/main.c              \
  lib/libc/util.c
				

PROC0_OBJECTS := \
  $(PROC0_SRC_A:%.S=%.o) \
  $(PROC0_SRC_C:%.c=%.o)

CLEAN += $(PROC0_OBJECTS)

arm/proc0/proc0.elf: arm/user.ld $(PROC0_OBJECTS)
	$(LD) $(LDFLAGS) \
		-T arm/user.ld -Ttext $(USER_ADDR) \
		-o $@ $(PROC0_OBJECTS) \
		-lgcc

