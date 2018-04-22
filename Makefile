LDFLAGS			+= -lGL -lGLEW -lGLU -lglut
CFLAGS			:= -std=c11  -Wall -Wextra -Wno-pointer-sign
DEBUG_CFLAGS	:= ${CFLAGS} -ggdb -O0 -pie -fno-omit-frame-pointer
RELEASE_CFLAGS	:= ${CFLAGS} -Ofast -pie -ftree-vectorize -march=native -s \
-DNDEBUG -funroll-all-loops -fprefetch-loop-arrays -minline-all-stringops

all: umap font
	@$(SH) ./ext/gen_headers se.c > se.h
	@ctags *.c
	$(CC) -DLINK_FONT -D_GNU_SOURCE  $(DEBUG_CFLAGS) \
		se.c lex.c util.c fio.c comp.c ./ext/unifont.o $(LDFLAGS) -o se

release: umap font
	@$(SH) ./ext/gen_headers se.c > se.h
	$(CC) -DLINK_FONT -D_GNU_SOURCE  $(RELEASE_CFLAGS) \
		se.c lex.c util.c fio.c comp.c ext/unifont.o $(LDFLAGS) -o se

umap:
	$(CC) -D_GNU_SOURCE  $(RELEASE_CFLAGS) \
		umap.c util.c fio.c $(LDFLAGS) -o ext/umap
	$(SH) ./ext/umap ext/unifont.rfp > umap.h

font:
	$(CC) -D_GNU_SOURCE  $(RELEASE_CFLAGS) \
		rfp.c util.c fio.c $(LDFLAGS) -o ext/rfp
	$(CC) -D_GNU_SOURCE  $(RELEASE_CFLAGS) \
		cfp.c util.c fio.c comp.c $(LDFLAGS) -o ext/cfp
	$(SH) ./ext/rfp ext/unifont-10.0.07.bmp ext/unifont.rfp
	$(SH) ./ext/cfp ext/unifont.rfp ext/unifont.cfp
	ld -r -b binary ext/unifont.cfp -o ext/unifont.o

clean:
	$(RM) se ext/rfp ext/cfp ext/umap ext/unifont.o

