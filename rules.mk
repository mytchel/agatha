.SUFFIXES: .c .S .h .o .a .elf .bin .list .umg .bo

CFLAGS = \
	-std=c89 \
	-Wall \
	-nostdinc -ffreestanding \
	-I$(BASE)/include

LDFLAGS = -nostdlib -nodefaultlibs -static

include ${BASE}/${ARCH}/rules.mk

.c.o .S.o:
	@echo CC $@
	$(CC) $(CFLAGS) -c $< -o $@

.o.a:
	@echo AR $@
	@$(AR) rcs $@ $<

.elf.bin:
	@echo OBJCOPY $@
	$(OBJCOPY) -Obinary $< $@

.elf.list:
	@echo OBJDUMP $@
	$(OBJDUMP) -S $< > $@

.bin.umg: 
	@make -C $(BASE)/tools/mkuboot
	@echo MKUBOOT $@
	@$(BASE)/tools/mkuboot/mkuboot \
		-a arm -o linux -t kernel \
		-e $(LOAD_ADDR) -l $(LOAD_ADDR) \
		$< $@

.bin.bo: 
	@echo OBJCOPY $@
	$(OBJCOPY) -B arm -O elf32-littlearm -I binary \
		--rename-section .data=.bundle \
		$< $@


