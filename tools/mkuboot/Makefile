src = mkuboot.c copy_elf32.c copy_elf64.c

mkuboot: $(src)
	@$(CC) -o $@ $(src) $(LDFLAGS) -lz


.PHONY: clean
clean:
	@rm -f mkuboot

