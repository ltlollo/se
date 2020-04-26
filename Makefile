VK_INCLUDE 		:= "${HOME}/opt/vulkan/1.2.135.0/x86_64/include"
VK_LIB 			:= "${HOME}/opt/vulkan/1.2.135.0/x86_64/lib"
VK_BIN 			:= "${HOME}/opt/vulkan/1.2.135.0/x86_64/bin"
LDFLAGS			+= -L $(VK_LIB) -lGL -lGLEW -lGLU -lSDL2 -lvulkan
CFLAGS			+= -I $(VK_INCLUDE) -std=c11  -Wall -Wextra -Wno-pointer-sign \
	-fPIC
DEBUG_CFLAGS	:= ${CFLAGS} -ggdb -O0 -pie -fno-omit-frame-pointer
RELEASE_CFLAGS	:= ${CFLAGS} -Ofast -pie -ftree-vectorize -march=native -s \
-DNDEBUG -funroll-all-loops -fprefetch-loop-arrays -minline-all-stringops
SRC				:= se.c lex.c diff.c input.c vk.c ui.c


se: tags $(SRC) se.h se.gen.h umap.gen.h util.c fio.c comp.c ilog.c \
	ext/unifont.o ext/vert.o ext/frag.o
	$(CC) -DLINK_FONT -D_GNU_SOURCE  $(DEBUG_CFLAGS) \
		se.c lex.c util.c fio.c comp.c ilog.c ./ext/unifont.o ./ext/vert.o \
		./ext/frag.o $(LDFLAGS) -o se

se.gen.h: $(SRC)
	$(SH) ./ext/gen_headers $(SRC) > se.gen.h

tags: $(SRC) se.gen.h umap.gen.h util.c fio.c comp.c ilog.c
	@ctags *.c *.h

release: $(SRC) se.gen.h umap.gen.h util.c fio.c comp.c ilog.c ext/unifont.o
	$(CC) -DNDEBUG -DLINK_FONT -D_GNU_SOURCE $(RELEASE_CFLAGS) \
		se.c lex.c util.c fio.c comp.c ilog.c ext/unifont.o ./ext/vert.o \
		./ext/frag.o $(LDFLAGS) -o se

asm/%.s: %.c
	mkdir -p asm
	$(CC) -fverbose-asm -DNDEBUG -DLINK_FONT -D_GNU_SOURCE $(RELEASE_CFLAGS) \
		$*.c -S -o $@

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
	ld -r -b binary -z noexecstack ext/unifont.cfp -o ext/unifont.o

clean:
	$(RM) se umap.gen.h se.gen.h ext/rfp ext/cfp ext/umap ext/unifont.cfp \
		ext/unifont.rfp ext/unifont.o asm/*.s ext/*.o ext/*.spv

perf:
	gprof2dot -f callgrind -o callgrind.dot callgrind.out
	gprof2dot -f perf -o perf.dot perf.data

ext/vert.spv: shader.vert
	$(VK_BIN)/glslangValidator -V shader.vert -o ext/vert.spv

ext/frag.spv: shader.frag
	$(VK_BIN)/glslangValidator -V shader.frag -o ext/frag.spv

ext/vert.o: ext/vert.spv
	ld -r -b binary -z noexecstack ext/vert.spv -o ext/vert.o

ext/frag.o: ext/frag.spv
	ld -r -b binary -z noexecstack ext/frag.spv -o ext/frag.o

run: se
	LD_LIBRARY_PATH=$(VK_LIB) ./se

.PHONY: clean
