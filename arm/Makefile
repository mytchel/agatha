include arm/rules.mk

LIBS += \
	arm/lib/c \
	lib/pool \
	lib/string \
	lib/log \
#	lib/block \
#	lib/net \
#	lib/sdmmc \
#	lib/fat
#	lib/mem \

BUNDLE_BIN =
BUNDLE_ELF =

.for L in $(LIBS)
include $(L)/Makefile
.endfor

.for P in $(IDLE) $(DRIVERS) $(PROCS) 
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
	arm/kern/sys.c \
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
	@echo 'BUNDLE idle    : ' $(IDLE)
	@echo 'BUNDLE drivers : ' $(DRIVERS)
	@echo 'BUNDLE procs   : ' $(PROCS)
	@IDLE="$(IDLE)" PROCS="$(DRIVERS) $(PROCS)" sh arm/mkbundle.sh

CLEAN += bundle.c
CLEAN += $(BUNDLE_BIN)
CLEAN += $(BUNDLE_ELF)

include arm/root/Makefile

root.bin: arm/root/out.bin
	cp arm/root/out.bin root.bin

kernel.bo: kernel.bin
bundle.bo: bundle.bin
root.bo: root.bin

CLEAN += kernel.bo kernel.bin
CLEAN += root.bo root.bin
CLEAN += bundle.bo bundle.bin

KERNEL_OBJECTS = \
	$(KERNEL_SRC_A:%.S=%.o) \
	$(KERNEL_SRC_C:%.c=%.o)

CLEAN += $(KERNEL_OBJECTS) 
CLEAN += kernel.elf kernel.bin kernel.list

kernel.elf: arm/kern/kernel.ld $(KERNEL_OBJECTS) 
	@echo LD $@
	@$(LD) $(LDFLAGS) \
		-T arm/kern/kernel.ld \
		-o $@ \
		$(KERNEL_OBJECTS) \
		-lgcc

BOOT_OBJECTS = \
	$(BOOT_SRC_A:%.S=%.o) \
	$(BOOT_SRC_C:%.c=%.o)

CLEAN += $(BOOT_OBJECTS) 
CLEAN += boot.elf boot.bin boot.list

boot.elf: arm/kern/boot.ld $(BOOT_OBJECTS) kernel.bo root.bo bundle.bo
	@echo LD $@
	@$(LD) $(LDFLAGS) \
		-T arm/kern/boot.ld \
		-Ttext $(LOAD_ADDR) \
		-o $@ \
		$(BOOT_OBJECTS) \
		kernel.bo root.bo bundle.bo \
		-lgcc

CLEAN += kern.umg

kern.umg: boot.bin
	$(MKIMAGE) -A arm -O linux -T kernel -C none \
		-a $(LOAD_ADDR) -e $(LOAD_ADDR) \
		-d boot.bin kern.umg

.PHONY: clean
clean:
	@rm -f $(CLEAN)


