LDFLAGS			+= `pkg-config --libs glew freeglut` -lm
CFLAGS			:= -std=c11 `pkg-config --cflags glew freeglut` -Wall -Wextra \
	-Wno-pointer-sign
DEBUG_CFLAGS	:= ${CFLAGS} -ggdb -O0 -pie -fno-omit-frame-pointer
RELEASE_CFLAGS	:= ${CFLAGS} -Ofast -pie -ftree-vectorize -march=native -s \
-DNDEBUG -funroll-all-loops -fprefetch-loop-arrays -minline-all-stringops


all: umap
	@$(SH) ./gen_headers se.c > se.h
	@ctags se.c util.c fio.c
	$(CC) -D_GNU_SOURCE $(LDFLAGS) $(DEBUG_CFLAGS) se.c util.c fio.c -o se
	$(CC) -D_GNU_SOURCE $(LDFLAGS) $(DEBUG_CFLAGS) rfp.c util.c fio.c -o rfp

clean:
	$(RM) se rfp umap

release: umap
	@$(SH) ./gen_headers se.c > se.h
	$(CC) -D_GNU_SOURCE $(LDFLAGS) $(RELEASE_CFLAGS) se.c util.c fio.c -o se
	$(CC) -D_GNU_SOURCE $(LDFLAGS) $(RELEASE_CFLAGS) rfp.c util.c fio.c -o rfp

umap:
	$(CC) -D_GNU_SOURCE $(LDFLAGS) $(RELEASE_CFLAGS) umap.c util.c fio.c -o umap
	$(SH) ./umap > umap.h
