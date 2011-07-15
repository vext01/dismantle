LDFLAGS=	-ludis86 -L/usr/local/lib
CPPFLAGS=	-I/usr/local/include
CFLAGS=		-g

all: dismantle

dismantle: dismantle.c
	${CC} ${CPPFLAGS} ${CFLAGS} ${LDFLAGS} -o dismantle dismantle.c
