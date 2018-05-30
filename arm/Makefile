.PHONY: clean
clean:
	@echo Clean
	rm -f $(CLEAN)
	for B in $(BUNDLE); do \
		$(MAKE) -C $$B BASE=${BASE} ARCH=${ARCH} BOARD=${BOARD} \
		clean; \
	done

BOOT_SRC_A += \
        ../kern/boot_start.S  \
        ../kern/asm.S

BOOT_SRC_C += \
        ../kern/boot.c        \
        ../lib/libmem/mmu.c

BOOT_OBJECTS = \
	$(BOOT_SRC_A:%.S=%.o) \
	$(BOOT_SRC_C:%.c=%.o) 

KERNEL_SRC_A += \
        ../kern/vectors.S     \
        ../kern/asm.S

KERNEL_SRC_C += \
        ../kern/main.c       \
        ../kern/proc.c       \
        ../kern/intr.c       \
        ../../port/debug.c          \
        ../../port/proc.c           \
        ../../port/sys.c            \
        ../../port/util.c           \
        ../lib/libmem/mmu.c         \
        ../../lib/libstring/conv.c  \
        ../../lib/libstring/scanf.c \
        ../../lib/libstring/string.c

KERNEL_OBJECTS = \
	$(KERNEL_SRC_A:%.S=%.o) \
	$(KERNEL_SRC_C:%.c=%.o) 

.PHONY: bundle.bin bundle.c
bundle.bin bundle.c:
	BASE=${BASE} ARCH=${ARCH} BOARD=${BOARD} \
		sh ../bundle.sh \
				bundle.bin \
				bundle.c \
				$(BUNDLE)

bundle.bo: bundle.bin

CLEAN += bundle.c bundle.bin bundle.bo

include ../proc0/Makefile

proc0.bin: ../proc0/proc0.bin
	cp ../proc0/proc0.bin proc0.bin

proc0.bo: proc0.bin

CLEAN += proc0.bo proc0.bin

CLEAN += kern.elf kern.bin kern.umg kern.list
CLEAN += $(KERNEL_OBJECTS)

kern.elf: ../kernel.ld $(KERNEL_OBJECTS)
	@echo LD $@
	$(LD) $(LDFLAGS) \
			-T ../kernel.ld \
			-Ttext $(KERNEL_BASE) \
			-o $@ $(KERNEL_OBJECTS) \
			-lgcc

boot.elf: ../boot.ld $(BOOT_OBJECTS) kern.bo proc0.bo bundle.bo
	@echo LD $@
	$(LD) $(LDFLAGS) \
			-T ../boot.ld \
			-Ttext $(LOAD_ADDR) \
			-o $@ $(BOOT_OBJECTS) \
			kern.bo proc0.bo bundle.bo \
			-lgcc

