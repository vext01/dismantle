LDFLAGS=	-ludis86 -L/usr/local/lib -lelf
CPPFLAGS=	-I/usr/local/include
CFLAGS=		-g -O0

all: dismantle

dismantle: dismantle.c
	${CC} ${CPPFLAGS} ${CFLAGS} ${LDFLAGS} -o dismantle dismantle.c
