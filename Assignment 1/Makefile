CFLAGS=-std=gnu99 -g -IZDK -LZDK  -lzdk -lncurses -lm

NAME=a1_n10133810

all:
	reset
	gcc game.c -o $(NAME) $(CFLAGS)

clean:
	@rm -f $(NAME)
	@rm -f vgcore.*

play: clean all
	./$(NAME)

test: clean all
	./$(NAME) ./room_files/room0{0..9}.txt

debug: clean all
	valgrind ./$(NAME) ./room_files/room0{0..9}.txt



