
CFLAGS +=

PROG := stencil_CS

SRCS += \
	main.c \
        parboil.c \
        file.c \
        kernels.c \
	$(LIB)/thread.c \
        $(LIB)/memory.c
#
OBJS := ${SRCS:.c=.o}

