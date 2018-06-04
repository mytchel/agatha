.SUFFIXES: .c .S .h .o .a .elf .bin .list .umg .bo

CFLAGS = \
	-std=c89 \
	-Wall \
	-nostdinc -ffreestanding \
	-I$(BASE)/include

LDFLAGS = -nostdlib -nodefaultlibs -static \
					-L${BASE}/lib

include ${BASE}/${ARCH}/rules.mk

.c.o .S.o:
	@echo CC $@
	@$(CC) $(CFLAGS) -c $< -o $@

.elf.bin:
	@echo BIN $@
	@$(OBJCOPY) -Obinary $< $@

.elf.list:
	@echo LIST $@
	@$(OBJDUMP) -S $< > $@

.bin.umg: 
	@make -C $(BASE)/tools/mkuboot
	@echo MKUBOOT $@
	@$(BASE)/tools/mkuboot/mkuboot \
		-a arm -o linux -t kernel \
		-e $(LOAD_ADDR) -l $(LOAD_ADDR) \
		$< $@

.bin.bo: 
	@echo BO $@
	@$(OBJCOPY) -B arm -O elf32-littlearm -I binary \
		--rename-section .data=.bundle \
		$< $@


