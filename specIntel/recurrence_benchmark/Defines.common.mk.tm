
CFLAGS +=

PROG := recurrence_TM

SRCS += \
	recurrence.c \
	$(LIB)/thread.c \
        $(LIB)/memory.c
#
OBJS := ${SRCS:.c=.o}

