LDFLAGS= 	-ludis86 -L/usr/local/lib -lelf -lreadline -ltermcap -ldwarf
CPPFLAGS=	-I/usr/local/include
CFLAGS=		-g -Wall -Wextra

all: dismantle

dismantle: dismantle.c dm_dis.o dm_elf.o dm_cfg.o dm_gviz.o dm_dom.o dm_ssa.o dm_dwarf.o
	${CC} ${CPPFLAGS} ${CFLAGS} ${LDFLAGS} -o dismantle \
		dismantle.c dm_dis.o dm_elf.o dm_cfg.o dm_gviz.o dm_dom.o dm_ssa.o dm_dwarf.o

static: dismantle-static
dismantle-static: dismantle.c dm_dis.o dm_elf.o dm_cfg.o dm_gviz.o dm_dom.o dm_ssa.o dm_dwarf.o
	${CC} ${CPPFLAGS} ${CFLAGS} ${LDFLAGS} -o dismantle \
		dismantle.c dm_dis.o dm_elf.o dm_cfg.o dm_gviz.o dm_dom.o dm_ssa.o dm_dwarf.o \
		/usr/local/lib/libudis86.a /usr/lib/libdwarf.a

dm_dis.o: dm_dis.c dm_dis.h common.h
	${CC} -c ${CPPFLAGS} ${CFLAGS} -o dm_dis.o dm_dis.c

dm_elf.o: dm_elf.c dm_elf.h common.h
	${CC} -c ${CPPFLAGS} ${CFLAGS} -o dm_elf.o dm_elf.c

dm_cfg.o: dm_cfg.c dm_cfg.h dm_dis.o
	${CC} -c ${CPPFLAGS} ${CFLAGS} -o dm_cfg.o dm_cfg.c

dm_gviz.o: dm_gviz.c dm_gviz.h
	${CC} -c ${CPPFLAGS} ${CFLAGS} -o dm_gviz.o dm_gviz.c

dm_dom.o: dm_dom.c dm_dom.h dm_cfg.o dm_gviz.o
	${CC} -c ${CPPFLAGS} ${CFLAGS} -o dm_dom.o dm_dom.c

dm_ssa.o: dm_ssa.c dm_ssa.h dm_dom.o
	${CC} -c ${CPPFLAGS} ${CFLAGS} -o dm_ssa.o dm_ssa.c

dm_dwarf.o: dm_dwarf.c dm_dwarf.h
	${CC} -c ${CPPFLAGS} ${CFLAGS} -o dm_dwarf.o dm_dwarf.c

clean:
	rm -f *.o *.dot dismantle
