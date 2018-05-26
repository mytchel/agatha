.SUFFIXES: .c .S .h .o .a .elf .bin .list .umg .bo

.PHONY: clean
clean:
	@echo Clean
	rm -f $(CLEAN)
	for B in $(BUNDLE); do \
		$(MAKE) -C $$B clean; \
	done

KERNEL_SRC_A += \
        $(BASE)/arm/kern/vectors.S     \
        $(BASE)/arm/kern/asm.S

KERNEL_SRC_C += \
        $(BASE)/arm/kern/main.c       \
        $(BASE)/arm/kern/mem.c        \
        $(BASE)/arm/kern/proc.c       \
        $(BASE)/arm/kern/intr.c       \
        $(BASE)/kern/debug.c          \
        $(BASE)/kern/proc.c           \
        $(BASE)/kern/sys.c            \
        $(BASE)/kern/util.c           \
        $(BASE)/lib/libstring/conv.c  \
        $(BASE)/lib/libstring/scanf.c \
        $(BASE)/lib/libstring/string.c

KERNEL_OBJECTS = \
	$(KERNEL_SRC_A:%.S=%.o) \
	$(KERNEL_SRC_C:%.c=%.o) 

bundle.bin bundle.list: $(BASE)/rules.mk
		@sh $(BASE)/bundle.sh \
				bundle.bin \
				bundle.list \
				$(BUNDLE)

bundle.bo: bundle.bin

CLEAN += bundle.list bundle.bin bundle.bo

include $(BASE)/arm/proc0/Makefile

proc0.bin: $(BASE)/arm/proc0/proc0.elf
proc0.bo: proc0.bin

CLEAN += proc0.bo proc0.bin

CLEAN += kern.elf kern.bin kern.umg kern.list
CLEAN += $(KERNEL_OBJECTS)

kern.elf: $(BASE)/arm/kernel.ld  proc0.bo bundle.bo $(KERNEL_OBJECTS)
	@echo LD $@
	$(LD) $(LDFLAGS) \
			-T $(BASE)/arm/kernel.ld -Ttext $(LOAD_ADDR) \
			-o $@ $(KERNEL_OBJECTS) proc0.bo bundle.bo \
			-lgcc


