TARGET		:= se
LDFLAGS		+= `pkg-config --libs glew freeglut` -lm
CFLAGS		:= -std=c11 -g -Wall -Wextra -O0 -pie
CFLAGS		+= `pkg-config --cflags glew freeglut`

all:
	@$(SH) ./gen_headers $(TARGET).c > $(TARGET).h
	@ctags $(TARGET).c
	$(CC) -D_GNU_SOURCE $(LDFLAGS) $(CFLAGS) $(TARGET).c -o $(TARGET)

clean:
	$(RM) $(TARGET)
