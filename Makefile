LDFLAGS			+= -lGL -lGLEW -lGLU -lSDL2
CFLAGS			:= -std=c11  -Wall -Wextra -Wno-pointer-sign -fPIC
DEBUG_CFLAGS	:= ${CFLAGS} -ggdb -O0 -pie -fno-omit-frame-pointer
RELEASE_CFLAGS	:= ${CFLAGS} -Ofast -pie -ftree-vectorize -march=native -s \
-DNDEBUG -funroll-all-loops -fprefetch-loop-arrays -minline-all-stringops
SRC				:= se.c lex.c diff.c input.c

se: tags se.gen.h umap.gen.h $(SRC) util.c fio.c comp.c ilog.c ext/unifont.o
	$(CC) -DLINK_FONT -D_GNU_SOURCE  $(DEBUG_CFLAGS) \
		se.c lex.c util.c fio.c comp.c ilog.c ./ext/unifont.o $(LDFLAGS) -o se

se.gen.h: $(SRC)
	@$(SH) ./ext/gen_headers $(SRC) > se.gen.h

tags: *.c *.h
	@ctags *.c *.h

release: se.gen.h umap.gen.h $(SRC) util.c fio.c comp.c ilog.c ext/unifont.o
	$(CC) -DNDEBUG -DLINK_FONT -D_GNU_SOURCE $(RELEASE_CFLAGS) \
		se.c lex.c util.c fio.c comp.c ilog.c ext/unifont.o $(LDFLAGS) -o se

asm/%.s: %.c
	mkdir -p asm
	$(CC) -fverbose-asm -DNDEBUG -DLINK_FONT -D_GNU_SOURCE $(RELEASE_CFLAGS) $*.c -S -o $@

ext/umap: umap.c util.c fio.c
	$(CC) -D_GNU_SOURCE  $(RELEASE_CFLAGS) \
		umap.c util.c fio.c $(LDFLAGS) -o ext/umap
umap.gen.h: ext/umap ext/unifont.rfp
	@$(SH) ./ext/umap ext/unifont.rfp > umap.gen.h

ext/rfp: rfp.c util.c fio.c
	$(CC) -D_GNU_SOURCE  $(RELEASE_CFLAGS) \
		rfp.c util.c fio.c $(LDFLAGS) -o ext/rfp
ext/unifont.rfp: ext/rfp ext/unifont-10.0.07.bmp
	$(SH) ./ext/rfp ext/unifont-10.0.07.bmp ext/unifont.rfp

ext/cfp: cfp.c util.c fio.c comp.c
	$(CC) -D_GNU_SOURCE  $(RELEASE_CFLAGS) \
		cfp.c util.c fio.c comp.c $(LDFLAGS) -o ext/cfp
ext/unifont.cfp: ext/cfp ext/unifont.rfp
	$(SH) ./ext/cfp ext/unifont.rfp ext/unifont.cfp

ext/unifont.o: ext/unifont.cfp
	ld -r -b binary ext/unifont.cfp -o ext/unifont.o

clean:
	$(RM) se ext/rfp ext/cfp ext/umap ext/unifont.cfp unifont.rfp ext/unifont.o

perf:
	#gprof2dot --format=callgrind --output=out.dot callgrind.out.26855
	#gprof2dot --format=perf --output=out.dot perf.data

.PHONY: clean
