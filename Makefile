LDFLAGS			+= `pkg-config --libs glew freeglut` -lm
CFLAGS			:= -std=c11 `pkg-config --cflags glew freeglut` -Wall -Wextra \
	-Wno-pointer-sign
DEBUG_CFLAGS	:= ${CFLAGS} -ggdb -O0 -pie -fno-omit-frame-pointer
RELEASE_CFLAGS	:= ${CFLAGS} -Ofast -pie -ftree-vectorize -march=native -s \
-DNDEBUG -funroll-all-loops -fprefetch-loop-arrays -minline-all-stringops

all: umap font
	@$(SH) ./ext/gen_headers se.c > se.h
	@ctags *.c
	$(CC) -DLINK_FONT -D_GNU_SOURCE $(LDFLAGS) $(DEBUG_CFLAGS) \
		se.c lex.c util.c fio.c comp.c unifont.o -o se
	$(CC) -D_GNU_SOURCE $(LDFLAGS) $(DEBUG_CFLAGS) \
		rfp.c util.c fio.c -o rfp
	$(CC) -D_GNU_SOURCE $(LDFLAGS) $(DEBUG_CFLAGS) \
		cfp.c util.c fio.c comp.c -o cfp

release: umap font
	@$(SH) ./gen_headers se.c > se.h
	$(CC) -DLINK_FONT -D_GNU_SOURCE $(LDFLAGS) $(RELEASE_CFLAGS) \
		se.c lex.c util.c fio.c comp.c unifont.o -o se
	$(CC) -D_GNU_SOURCE $(LDFLAGS) $(RELEASE_CFLAGS) \
		rfp.c util.c fio.c -o rfp
	$(CC) -D_GNU_SOURCE $(LDFLAGS) $(RELEASE_CFLAGS) \
		cfp.c util.c fio.c comp.c -o cfp

umap:
	$(CC) -D_GNU_SOURCE $(LDFLAGS) $(RELEASE_CFLAGS) \
		umap.c util.c fio.c -o umap
	$(SH) ./umap > umap.h

font:
	ld -r -b binary unifont.cfp -o unifont.o

clean:
	$(RM) se rfp umap unifont.o

