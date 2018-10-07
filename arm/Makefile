LIBS = \
			 ../lib/libc \
			 ../../lib/libstring \
			 ../../lib/libsdmmc \
			 ../../lib/libblock \
			 ../../lib/libfat \
			 ../../lib/libfdt

.PHONY: clean
clean:
	@rm -f $(CLEAN)
	@for B in $(BUNDLE); do \
		$(MAKE) -C $$B BASE=${BASE} ARCH=${ARCH} BOARD=${BOARD} clean; \
	done
	@for L in $(LIBS); do \
		$(MAKE) -C $$L BASE=${BASE} ARCH=${ARCH} BOARD=${BOARD} clean; \
	done

.PHONE: build_libs
build_libs:
	@for L in $(LIBS); do \
		$(MAKE) -C $$L BASE=${BASE} ARCH=${ARCH} BOARD=${BOARD}; \
	done

KERNEL_SRC_A = \
								../kern/vectors.S     \
								../kern/asm.S         \
								$(BOARD_KERNEL_SRC_A)

KERNEL_SRC_C = \
								../kern/main.c               \
								../kern/proc.c               \
								../kern/intr.c               \
								../lib/libc/mmu.c            \
								../lib/libc/util.c           \
								../../sys/debug.c            \
								../../sys/proc.c             \
								../../sys/sys.c              \
								../../lib/libstring/conv.c   \
								../../lib/libstring/scanf.c  \
								../../lib/libstring/string.c \
								$(BOARD_KERNEL_SRC_C)

.PHONY: bundle.bin 
bundle.bin bundle.c:
	@for B in $(BUNDLE); do \
		echo $(MAKE) -C $$B; \
		$(MAKE) -C $$B BASE=${BASE} ARCH=${ARCH} BOARD=${BOARD} || exit 1; \
		done
	@echo BUNDLE $(BUNDLE)
	@sh ../bundle.sh \
		bundle.bin \
		bundle.c \
		$(BUNDLE)

bundle.bo: bundle.bin

CLEAN += bundle.c bundle.bin bundle.bo

include ../proc0/Makefile

proc0.bin: ../proc0/proc0.bin
	@cp ../proc0/proc0.bin proc0.bin

proc0.bo: proc0.bin

CLEAN += proc0.bo proc0.bin

KERNEL_OBJECTS = $(KERNEL_SRC_A:%.S=%.o) $(KERNEL_SRC_C:%.c=%.o)

CLEAN += $(KERNEL_OBJECTS) 
CLEAN += kern.elf kern.bin kern.umg kern.list
		
kern.elf: ../kernel.ld $(KERNEL_OBJECTS) bundle.bo proc0.bo 
	@echo LD $@
	@$(LD) $(LDFLAGS) \
		-T ../kernel.ld \
		-Ttext $(LOAD_ADDR) \
		-o $@ \
		$(KERNEL_OBJECTS) \
		proc0.bo bundle.bo \
		-lgcc

kern.umg: kern.bin
	$(MKIMAGE) -A arm -O linux -T kernel -C none \
		-a $(LOAD_ADDR) -e $(LOAD_ADDR) \
		-d kern.bin kern.umg

