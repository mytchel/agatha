VIRT_GPU_OUT = arm/drivers/virtio/gpu/out.elf

VIRT_GPU_SRC_C = \
	arm/drivers/virtio/gpu/gpu.c \
	arm/drivers/virtio/virtq.c

VIRT_GPU_LIBS = lib/log $(USER_LIBS)

VIRT_GPU_OBJECTS = \
	$(VIRT_GPU_SRC_C:%.c=%.o) \
	$(VIRT_GPU_LIBS:%=%/lib.a)

$(VIRT_GPU_OUT): $(VIRT_GPU_OBJECTS) 
	@echo LD $@ $(OBJECTS)
	$(LD) $(LDFLAGS_USER) \
		-o $@ \
		$(VIRT_GPU_OBJECTS) \
		-lgcc

CLEAN += $(VIRT_GPU_OBJECTS)
CLEAN += $(VIRT_GPU_OUT)

