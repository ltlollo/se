TARGET			:= se
LDFLAGS			+= `pkg-config --libs glew freeglut` -lm
CFLAGS			:= -std=c11 `pkg-config --cflags glew freeglut` -Wall -Wextra
DEBUG_CFLAGS	:= ${CFLAGS} -ggdb -Og -pie -fno-omit-frame-pointer
RELEASE_CFLAGS	:= ${CFLAGS} -Ofast -pie -ftree-vectorize -march=native -s \
-DNDEBUG -funroll-all-loops -fprefetch-loop-arrays -minline-all-stringops

all:
	@$(SH) ./gen_headers $(TARGET).c > $(TARGET).h
	@ctags $(TARGET).c
	$(CC) -D_GNU_SOURCE $(LDFLAGS) $(DEBUG_CFLAGS) $(TARGET).c -o $(TARGET)

clean:
	$(RM) $(TARGET)

release:
	@$(SH) ./gen_headers $(TARGET).c > $(TARGET).h
	$(CC) -D_GNU_SOURCE $(LDFLAGS) $(RELEASE_CFLAGS) $(TARGET).c -o $(TARGET)
