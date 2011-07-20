LDFLAGS=	-ludis86 -L/usr/local/lib -lelf -lreadline -ltermcap
CPPFLAGS=	-I/usr/local/include
CFLAGS=		-g -Wall -Wextra

all: dismantle

dismantle: dismantle.c
	${CC} ${CPPFLAGS} ${CFLAGS} ${LDFLAGS} -o dismantle dismantle.c
