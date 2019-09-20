VIDEO_AM335X_OUT = arm/drivers/video-am335x/out.elf

VIDEO_AM335X_SRC_C = \
	arm/drivers/video-am335x/main.c

VIDEO_AM335X_LIBS = lib/log $(USER_LIBS)

VIDEO_AM335X_OBJECTS = \
	$(VIDEO_AM335X_SRC_C:%.c=%.o) \
	$(VIDEO_AM335X_LIBS:%=%/lib.a)

$(VIDEO_AM335X_OUT): $(VIDEO_AM335X_OBJECTS) 
	@echo LD $@ $(OBJECTS)
	@$(LD) $(LDFLAGS_USER) \
		-o $@ \
		$(VIDEO_AM335X_OBJECTS) \
		-lgcc

CLEAN += $(VIDEO_AM335X_OBJECTS)
CLEAN += $(VIDEO_AM335X_OUT)

