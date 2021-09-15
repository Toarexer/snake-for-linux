#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <pthread.h>
#include "snake.h"

uint record_score;
char map[MAP_HEIGHT * MAP_WIDTH];
char direction = D_UP;
bool input_thread_running = true;

struct snake_s
{
    uint length;
    struct body_s
    {
        uint x, y;
    } body[SNAKE_MAXLEN];
} snake;

struct fruit_s
{
    uint x, y;
    uint cooldown;
    char present;
} fruit;

void create_map()
{
    for (uint y = 0; y < MAP_HEIGHT; y++)
        for (uint x = 0; x < MAP_WIDTH; x++)
            map[mappos(x, y)] = (y == 0 || y == MAP_HEIGHT - 1 || x == 0 || x == MAP_WIDTH - 1) ? '#' : ' ';
}

void draw_map()
{
    printf("\e[H");
    for (uint y = 0; y < MAP_HEIGHT; y++)
    {
        for (uint x = 0; x < MAP_WIDTH; x++)
            putchar(map[mappos(x, y)]);
        printf("\n\e[G");
    }
    if (fruit.present)
        printf("\e[%u;%uH\e[91m@\e[0m\n", fruit.y + 1, fruit.x + 1);
}

void draw_snake()
{
    printf("\e[%u;%uH\e[32mV\e[0m\n", snake.body[0].y + 1, snake.body[0].x + 1);
    for (uint i = 1; i < snake.length; i++)
        printf("\e[%u;%uH\e[32mO\e[0m\n", snake.body[i].y + 1, snake.body[i].x + 1);
}

void draw_score()
{
    if (snake.length - SNAKE_STARTLEN > record_score)
        record_score = snake.length - SNAKE_STARTLEN;
    printf("\e[%u;0HSCORE:  %u\e[%u;0HRECORD: %u\e[H\n", MAP_HEIGHT + 2, snake.length - SNAKE_STARTLEN, MAP_HEIGHT + 3, record_score);
}

void move_snake()
{
    for (uint i = snake.length - 1; i > 0; i--)
        snake.body[i] = snake.body[i - 1];
    switch (direction)
    {
    case D_UP:
        snake.body[0].y--;
        return;
    case D_DOWN:
        snake.body[0].y++;
        return;
    case D_LEFT:
        snake.body[0].x--;
        return;
    case D_RIGHT:
        snake.body[0].x++;
        return;
    }
}

bool body_collision()
{
    for (uint i = 1; i < snake.length; i++)
        if (snake.body[0].x == snake.body[i].x && snake.body[0].y == snake.body[i].y)
            return true;
    return false;
}

void *getinput(void *args)
{
    while (1)
    {
        char c = getchar();
        if (c == 'q')
        {
            input_thread_running = 0;
            return NULL;
        }
        else if (c == 'w' && direction != D_DOWN)
            direction = D_UP;
        else if (c == 'a' && direction != D_RIGHT)
            direction = D_LEFT;
        else if (c == 's' && direction != D_UP)
            direction = D_DOWN;
        else if (c == 'd' && direction != D_LEFT)
            direction = D_RIGHT;
        else if (c == '+')
        {
            snake.length++;
            draw_score();
        }
        else if (c == '-')
        {
            snake.length--;
            draw_score();
        }
    }
}

int main()
{
    if (!isatty(STDIN_FILENO))
    {
        perror("error: stdin is not a tty!");
        return -1;
    }

    FILE *record_file = fopen(".snake_record_score", "r");
    fscanf(record_file, "%u", &record_score);
    fclose(record_file);

    struct termios tty_og, tty_raw;
    tcgetattr(0, &tty_og);
    cfmakeraw(&tty_raw);
    tcsetattr(0, TCSANOW, &tty_raw);

    printf("\e[?1049h\e[?25l"); // enable alternative screen buffer and hide cursor

    create_map();
    snake.length = SNAKE_STARTLEN;
    for (uint i = 0; i < snake.length; i++)
    {
        snake.body[i].x = MAP_WIDTH / 2;
        snake.body[i].y = MAP_HEIGHT / 2 - snake.length / 2 + i + 1;
    }

    fruit.cooldown = FRUIT_MAXC;

    pthread_t input_thread;
    pthread_create(&input_thread, NULL, getinput, NULL);
    draw_score();

    while (input_thread_running)
    {
        draw_map();
        move_snake();
        draw_snake();

        if (fruit.present && snake.body[0].x == fruit.x && snake.body[0].y == fruit.y)
        {
            snake.body[snake.length] = snake.body[snake.length++ - 1];
            fruit.present = false;
            draw_score();
        }

        if (map[mappos(snake.body[0].x, snake.body[0].y)] == '#')
        {
            printf("\e[%u;%uH\e[31m\e[5m#\e[0m\n", snake.body[0].y + 1, snake.body[0].x + 1);
            break;
        }

        if (body_collision())
        {
            printf("\e[%u;%uH\e[31m\e[5mO\e[0m\n", snake.body[0].y + 1, snake.body[0].x + 1);
            break;
        }

        if (!fruit.present)
            fruit.cooldown--;
        if (fruit.cooldown == 0)
        {
            time_t t;
            srand(time(&t));
            fruit.x = rand() % (MAP_WIDTH - 2) + 1;
            srand(fruit.x);
            fruit.y = rand() % (MAP_HEIGHT - 2) + 1;
            fruit.cooldown = FRUIT_MAXC;
            fruit.present = true;
        }

        msleep(WAIT_TIME);
    }

    pthread_join(input_thread, NULL);

    record_file = fopen(".snake_record_score", "w");
    fprintf(record_file, "%u", record_score);
    fclose(record_file);

    tcsetattr(0, TCSANOW, &tty_og);
    printf("\e[H\e[J\e[?1049l\e[?25h"); // go to (1;1), clear display, disnable alternative screen buffer and show cursor
    return 0;
}