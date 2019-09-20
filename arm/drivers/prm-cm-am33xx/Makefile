PRM_CMMAM33XX_OUT = arm/drivers/prm-cm-am33xx/out.elf

PRM_CMMAM33XX_SRC_C = \
	arm/drivers/prm-cm-am33xx/main.c

PRM_CMMAM33XX_LIBS = lib/log $(USER_LIBS)

PRM_CMMAM33XX_OBJECTS = \
	$(PRM_CMMAM33XX_SRC_C:%.c=%.o) \
	$(PRM_CMMAM33XX_LIBS:%=%/lib.a)

$(PRM_CMMAM33XX_OUT): $(PRM_CMMAM33XX_OBJECTS) 
	@echo LD $@ $(OBJECTS)
	$(LD) $(LDFLAGS_USER) \
		-o $@ \
		$(PRM_CMMAM33XX_OBJECTS) \
		-lgcc

CLEAN += $(PRM_CMMAM33XX_OBJECTS)
CLEAN += $(PRM_CMMAM33XX_OUT)

