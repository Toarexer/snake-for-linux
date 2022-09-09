snake: snake.c snake.h
	gcc snake.c -o snake -lpthread
snakedbg: snake.c snake.h
	gcc snake.c -o snakedbg -lpthread -g
all: snake snakedbg
