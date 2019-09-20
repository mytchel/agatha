VIRT_INPUT_OUT = arm/drivers/virtio/input/out.elf

VIRT_INPUT_SRC_C = \
	arm/drivers/virtio/input/input.c \
	arm/drivers/virtio/virtq.c

VIRT_INPUT_LIBS = lib/log $(USER_LIBS)

VIRT_INPUT_OBJECTS = \
	$(VIRT_INPUT_SRC_C:%.c=%.o) \
	$(VIRT_INPUT_LIBS:%=%/lib.a)

$(VIRT_INPUT_OUT): $(VIRT_INPUT_OBJECTS) 
	@echo LD $@ $(OBJECTS)
	$(LD) $(LDFLAGS_USER) \
		-o $@ \
		$(VIRT_INPUT_OBJECTS) \
		-lgcc

CLEAN += $(VIRT_INPUT_OBJECTS)
CLEAN += $(VIRT_INPUT_OUT)

