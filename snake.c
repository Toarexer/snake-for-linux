#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include "snake.h"

int speed_mod = 0;
long seed;
uint record_score;
char score_path[FILENAME_MAX];
char map[MAP_HEIGHT * MAP_WIDTH];
char direction = D_UP, next_direction = D_UP;
bool stop_game_thread = false;
bool pause_game_thread = false;
bool wait_for_input = true;

struct
{
    uint length;
    struct
    {
        uint x, y;
    } body[SNAKE_MAXLEN];
} snake;

struct
{
    uint x, y;
    uint cooldown;
    char present;
} fruit;

void draw_map()
{
    printf("\e[H");
    for (uint y = 0; y < MAP_HEIGHT; y++)
    {
        for (uint x = 0; x < MAP_WIDTH; x++)
            putchar((y == 0 || y == MAP_HEIGHT - 1 || x == 0 || x == MAP_WIDTH - 1) ? '#' : ' ');
        printf("\n\e[G");
    }

    if (fruit.present)
        printf("\e[%u;%uH\e[91m@\e[0m", fruit.y + 1, fruit.x + 1);
}

void set_snake()
{
    snake.length = SNAKE_STARTLEN;
    for (uint i = 0; i < snake.length; i++)
    {
        snake.body[i].x = MAP_WIDTH / 2;
        snake.body[i].y = MAP_HEIGHT / 2 - snake.length / 2 + i + 1;
    }
}

void draw_snake()
{
    printf("\e[%u;%uH\e[32mV\e[0m", snake.body[0].y + 1, snake.body[0].x + 1);
    for (uint i = 1; i < snake.length; i++)
        printf("\e[%u;%uH\e[32mO\e[0m\n", snake.body[i].y + 1, snake.body[i].x + 1);
}

void draw_score()
{
    if (snake.length - SNAKE_STARTLEN > record_score)
        record_score = snake.length - SNAKE_STARTLEN;
    printf("\e[%u;0H", MAP_HEIGHT + 1);
    for (int i = 0; i < MAP_WIDTH; i++)
        putchar(' ');
    printf("\e[%u;0HSCORE:  %u\e[%u;0HRECORD: %u\e[H", MAP_HEIGHT + 2, snake.length - SNAKE_STARTLEN, MAP_HEIGHT + 3, record_score);
}

void draw_help(bool visible)
{
    if (visible)
    {
        printf("\e[%u;%uH\e[33m   Controls:", 2, MAP_WIDTH + 1);
        printf("\e[%u;%uH     Use \e[0m[WASD]\e[33m or the \e[0marrow keys\e[33m to move", 3, MAP_WIDTH + 1);
        printf("\e[%u;%uH     Press \e[0m[P]\e[33m to pasue and unpause the game", 4, MAP_WIDTH + 1);
        printf("\e[%u;%uH     Press \e[0m[R]\e[33m to restart the game", 5, MAP_WIDTH + 1);
        printf("\e[%u;%uH     Press \e[0m[Q]\e[33m to quit from the game\e[0m", 6, MAP_WIDTH + 1);
    }
    else
    {
        printf("\e[%u;%uH\e[K", 2, MAP_WIDTH + 1);
        printf("\e[%u;%uH\e[K", 3, MAP_WIDTH + 1);
        printf("\e[%u;%uH\e[K", 4, MAP_WIDTH + 1);
        printf("\e[%u;%uH\e[K", 5, MAP_WIDTH + 1);
        printf("\e[%u;%uH\e[K", 6, MAP_WIDTH + 1);
    }
}

void move_snake()
{
    for (uint i = snake.length - 1; i > 0; i--)
        snake.body[i] = snake.body[i - 1];
    switch (direction = next_direction)
    {
    case D_UP:
        snake.body[0].y--;
        return;
    case D_DOWN:
        snake.body[0].y++;
        return;
    case D_RIGHT:
        snake.body[0].x++;
        return;
    case D_LEFT:
        snake.body[0].x--;
        return;
    }
}

bool body_collision(uint x, uint y, uint start)
{
    for (uint i = start; i < snake.length; i++)
        if (x == snake.body[i].x && y == snake.body[i].y)
            return true;
    return false;
}

// game logic and printing thread
void *game(void *args)
{
    wait_for_input = true;
    draw_map();
    draw_snake();
    draw_help(true);
    printf("\e[%u;%uH\e[33m\e[5mPRESS ANY KEY\e[%u;%uHTO START!\e[0m\n", 3, MAP_WIDTH / 2 - 5, 4, MAP_WIDTH / 2 - 3);
    while (wait_for_input && !stop_game_thread)
        usleep(1000);
    draw_help(false);

    while (!stop_game_thread)
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

        if (snake.body[0].x == 0 || snake.body[0].y == 0 || snake.body[0].x == MAP_WIDTH - 1 || snake.body[0].y == MAP_HEIGHT - 1)
        {
            draw_help(true);
            printf("\e[%u;%uH\e[31m\e[5m#\e[0m\n", snake.body[0].y + 1, snake.body[0].x + 1);
            break;
        }

        if (body_collision(snake.body[0].x, snake.body[0].y, 1))
        {
            draw_help(true);
            printf("\e[%u;%uH\e[31m\e[5mO\e[0m\n", snake.body[0].y + 1, snake.body[0].x + 1);
            break;
        }

        if (!fruit.present)
            fruit.cooldown--;
        if (fruit.cooldown == 0)
        {
            time_t t;
            do
            {
                srand(seed);
                fruit.x = rand() % (MAP_WIDTH - 2) + 1;
                srand(seed = fruit.x);
                fruit.y = rand() % (MAP_HEIGHT - 2) + 1;
                seed = fruit.y;
            } while (body_collision(fruit.x, fruit.y, 0));
            fruit.cooldown = FRUIT_MAXC;
            fruit.present = true;
        }

        do
            usleep((WAIT_TIME - speed_mod) * 1000);
        while (pause_game_thread);
    }

    stop_game_thread = true;
    return NULL;
}

// input and setup thread
int main(int argc, char *argv[])
{
    if (!isatty(STDIN_FILENO))
    {
        perror("error: stdin is not a tty!");
        return -1;
    }

    if (argc > 1)
        speed_mod = atoi(argv[1]); // +1 speed_mod -> -1 milisec delay
    if (WAIT_TIME - speed_mod <= 0)
        speed_mod = WAIT_TIME - 1;

    // adjust score file path to startup path
    char *home = getenv("HOME");
    if (home != NULL)
    {
        strncpy(score_path, home, sizeof(score_path));
        strncat(score_path, "/.snake_record_score", sizeof(score_path) - strlen(score_path));

        // read score from record file, create it if it does not exist
        FILE *record_file = fopen(score_path, access(score_path, 0) ? "w+" : "r");
        fscanf(record_file, "%u", &record_score);
        fclose(record_file);
    }

    // set terminal to raw mode
    struct termios tty_og, tty_raw;
    tcgetattr(0, &tty_og);
    cfmakeraw(&tty_raw);
    tcsetattr(0, TCSANOW, &tty_raw);

    printf("\e[?1049h\e[?25l"); // enable alternative screen buffer and hide cursor

    fruit.cooldown = FRUIT_MAXC;
    seed = time(NULL);
    set_snake();

    pthread_t game_thread;
    pthread_create(&game_thread, NULL, game, NULL);
    draw_score();

    while (true)
    {
        char c = getchar();
        wait_for_input = false;
        if (c == 'q')
        {
            stop_game_thread = true;
            pause_game_thread = wait_for_input = false;
            pthread_join(game_thread, NULL);
            break;
        }
        else if (c == 'p' && !stop_game_thread)
        {
            if (pause_game_thread = !pause_game_thread)
                printf("\e[%u;%uH\e[33m\e[5mGAME PAUSED\e[0m", MAP_HEIGHT / 2, MAP_WIDTH / 2 - 4);
        }
        else if (c == 'r')
        {
            stop_game_thread = true;
            pthread_join(game_thread, NULL);
            stop_game_thread = false;

            direction = next_direction = D_UP;
            fruit.present = false;
            fruit.cooldown = FRUIT_MAXC;
            set_snake();

            pthread_create(&game_thread, NULL, game, NULL);
        }
        else if (c == 'w' && direction != D_DOWN || c == 'A' && direction != D_DOWN) // UP ARROW KEY
            next_direction = D_UP;
        else if (c == 's' && direction != D_UP || c == 'B' && direction != D_UP) // DOWN ARROW KEY
            next_direction = D_DOWN;
        else if (c == 'd' && direction != D_LEFT || c == 'C' && direction != D_LEFT) // RIGTH ARROW KEY
            next_direction = D_RIGHT;
        else if (c == 'a' && direction != D_RIGHT || c == 'D' && direction != D_RIGHT) // LEFT ARROW KEY
            next_direction = D_LEFT;
        else if (c == '+')
        {
            snake.length++;
            draw_score();
        }
        else if (c == '-')
        {
            if (snake.length > 4)
                snake.length--;
            draw_score();
        }
    }

    if (home != NULL)
    {
        FILE *record_file = fopen(score_path, "w");
        fprintf(record_file, "%u", record_score);
        fclose(record_file);
    }

    tcsetattr(0, TCSANOW, &tty_og);
    printf("\e[H\e[J\e[?1049l\e[?25h"); // go to (1;1), clear display, disnable alternative screen buffer and show cursor
    return snake.length - SNAKE_STARTLEN;
}
