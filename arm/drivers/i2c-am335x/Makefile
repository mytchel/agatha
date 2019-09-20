I2C_AM335X_OUT = arm/drivers/i2c-am335x/out.elf

I2C_AM335X_SRC_C = \
	arm/drivers/i2c-am335x/main.c

I2C_AM335X_LIBS = lib/log $(USER_LIBS)

I2C_AM335X_OBJECTS = \
	$(I2C_AM335X_SRC_C:%.c=%.o) \
	$(I2C_AM335X_LIBS:%=%/lib.a)

$(I2C_AM335X_OUT): $(I2C_AM335X_OBJECTS) 
	@echo LD $@ $(OBJECTS)
	$(LD) $(LDFLAGS_USER) \
		-o $@ \
		$(I2C_AM335X_OBJECTS) \
		-lgcc

CLEAN += $(I2C_AM335X_OBJECTS)
CLEAN += $(I2C_AM335X_OUT)

