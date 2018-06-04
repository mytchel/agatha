include ../../rules.mk

OBJECTS = \
					${SRC_A:%.S=%.o} \
					${SRC_C:%.c=%.o}

CLEAN += ${OBJECTS}
CLEAN += ../${LIB}.a

../${LIB}.a: ${OBJECTS}
	@echo AR ${LIB}.a
	@${AR} rc $@ ${OBJECTS}
	@${RANLIB} ../${LIB}.a

.PHONY: clean
clean: 
	@rm -f ${CLEAN}

