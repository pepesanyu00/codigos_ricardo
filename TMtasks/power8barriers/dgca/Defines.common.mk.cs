
CFLAGS +=

PROG := dgca_CS

SRCS += \
	dgca.c \
	$(LIB)/thread.c \
	$(LIB)/random.c \
	$(LIB)/mt19937ar.c \
	$(LIB)/memory.c
#
OBJS := ${SRCS:.c=.o}

