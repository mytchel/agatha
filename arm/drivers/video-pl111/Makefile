VIDEO_PL111_OUT = arm/drivers/video-pl111/out.elf

VIDEO_PL111_SRC_C = \
	arm/drivers/video-pl111/pl111.c

VIDEO_PL111_LIBS = \
	lib/sdmmc \
	lib/block \
	lib/log \
	$(USER_LIBS)

VIDEO_PL111_OBJECTS = \
	$(VIDEO_PL111_SRC_C:%.c=%.o) \
	$(VIDEO_PL111_LIBS:%=%/lib.a)

$(VIDEO_PL111_OUT): $(VIDEO_PL111_OBJECTS) 
	@echo LD $@ $(OBJECTS)
	@$(LD) $(LDFLAGS_USER) \
		-o $@ \
		$(VIDEO_PL111_OBJECTS) \
		-lgcc

CLEAN += $(VIDEO_PL111_OBJECTS)
CLEAN += $(VIDEO_PL111_OUT)

