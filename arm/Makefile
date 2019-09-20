include arm/rules.mk

LIBS += \
	arm/lib/c \
	lib/pool \
	lib/mem \
	lib/string \
	lib/block \
	lib/fat \
	lib/log \
	lib/net \
	lib/sdmmc

BUNDLE_BIN =
BUNDLE_ELF =

.for L in $(LIBS)
include $(L)/Makefile
.endfor

.for P in $(PROCS) $(IDLE) $(DRIVERS)
include $(P)/Makefile
BUNDLE_BIN += $(P)/out.bin
BUNDLE_ELF += $(P)/out.elf
.endfor

BOOT_SRC_A = \
	arm/kern/boot_start.S \
	arm/kern/asm.S 

BOOT_SRC_C = \
	arm/kern/boot.c \
	arm/lib/c/mmu.c \
	lib/mem/util.c 

KERNEL_SRC_A = \
	arm/kern/vectors.S \
	arm/kern/asm.S \
	$(BOARD_KERNEL_SRC_A)

KERNEL_SRC_C = \
	arm/kern/main.c \
	arm/kern/proc.c \
	arm/kern/intr.c \
	arm/lib/c/mmu.c \
	sys/debug.c \
	sys/proc.c \
	sys/sys.c \
	lib/mem/util.c \
	lib/string/conv.c \
	lib/string/scanf.c \
	lib/string/string.c \
	$(BOARD_KERNEL_SRC_C)


bundle.bin bundle.c: $(BUNDLE_BIN) $(BUNDLE_ELF)
	@echo 'BUNDLE procs   : ' $(PROCS)
	@echo 'BUNDLE idle    : ' $(IDLE)
	@echo 'BUNDLE drivers : ' $(DRIVERS)
	@PROCS="$(PROCS)" IDLE="$(IDLE)" DRIVERS="$(DRIVERS)" sh arm/mkbundle.sh

CLEAN += bundle.c
CLEAN += $(BUNDLE_BIN)
CLEAN += $(BUNDLE_ELF)

include arm/proc0/Makefile

proc0.bin: arm/proc0/out.bin
	@cp arm/proc0/out.bin proc0.bin

kernel.bo: kernel.bin
bundle.bo: bundle.bin
proc0.bo: proc0.bin

CLEAN += kernel.bo kernel.bin
CLEAN += proc0.bo proc0.bin
CLEAN += bundle.bo bundle.bin

KERNEL_OBJECTS = \
	$(KERNEL_SRC_A:%.S=%.o) \
	$(KERNEL_SRC_C:%.c=%.o)

CLEAN += $(KERNEL_OBJECTS) 
CLEAN += kernel.elf kernel.bin kernel.list

kernel.elf: arm/kern/kernel.ld $(KERNEL_OBJECTS) 
	@echo LD $@
	$(LD) $(LDFLAGS) \
		-T arm/kern/kernel.ld \
		-o $@ \
		$(KERNEL_OBJECTS) \
		-lgcc

BOOT_OBJECTS = \
	$(BOOT_SRC_A:%.S=%.o) \
	$(BOOT_SRC_C:%.c=%.o)

CLEAN += $(BOOT_OBJECTS) 
CLEAN += boot.elf boot.bin boot.list

boot.elf: arm/kern/boot.ld $(BOOT_OBJECTS) kernel.bo proc0.bo bundle.bo
	@echo LD $@
	$(LD) $(LDFLAGS) \
		-T arm/kern/boot.ld \
		-Ttext $(LOAD_ADDR) \
		-o $@ \
		$(BOOT_OBJECTS) \
		kernel.bo proc0.bo bundle.bo \
		-lgcc

CLEAN += kern.umg

kern.umg: boot.bin
	$(MKIMAGE) -A arm -O linux -T kernel -C none \
		-a $(LOAD_ADDR) -e $(LOAD_ADDR) \
		-d boot.bin kern.umg

.PHONY: clean
clean:
	@rm -f $(CLEAN)


