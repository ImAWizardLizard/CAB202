#include <cab202_graphics.h>
#include <cab202_timers.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DELAY (10) /* Millisecond delay between game updates */
#define MAX_CHEESE (50)
#define MAX_TRAPS (50)
#define MAX_FIREWORKS (10)

typedef struct Sprite
{
    bool draw;
    double x, y, dx, dy;
    char image;
} Sprite;

typedef enum Collisions
{
    WALL_LEFT,
    WALL_RIGHT,
    WALL_UP,
    WALL_DOWN,
    NO_COLLISION
} Collision;

// Game variables
bool game_over = false; /* Set this to true when game is over */
bool pause = false;     /* Set this to true when game is over */
int width, height;      // Width and height of screen
int lives, score;       // Lives and score variables for display screen
int number_of_cheese = 0;
int collected_cheese = 0;
int number_of_mousetraps = 0;
int number_of_fireworks = 0;
int number_of_walls = 0;
int levels[20][2][1000];
int current_level = 0;
int number_of_levels;
bool check_time = true;

// Time related variables
timer_id timer;
int time_seconds, time_minutes;
double current_time, current_time_diff, saved_time_diff;

// Sprite variables
Sprite **active_player;    // Current active player, either 'J' or 'T' , default is 'J'
Sprite **active_seeker;    // Current nonactive player, either 'J' or 'T' , default is 'T'
char current_player = 'J'; // Character used to identify the current player playing

Sprite *jerry;
char jerry_image = 'J';

Sprite *tom;
char tom_image = 'T';

Sprite cheese[MAX_CHEESE];
char cheese_image = '#';

Sprite traps[MAX_TRAPS];
char traps_image = '%';

Sprite fireworks[MAX_FIREWORKS];
char fireworks_image = 'F';

Sprite door;
char door_image = 'X';

// Functions which control certain stages/state of the game
void setup(); // Sets up the game including global variables
void loop();  // Processes all of the games functions
void read_files();
void update_time();     // Will return a formatted string containing the time elapsed in the format of mm:ss
void pause_game();      // Pauses the game
void reset_game();      // Resets the game
void reset_level();     // Resets the game
void display_screen();  // Displays current game information
void gameover_screen(); // Displays current game information
void draw_walls();      // Draw the walls from given files
void draw_sprite(Sprite *player);
void init_sprites();        // Initalize tom and jerry values for game
void switch_player();       // Switches the current player from jerry to tom, vice versa
void game_input(char code); // Sprite input for game
void player_randomly_turn();
void player_movement();
Collision get_current_wall_collision();
void chase();
bool has_collided();
void caught_collision();
void traps_collision();
void place_trap();
void cheese_collision();
void spawn_cheese();
void place_cheese();
void spawn_door();
void next_level();
void door_collision();

/// COLLISION FUNCTIONS ///

bool has_collided(Sprite *s1, Sprite *s2)
{
    return s1->x == s2->x && s1->y == s2->y;
}

Collision get_current_wall_collision(Sprite *player)
{
    int x = round(player->x);
    int y = round(player->y);

    if (x < 1 || scrape_char(x - 1, y) == '*')
    {
        return WALL_LEFT;
    }

    if (x >= width - 1 || scrape_char(x + 1, y) == '*')
    {
        return WALL_RIGHT;
    }

    if (y <= 4 || scrape_char(x, y - 1) == '*')
    {
        return WALL_UP;
    }

    if (y >= height - 1 || scrape_char(x, y + 1) == '*')
    {
        return WALL_DOWN;
    }

    return NO_COLLISION;
}
void door_collision()
{
    if (collected_cheese >= 5)
    {
        door.draw = true;
        if (has_collided(jerry, &door))
        {
            next_level();
        }
    }
}

void cheese_collision()
{
    for (size_t i = 0; i < MAX_CHEESE; i++)
    {
        if (has_collided(jerry, &cheese[i]))
        {
            cheese[i].draw = false;
            cheese[i].x = -1;
            cheese[i].y = -1;
            score++;
            collected_cheese++;
            number_of_cheese--;
        }
    }
}

void traps_collision()
{

    for (size_t i = 0; i < MAX_TRAPS; i++)
    {
        if (has_collided(jerry, &traps[i]))
        {
            traps[i].draw = false;
            traps[i].x = -1;
            traps[i].y = -1;
            lives--;
        }
    }
}

void caught_collision()
{
    if (has_collided(jerry, tom))
    {
        lives--;
        reset_level();
    }
}

void firework_collision()
{
    Collision collision;
    for (size_t i = 0; i < MAX_FIREWORKS; i++)
    {
        if (fireworks[i].draw)
        {
            collision = get_current_wall_collision(&fireworks[i]);
            if (collision != NO_COLLISION)
            {
                fireworks[i].draw = false;
                fireworks[i].x = 5;
                fireworks[i].y = 5;
                fireworks[i].dx = 0;
                fireworks[i].dy = 0;
                number_of_fireworks--;
            }

            if (has_collided(&fireworks[i], tom))
            {
                tom->x = (int)levels[current_level][0][1] - 1;
                tom->y = (int)levels[current_level][1][1] - 1;

                fireworks[i].draw = false;
                fireworks[i].dx = 0;
                fireworks[i].dy = 0;
            }
        }
    }
}

void firework_seek()
{
    int d, t1, t2;
    if (number_of_fireworks > 0)
    {
        for (size_t i = 0; i < MAX_FIREWORKS; i++)
        {
            t1 = tom->x - fireworks[i].x;
            t2 = tom->y - fireworks[i].y;
            d = sqrt(t1 * t1 + t2 * t2);
            fireworks[i].dx = t1 * 0.5 / d;
            fireworks[i].dy = t2 * 0.5 / d;
            fireworks[i].x += fireworks[i].dx;
            fireworks[i].y += fireworks[i].dy;
        }
    }
}

void chase()
{
    int d, t1, t2;
    t1 = jerry->x - tom->x;
    t2 = jerry->y - tom->y;
    d = sqrt(t1 * t1 + t2 * t2);

    if (d <= 10)
    {
        tom->dx = t1 * 0.05 / d;
        tom->dy = t2 * 0.05 / d;
    }
}

void evade()
{
    int d, t1, t2;
    t1 = tom->x - jerry->x;
    t2 = tom->y - jerry->y;
    d = sqrt(t1 * t1 + t2 * t2);

    if (d <= 10)
    {
        jerry->dx = -t1 * 0.1 / d;
        jerry->dy = -t2 * 0.1 / d;
    }
}

/// COLLISION FUNCTIONS ///

/// SPAWN FUNCTIONS ///
void spawn_cheese()
{
    srand(time(NULL));
    int x, y;
    double fractpart, intpart;
    fractpart = modf(current_time_diff, &intpart);
    if (number_of_cheese <= 5)
    {
        if (time_seconds % 2 == 0 && time_seconds != 0 && fractpart <= 0.01) // If it is at a 2 second interval
        {
            for (size_t i = 0; i < MAX_CHEESE; i++)
            {
                // Fith one isnt drawing because it isnt being reached
                if (cheese[i].draw == false)
                {
                    x = rand() % width + 1;
                    y = rand() % height + 4;

                    while (!(scrape_char(x, y) == ' '))
                    {
                        x = rand() % width + 1;
                        y = rand() % height + 4;
                    }
                    cheese[i].x = x;
                    cheese[i].y = y;
                    cheese[i].draw = true;
                    number_of_cheese++;
                }
                else
                {
                    continue;
                }
                break;
            }
        }
    }
}

void place_cheese()
{
    for (size_t i = 0; i < MAX_CHEESE; i++)
    {
        if (cheese[i].draw == false)
        {
            cheese[i].x = round(tom->x);
            cheese[i].y = round(tom->y);
            cheese[i].draw = true;
            number_of_cheese++;
        }
        else
        {
            continue;
        }
        break;
    }
}

void spawn_moustraps()
{
    double fractpart, intpart;
    fractpart = modf(current_time_diff, &intpart);
    if (number_of_mousetraps <= 5)
    {
        if (time_seconds % 3 == 0 && time_seconds != 0 && fractpart <= 0.01) // If it is at a 3 second interval
        {
            for (size_t i = 0; i < MAX_TRAPS; i++)
            {
                // Fith one isnt drawing because it isnt being reached
                if (traps[i].draw == false)
                {
                    traps[i].x = round(tom->x);
                    traps[i].y = round(tom->y);
                    traps[i].draw = true;
                    number_of_mousetraps++;
                }
                else
                {
                    continue;
                }
                break;
            }
        }
    }
}

void place_trap()
{
    for (size_t i = 0; i < MAX_TRAPS; i++)
    {
        if (traps[i].draw == false)
        {
            traps[i].x = round(tom->x);
            traps[i].y = round(tom->y);
            traps[i].draw = true;
            number_of_mousetraps++;
        }
        else
        {
            continue;
        }
        break;
    }
}

void spawn_firework()
{
    for (size_t i = 0; i < MAX_FIREWORKS; i++)
    {
        // Fith one isnt drawing because it isnt being reached
        if (fireworks[i].draw == false)
        {
            fireworks[i].x = jerry->x;
            fireworks[i].y = jerry->y;
            fireworks[i].draw = true;
            number_of_fireworks++;
        }
        else
        {
            continue;
        }
        break;
    }
}

void spawn_door()
{
    srand(time(NULL));
    int x, y;
    x = rand() % width + 1;
    y = rand() % height + 4;

    while (!(scrape_char(x, y) == ' '))
    {
        x = rand() % width + 1;
        y = rand() % height + 4;
    }

    door.x = x;
    door.y = y;
}
/// SPAWN FUNCTIONS ///

/// DRAW FUNCTIONS ///
void draw_sprite(Sprite *sprite)
{
    if (sprite->draw)
    {
        draw_char(sprite->x, sprite->y, sprite->image);
    }
}

void draw_cheese()
{
    for (size_t i = 0; i < MAX_CHEESE; i++)
    {
        draw_sprite(&cheese[i]);
    }
}

void draw_traps()
{
    for (size_t i = 0; i < MAX_TRAPS; i++)
    {
        draw_sprite(&traps[i]);
    }
}

void draw_fireworks()
{
    for (size_t i = 0; i < MAX_FIREWORKS; i++)
    {
        draw_sprite(&fireworks[i]);
    }
}

void draw_walls()
{
    int number_of_walls = (sizeof(levels[current_level][0]) / sizeof(int)) - 2;
    for (size_t i = 2; i <= number_of_walls; i += 2)
    {
        draw_line(levels[current_level][0][i], levels[current_level][1][i], levels[current_level][0][i + 1], levels[current_level][1][i + 1], '*');
    }
}
/// DRAW FUNCTIONS ///

void next_level()
{

    current_level++;

    jerry->x = (int)levels[current_level][0][0];
    jerry->y = (int)levels[current_level][1][0];

    tom->x = (int)levels[current_level][0][1] - 1;
    tom->y = (int)levels[current_level][1][1] - 1;

    door.draw = false;
    number_of_cheese = 0;
    number_of_fireworks = 0;
    number_of_mousetraps = 0;
    collected_cheese = 0;

    for (size_t i = 0; i < MAX_CHEESE; i++)
    {
        cheese[i].draw = false;
    }

    for (size_t i = 0; i < MAX_TRAPS; i++)
    {
        traps[i].draw = false;
    }

    spawn_cheese();
    spawn_moustraps();
    spawn_door();
}

/// Tom&Jerry/Game interaction functions ///
void switch_player()
{
    if (current_player == 'J')
    {
        current_player = 'T';
        active_player = &tom;
        active_seeker = &jerry;
    }
    else
    {
        current_player = 'J';
        active_player = &jerry;
        active_seeker = &tom;
    }
}

void player_randomly_turn(int from, int to)
{
    srand(time(NULL));

    Sprite *player = *active_seeker;
    double radians;

    radians = (rand() % to + from) * M_PI / 180;
    double s = sin(radians);
    double c = cos(radians);
    double dx = c * player->dx + s * player->dy;
    double dy = -s * player->dx + c * player->dy;
    player->dx = dx;
    player->dy = dy;
}

void automatic_movement()
{
    Sprite *player = *active_seeker;
    player->x += player->dx;
    player->y += player->dy;

    Collision collision = get_current_wall_collision(player);
    if (collision == WALL_LEFT)
    {
        player_randomly_turn(-90, 90);
    }
    else if (collision == WALL_RIGHT)
    {
        player_randomly_turn(90, 270);
    }
    else if (collision == WALL_UP)
    {
        player_randomly_turn(180, 360);
    }
    else if (collision == WALL_DOWN)
    {
        player_randomly_turn(0, 180);
    }
}

void game_input(char code)
{
    Sprite *player = *active_player;
    int x = round(player->x);
    int y = round(player->y);
    Collision collision = get_current_wall_collision(player);
    switch (code)
    {
    case 'a':
        if (collision != WALL_LEFT)
            player->x -= 1;
        break;
    case 'd':
        if (collision != WALL_RIGHT)
            player->x += 1;
        break;
    case 'w':
        if (collision != WALL_UP)
            player->y -= 1; // Move jerry up
        break;
    case 's':
        if (collision != WALL_DOWN)
            player->y += 1; // Move jerry up
        break;
    case 'f':
        if (current_player == 'J')
            spawn_firework();
        break;
    case 'c':
        if (current_player == 'T')
            place_cheese();
        break;
    case 'm':
        if (current_player == 'T')
            place_trap();
        break;
    case 'p':
        pause_game();
        break;
    case 'z':
        switch_player();
        break;
    case 'l':
        next_level();
        break;
    }
}
/// Tom&Jerry/Game interaction functions ///

/// Core functions ///
void display_screen()
{
    // Draw the display screen box
    draw_line(0, 3, width, 3, '~');

    draw_formatted(0.05 * width, 0, "Student #: n10133810");
    draw_formatted(0.2 * width, 0, "Score: %d", score);
    draw_formatted(0.3 * width, 0, "Lives: %d", lives);
    draw_formatted(0.4 * width, 0, "Active Sprite: %c", current_player);
    draw_formatted(0.5 * width, 0, "Time: %02d:%02d", time_minutes, time_seconds);

    draw_formatted(0.05 * width, 2, "Cheese: %d", number_of_cheese);
    draw_formatted(0.2 * width, 2, "Traps: %d", number_of_mousetraps);
    draw_formatted(0.3 * width, 2, "Fireworks: %d", number_of_fireworks);
    draw_formatted(0.4 * width, 2, "Level: %d", current_level);
}

void gameover_screen()
{
    if (lives <= 0 || current_level > number_of_levels || game_over)
    {
        clear_screen();

        draw_string(0.4 * width, 0.38 * height, "Press (r) to restart and (q) to quit");

        show_screen();

        char input = wait_char();

        if (input == 'r')
        {
            reset_game();
        }
        else if (input == 'q')
        {
            exit(0);
        }
    }
}

void init_sprites()
{
    jerry = malloc(sizeof(Sprite));
    tom = malloc(sizeof(Sprite));

    jerry->x = (int)levels[current_level][0][0];
    jerry->y = (int)levels[current_level][1][0];
    jerry->dx = 0.1;
    jerry->dy = 0.1;
    jerry->image = jerry_image;
    jerry->draw = true;

    tom->x = (int)levels[current_level][0][1];
    tom->y = (int)levels[current_level][1][1];
    tom->dx = -0.1;
    tom->dy = -0.1;
    tom->image = tom_image;
    tom->draw = true;

    for (size_t i = 0; i < MAX_CHEESE; i++)
    {
        cheese[i].image = cheese_image;
        cheese[i].draw = false;
    }

    for (size_t i = 0; i < MAX_TRAPS; i++)
    {
        traps[i].image = traps_image;
        traps[i].draw = false;
    }

    for (size_t i = 0; i < MAX_FIREWORKS; i++)
    {
        fireworks[i].image = fireworks_image;
        fireworks[i].draw = false;
    }

    door.image = door_image;
    door.draw = false;
}

void reset_game()
{
    game_over = false;
    lives = 5;
    score = 0;
    active_player = &jerry;
    active_seeker = &tom;
    current_level = 0;
    next_level();
}

void reset_level()
{
    jerry->x = (int)levels[current_level][0][0];
    jerry->y = (int)levels[current_level][1][0];

    tom->x = (int)levels[current_level][0][1] - 1;
    tom->y = (int)levels[current_level][1][1] - 1;
}

void update_time()
{
    current_time = get_current_time();
    current_time_diff = current_time - timer->reset_time;

    time_seconds = (int)current_time_diff % 60;
    time_minutes = (int)current_time_diff / 60;
}

void pause_game()
{
    if (pause == false)
    {
        pause = true;
        saved_time_diff = current_time_diff;
    }
    else
    {
        pause = false;
        timer->reset_time = get_current_time() - saved_time_diff;
    }
}

void parse_instructions(int level, FILE *stream)
{
    int i = 2;
    while (!feof(stream))
    {
        //  Declare a variable called command of type char.
        char command;

        //  Declare four variables of type int.
        float a, b, c, d;
        //  Use a single call to fscanf to (attempt to) read a char and four int
        //  values into command, and your integer variables. Capture the value
        //  returned by fscanf for later use.
        int captured = fscanf(stream, "%c %f %f %f %f", &command, &a, &b, &c, &d);

        if (captured == 3)
        {
            a = round(a * width);
            b = round(b * height);
            if (command == 'J')
            {
                levels[level][0][0] = (int)a;
                levels[level][1][0] = (int)b + 4.0;
            }
            else if (command == 'T')
            {
                levels[level][0][1] = (int)a - 1;
                levels[level][1][1] = (int)b - 1;
            }
        }
        else if (captured == 5)
        {
            if (command == 'W')
            {
                a = round(a * width);
                b = round(b * height);
                c = round(c * width);
                d = round(d * height);

                levels[level][0][i] = (int)a;
                levels[level][1][i] = (int)b + 4.0;
                i++;
                levels[level][0][i] = (int)c;
                levels[level][1][i] = (int)d + 4.0;
                i++;
            }
        }
    }
}

void read_files(int level, char *file)
{
    FILE *stream = fopen(file, "r");
    if (stream != NULL)
    {
        parse_instructions(level, stream);
        fclose(stream);
    }
    else
    {
        printf("Could not open file");
    }
}
/// Core functions ///

void setup(int argc, char *argv[])
{
    // Initalize global variables
    width = screen_width();
    height = screen_height();

    lives = 5;
    score = 0;
    active_player = &jerry;
    active_seeker = &tom;
    number_of_levels = argc - 1;

    // Initalize the timer
    timer = create_timer(1);

    // Read all of the instructions from the room files
    for (size_t i = 1; i < argc; i++)
    {
        read_files(i, argv[i]); // Read the room instructions
    }

    init_sprites(); // Initalize the players

    next_level(); // Start the first level

    display_screen(); // Draw display screen
}
void loop()
{
    char code = (char)get_char();

    game_input(code); // Check for keyboard input

    clear_screen(); // Clear the screen

    if (!pause)
    {
        update_time(); // Update the time_seconds and time_minutes variables
    }
    // Information based screens (depending on state of game)
    display_screen(); // Draw the game information

    gameover_screen(); // Draw the game over screen (if game over)

    // Movement based functions

    automatic_movement(); // Move the player that is chasing

    firework_seek();

    if (current_player == 'J')
    {
        chase(); // If jerry is close to tom, tom starts chasing jerry
    }
    else
    {
        evade();
    }

    // Spawning functions

    spawn_cheese(); // Spawn the cheese every 2 seconds

    spawn_moustraps(); // Spawn the mousetrap every 3 seconds

    // Collision functions

    caught_collision(); // If tom catches jerry, reset_game the level and lose a life

    traps_collision(); // If jerry collides with a trap, he loses a life

    cheese_collision(); // If the player touches the cheese, it is collected and added to the cheese score

    door_collision(); // If jerry touches the X, it moves to the next level

    firework_collision();

    // Drawing sprites

    draw_sprite(tom); // Draw the Tom Sprite

    draw_sprite(jerry); // Draw the jerry player

    draw_cheese(); // Draw the cheese

    draw_traps(); // Draw the cheese

    draw_fireworks();

    draw_sprite(&door);

    draw_walls(); // Draw the walls for the current level
}
int main(int argc, char *argv[])
{
    setup_screen();
    setup(argc, argv);
    while (!game_over)
    {
        loop();
        show_screen();
        timer_pause(DELAY);
    }
    return 0;
}