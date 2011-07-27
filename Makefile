LDFLAGS=	-ludis86 -L/usr/local/lib -lelf -lreadline -ltermcap -ldwarf
CPPFLAGS=	-I/usr/local/include
CFLAGS=		-g -Wall -Wextra

all: dismantle

dm_dwarf.o: dm_dwarf.c dm_dwarf.h
	${CC} -c ${CPPFLAGS} ${CLAGS} ${LDFLAGS} -o dm_dwarf.o dm_dwarf.c

dismantle: dismantle.c dm_dwarf.o
	${CC} ${CPPFLAGS} ${CFLAGS} ${LDFLAGS} -o dismantle \
		dm_dwarf.o dismantle.c

clean:
	rm -f dm_dwarf.o dismantle
