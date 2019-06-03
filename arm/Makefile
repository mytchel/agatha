LIBS = \
			 ../lib/c \
			 ../../lib/string \
			 ../../lib/pool \
			 ../../lib/mem \
			 ../../lib/log \
			 ../../lib/sdmmc \
			 ../../lib/block \
			 ../../lib/fat

.PHONY: clean
clean:
	@rm -f $(CLEAN)
	@for B in $(PROCS) $(IDLE) $(DRIVERS) $(LIBS); do \
		$(MAKE) -C $$B BASE=${BASE} ARCH=${ARCH} BOARD=${BOARD} clean; \
	done

.PHONE: build_libs
build_libs:
	@for L in $(LIBS); do \
		$(MAKE) -C $$L BASE=${BASE} ARCH=${ARCH} BOARD=${BOARD}; \
	done

BOOT_SRC_A = \
							../kern/boot_start.S \
							../kern/asm.S 

BOOT_SRC_C = \
						 ../kern/boot.c \
						 ../lib/c/mmu.c \
						 ../../lib/mem/util.c 

KERNEL_SRC_A = \
							 ../kern/vectors.S \
							 ../kern/asm.S \
							 $(BOARD_KERNEL_SRC_A)

KERNEL_SRC_C = \
							 ../kern/main.c \
							 ../kern/proc.c \
							 ../kern/intr.c \
							 ../lib/c/mmu.c \
							 ../../sys/debug.c \
							 ../../sys/proc.c \
							 ../../sys/sys.c \
						   ../../lib/mem/util.c \
							 ../../lib/string/conv.c \
							 ../../lib/string/scanf.c \
							 ../../lib/string/string.c \
							 $(BOARD_KERNEL_SRC_C)

.PHONY: bundle.bin 
bundle.bin bundle.c:
	@for B in $(PROCS) $(IDLE) $(DRIVERS); do \
		echo $(MAKE) -C $$B; \
		$(MAKE) -C $$B BASE=${BASE} ARCH=${ARCH} BOARD=${BOARD} || exit 1; \
		done
	@echo 'BUNDLE procs   : ' $(PROCS)
	@echo 'BUNDLE idle    : ' $(IDLE)
	@echo 'BUNDLE drivers : ' $(DRIVERS)
	@PROCS="$(PROCS)" IDLE="$(IDLE)" DRIVERS="$(DRIVERS)" sh ../mkbundle.sh

CLEAN += bundle.c

include ../proc0/Makefile

proc0.bin: ../proc0/proc0.bin
	@cp ../proc0/proc0.bin proc0.bin

kernel.bo: kernel.bin
bundle.bo: bundle.bin
proc0.bo: proc0.bin

CLEAN += kernel.bo kernel.bin
CLEAN += proc0.bo proc0.bin
CLEAN += bundle.bo bundle.bin

KERNEL_OBJECTS = $(KERNEL_SRC_A:%.S=%.o) $(KERNEL_SRC_C:%.c=%.o)

CLEAN += $(KERNEL_OBJECTS) 
CLEAN += kernel.elf kernel.bin kernel.list

kernel.elf: ../kern/kernel.ld $(KERNEL_OBJECTS) 
	@echo LD $@
	$(LD) $(LDFLAGS) \
		-T ../kern/kernel.ld \
		-o $@ \
		$(KERNEL_OBJECTS) \
		-lgcc

BOOT_OBJECTS = $(BOOT_SRC_A:%.S=%.o) $(BOOT_SRC_C:%.c=%.o)

CLEAN += $(BOOT_OBJECTS) 
CLEAN += boot.elf boot.bin boot.list

boot.elf: ../kern/boot.ld $(BOOT_OBJECTS) kernel.bo proc0.bo bundle.bo
	@echo LD $@
	$(LD) $(LDFLAGS) \
		-T ../kern/boot.ld \
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

