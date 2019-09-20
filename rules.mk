.SUFFIXES: .c .S .h .o .a .elf .bin .list .umg .bo

CFLAGS = \
	-std=c89 \
	-Wall \
	-nostdinc \
	-ffreestanding \
	-Iinclude

LDFLAGS = \
	-nostdlib \
	-nodefaultlibs \
	-static \
	-Llib

.c.o .S.o:
	@echo CC $@
	$(CC) $(CFLAGS) -c $< -o $@

.elf.bin:
	@echo BIN $@
	$(OBJCOPY) -Obinary $< $@

.elf.list:
	@echo LIST $@
	$(OBJDUMP) -S $< > $@

