.SUFFIXES: .c .S .h .o .a .elf .bin .list .umg .bo

.PHONY: clean
clean:
	@echo Clean
	rm -f $(CLEAN)
	for B in $(BUNDLE); do \
		$(MAKE) -C $$B clean; \
	done

KERNEL_SRC_A += \
        ../kern/vectors.S     \
        ../kern/asm.S

KERNEL_SRC_C += \
        ../kern/main.c       \
        ../kern/mem.c        \
        ../kern/proc.c       \
        ../kern/intr.c       \
        ../../kern/debug.c          \
        ../../kern/proc.c           \
        ../../kern/sys.c            \
        ../../kern/util.c           \
        ../../lib/libstring/conv.c  \
        ../../lib/libstring/scanf.c \
        ../../lib/libstring/string.c

KERNEL_OBJECTS = \
	$(KERNEL_SRC_A:%.S=%.o) \
	$(KERNEL_SRC_C:%.c=%.o) 

bundle.bin bundle.list: ../../rules.mk
		@sh ../bundle.sh \
				bundle.bin \
				bundle.list \
				$(BUNDLE)

bundle.bo: bundle.bin

CLEAN += bundle.list bundle.bin bundle.bo

include ../proc0/Makefile

proc0.bin: ../proc0/proc0.bin
	cp ../proc0/proc0.bin proc0.bin

proc0.bo: proc0.bin

CLEAN += proc0.bo proc0.bin

CLEAN += kern.elf kern.bin kern.umg kern.list
CLEAN += $(KERNEL_OBJECTS)

kern.elf: ../kernel.ld  proc0.bo bundle.bo $(KERNEL_OBJECTS)
	@echo LD $@
	$(LD) $(LDFLAGS) \
			-T ../kernel.ld -Ttext $(LOAD_ADDR) \
			-o $@ $(KERNEL_OBJECTS) proc0.bo bundle.bo \
			-lgcc

../../rules.mk: ../../port_rules.mk ../rules.mk rules.mk
	rm -f ../../rules.mk
	cat ../../port_rules.mk >> ../../rules.mk
	echo BASE = ${BASE} >> ../../rules.mk
	cat ../rules.mk >> ../../rules.mk
	cat rules.mk >> ../../rules.mk

