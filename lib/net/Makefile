LIB_NET_LIB = lib/net/lib.a

LIB_NET_SRC_A = 

LIB_NET_SRC_C = \
	lib/net/net.c \
	lib/net/util.c \
	lib/net/eth.c \
	lib/net/arp.c \
	lib/net/ip.c \
	lib/net/tcp.c \
	lib/net/udp.c \
	lib/net/icmp.c

LIB_NET_OBJECTS = \
	$(LIB_NET_SRC_A:%.S=%.o) \
	$(LIB_NET_SRC_C:%.c=%.o) 

$(LIB_NET_LIB): $(LIB_NET_OBJECTS)
	@echo AR $@
	$(AR) rc $@ $(LIB_NET_OBJECTS)
	$(RANLIB) $@

CLEAN += $(LIB_NET_OBJECTS)
CLEAN += $(LIB_NET_LIB)

