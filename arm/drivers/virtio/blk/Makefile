VIRT_BLK_OUT = arm/drivers/virtio/blk/out.elf

VIRT_BLK_SRC_C = \
	arm/drivers/virtio/blk/blk.c \
	arm/drivers/virtio/virtq.c

VIRT_BLK_LIBS = lib/block lib/log $(USER_LIBS)

VIRT_BLK_OBJECTS = \
	$(VIRT_BLK_SRC_C:%.c=%.o) \
	$(VIRT_BLK_LIBS:%=%/lib.a)

$(VIRT_BLK_OUT): $(VIRT_BLK_OBJECTS) 
	@echo LD $@ $(OBJECTS)
	@$(LD) $(LDFLAGS_USER) \
		-o $@ \
		$(VIRT_BLK_OBJECTS) \
		-lgcc

CLEAN += $(VIRT_BLK_OBJECTS)
CLEAN += $(VIRT_BLK_OUT)

