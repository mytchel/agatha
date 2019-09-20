VIRT_NET_OUT = arm/drivers/virtio/net/out.elf

VIRT_NET_SRC_C = \
	arm/drivers/virtio/net/net.c \
	arm/drivers/virtio/virtq.c

VIRT_NET_LIBS = lib/net lib/log $(USER_LIBS)

VIRT_NET_OBJECTS = \
	$(VIRT_NET_SRC_C:%.c=%.o) \
	$(VIRT_NET_LIBS:%=%/lib.a)

$(VIRT_NET_OUT): $(VIRT_NET_OBJECTS) 
	@echo LD $@ $(OBJECTS)
	@$(LD) $(LDFLAGS_USER) \
		-o $@ \
		$(VIRT_NET_OBJECTS) \
		-lgcc

CLEAN += $(VIRT_NET_OBJECTS)
CLEAN += $(VIRT_NET_OUT)

