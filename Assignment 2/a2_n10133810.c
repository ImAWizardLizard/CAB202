#include "cab202_adc.h"
#include "lcd_model.h"
#include "usb_serial.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <cpu_speed.h>
#include <graphics.h>
#include <lcd.h>
#include <macros.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <util/delay.h>

// HELPERS
typedef enum
{
    false, // 0
    true   // 1
} bool;

// Enum which represent inputs given on the teensy
typedef enum
{
    BUTTON_LEFT,
    BUTTON_RIGHT,
    JOYSTICK_LEFT,
    JOYSTICK_RIGHT,
    JOYSTICK_UP,
    JOYSTICK_DOWN,
    JOYSTICK_CENTER
} Input;

// Makes it easier to give ADC channels
enum
{
    ADC_LEFT,
    ADC_RIGHT
};

// Struct which represents a sprite within the game, easier keeping all of the values packed together
typedef struct Sprite
{
    bool draw;
    double x, y, dx, dy;
    uint8_t *bitmap;
    uint8_t width, height;
} Sprite;

// Definitions

// ADC values
#define ADC_MAX (1023)
#define THRESHOLD (1000)

#define STATUS_BAR_HEIGHT (8)
#define SCREEN_WIDTH (LCD_X)
#define SCREEN_HEIGHT (LCD_Y)

#define JERRY_INITIAL_X_POSITION (0);
#define JERRY_INITIAL_Y_POSITION (STATUS_BAR_HEIGHT + 1);
#define JERRY_WIDTH (6)
#define JERRY_HEIGHT (5)

#define TOM_INITIAL_X_POSITION (SCREEN_WIDTH - 5);
#define TOM_INITIAL_Y_POSITION (SCREEN_HEIGHT - 9);
#define TOM_WIDTH (6)
#define TOM_HEIGHT (5)

#define DOOR_WIDTH (6)
#define DOOR_HEIGHT (5)

#define JERRY_MAX_SPEED (2.0)
#define TOM_MAX_SPEED (1)

#define MAX_CHEESE (5)
#define CHEESE_WIDTH (6)
#define CHEESE_HEIGHT (5)

#define MAX_TRAPS (5)
#define TRAPS_WIDTH (3)
#define TRAPS_HEIGHT (6)

#define MAX_FIREWORKS (20)
#define FIREWORK_WIDTH (5)
#define FIREWORK_HEIGHT (5)
#define FIREWORK_SPEED (0.5)

// Debouncing variables
bool pressed = false;
uint16_t closed_num = 0;
uint16_t open_num = 0;

// Game state varables
uint8_t current_level = 1;          // The current level of the game
uint8_t levels = 1;                 // The number of levels
uint8_t lives = 5;                  // The number of lives Tom has left
uint8_t score = 0;                  // The current score Tom has
bool game_over = false;             // Set this to true when game is over
bool paused = false;                // Used to pause the game
uint8_t time_minutes, time_seconds; // Variables to store time
int Lcd_Contrast = LCD_DEFAULT_CONTRAST;

// Time based variables
uint16_t saved_time_diff;
uint16_t current_time_diff;
uint16_t current_time;
uint16_t reset_time;

// Sprite variables
Sprite jerry;
Sprite tom;
Sprite door;
Sprite cheese[MAX_CHEESE];
Sprite mousetraps[MAX_TRAPS];
Sprite fireworks[MAX_FIREWORKS];

// Variables used for collisions
bool can_move_up, can_move_down, can_move_left, can_move_right;

// Variables for spawning
bool can_spawn_cheese = false;
uint8_t number_of_cheese = 0;

bool can_spawn_traps = false;
uint8_t number_of_traps = 0;

bool door_spawned = false;

uint8_t number_of_fireworks = 0;

// Sprite bitmaps

// Tom is T
uint8_t tom_image[] = {
    0b11111100,
    0b00100000,
    0b00100000,
    0b00100000,
    0b00100000,
    0b00000000,
    0b00000000,
    0b00000000,
};

// Jerry is J
uint8_t jerry_image[] = {
    0b11111100,
    0b00010000,
    0b00010000,
    0b00100000,
    0b11000000,
    0b00000000,
    0b00000000,
    0b00000000,
};

uint8_t cheese_image[] = {
    0b01001000,
    0b11111100,
    0b01001000,
    0b11111100,
    0b01001000,
    0b00000000,
    0b00000000,
    0b00000000,
};

uint8_t mousetrap_image[] = {
    0b00110000,
    0b00110000,
    0b11111100,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
};

uint8_t door_image[] = {
    0b11111100,
    0b10000100,
    0b10000100,
    0b11111100,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
};

uint8_t firework_image[] = {
    0b10000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
};

// Overflow related code
uint32_t overflow_count = 0;
uint8_t width = LCD_X;
uint8_t height = LCD_Y;

// ---------------------------------------------------------
//	Timer overflow for measuring time passed.
// ---------------------------------------------------------
ISR(TIMER3_OVF_vect)
{
    overflow_count++;
}

// ---------------------------------------------------------
//	Timer overflow for serial communication.
// ---------------------------------------------------------

#define FREQ 8000000.0
#define PRESCALE 256.0
#define TIMER_SCALE 256.0

double interval = 0;

ISR(TIMER0_OVF_vect)
{
    interval += TIMER_SCALE * PRESCALE / FREQ;

    if (interval >= 1.0)
    {
        interval = 0;
        PORTD ^= 1 << 6;
    }
}

// FUNCTIONS

// Teensy and setup related functions
void setup_teensy_controls();
double get_adc_value(uint8_t channel);
void initalize_uart();

// Screen related functions
void start_screen();
void game_over_screen();
void game_information();
void check_if_game_over();
void restart_game();

// Time related functions
double get_elapsed_time();
void calculate_current_time();

// Input related functions
void block_input(Input input);
bool button_pressed(Input input);

// Speed related functions
float random_speed(uint8_t speed);

// Collision based functions
int sprite_collision(Sprite *sprite1, Sprite *sprite2);
void check_borders_jerry();
void check_borders_tom();
void check_borders_fireworks();
void check_borders();
void collect_cheese();
void hit_traps();
void tom_and_jerry_collision();
void door_collision();

// Functions related to the screen and pixels
void draw_bitmap(Sprite *sprite);
bool pixel_exists(int x, int y);
bool space_empty(int x, int y, Sprite *sprite);

// Movement functions
void jerry_movement();
void tom_randomly_turn(int to, int from);

// Spawning functions
void spawn_cheese();
void spawn_mousetraps();
void spawn_firework();

// Drawing and setup based functions
void draw_sprites();
void draw_first_level();
void setup_sprites();
void reset_tom();
void reset_jerry();

// Serial functions
void recieve_serial_input();
void send_game_information();

// Base functions
void setup();
void process();

// The two functions below disable the watchdog timer after a reset stopping infinite resets
void wdt_init(void) __attribute__((naked)) __attribute__((section(".init3")));

// Function Implementation
void wdt_init(void)
{
    MCUSR = 0;
    wdt_disable();

    return;
}

// Reset the teensy
#define soft_reset()           \
    do                         \
    {                          \
        wdt_enable(WDTO_15MS); \
        for (;;)               \
        {                      \
        }                      \
    } while (0)

// Helper function which sets up the teensy on a basic level and initalizes its inputs/outputs as well as serial communication
void setup_teensy_controls(void)
{
    // Set clock speed of teensy
    set_clock_speed(CPU_8MHz);

    // Initalize the LCD
    lcd_init(LCD_DEFAULT_CONTRAST);

    // Left and right buttons
    SET_INPUT(DDRF, 6); // Left
    SET_INPUT(DDRF, 5); // Right

    // Joystick pins
    SET_INPUT(DDRD, 1); // Up
    SET_INPUT(DDRB, 1); // Left
    SET_INPUT(DDRD, 0); // Right
    SET_INPUT(DDRB, 7); // Down
    SET_INPUT(DDRB, 0); // Center

    // Initalize reading values from the adc
    adc_init();

    // Initalize the timer
    TCCR3A = 0;
    TCCR3B = 4;

    TIMSK3 = 1;

    // Set Timer 0 to overflow approx 122 times per second. (USB serial stuff)
    TCCR0B |= 4;
    TIMSK0 = 1;

    // Turn on interupts
    sei();

    // Initalize serial communication
    usb_init();

    while (!usb_configured())
    {
        // Block until USB is ready.
    }
}

// Helper function which reads the ADC value and returns a value between 0 and 1
double get_adc_value(uint8_t channel)
{
    return (double)adc_read(channel) / (double)ADC_MAX;
}

// Helper function which sends characters over the serial conneciton to the computer
void usb_serial_send(char *message)
{
    // Cast to avoid "error: pointer targets in passing argument 1
    //	of 'usb_serial_write' differ in signedness"
    usb_serial_write((uint8_t *)message, strlen(message));
}

// Helper function which gets the elapsed time (miliseconds)
double get_elapsed_time()
{
    return (overflow_count * 65536.0 + TCNT3) * 256 / 8000000;
}

// Helper function which caluclates the current time
void calculate_current_time()
{
    if (!paused)
    {
        current_time = get_elapsed_time();
        current_time_diff = current_time - reset_time;
        time_seconds = (int)current_time_diff % 60;
        time_minutes = (int)current_time_diff / 60;
    }
}

// Helper function used to draw a sprites bitmap to the screen
void draw_bitmap(Sprite *sprite)
{
    if (sprite->draw)
    {
        if (sprite->x >= SCREEN_WIDTH || sprite->x + sprite->width <= 0 || sprite->y >= SCREEN_HEIGHT || sprite->y <= STATUS_BAR_HEIGHT)
            return;

        int byte_width = (sprite->width + 7) / 8;
        uint8_t bit_val;

        for (size_t i = 0; i < sprite->height; i++)
        {
            for (size_t j = 0; j < sprite->width; j++)
            {
                bit_val = BIT_VALUE(sprite->bitmap[(int)(i * byte_width + j / 8)], (7 - j % 8));
                if (bit_val)
                    draw_pixel(sprite->x + j, sprite->y + i, bit_val);
            }
        }
    }
}

// Check to see if the pixel on the screen is active
bool pixel_exists(int x, int y)
{
    // If area selected is out of bounds
    if (x < 0 || y <= STATUS_BAR_HEIGHT || x >= LCD_X || y >= LCD_Y)
    {
        return 1;
    }
    uint8_t bank = y >> 3;
    return screen_buffer[bank * LCD_X + x];
}

// Check to see if the space at the coordinates is empty, in regards to the sprites width and height (if it is allowed to be drawn there)
bool space_empty(int x, int y, Sprite *sprite)
{
    for (size_t i = y; i <= y + sprite->height; i++)
    {
        for (size_t j = x; j <= x + sprite->width; j++)
        {
            if (pixel_exists(j, i))
            {
                return false;
            }
        }
    }
    return true;
}

// Pause the game (timer)
void pause_game()
{

    if (!paused) // If the game is not paused, change special variables before pausing
    {
        saved_time_diff = current_time_diff; // Save current time
        // Don't spawn cheese and mousetraps
        // Walls should not move
    }
    else // If the game is paused, change special variables before unpausing
    {
        // Update the timer
        reset_time = get_elapsed_time() - saved_time_diff;

        // Continue spawn cheese and mousetraps
        // Walls should start moving again
    }
    paused = !paused; // Flip the pased value
}

// Used for debouncing, helps with button input
void block_input(Input input)
{
    while (button_pressed(input))
        ;
}

// Check for the current input the player is giving
bool button_pressed(Input input)
{
    Input current_input = -1;
    if (BIT_IS_SET(PINF, 6))
    {
        current_input = BUTTON_LEFT;
    }
    else if (BIT_IS_SET(PINF, 5))
    {
        current_input = BUTTON_RIGHT;
    }
    else if (BIT_IS_SET(PIND, 1))
    {
        current_input = JOYSTICK_UP;
    }
    else if (BIT_IS_SET(PINB, 7))
    {
        current_input = JOYSTICK_DOWN;
    }
    else if (BIT_IS_SET(PINB, 1))
    {
        current_input = JOYSTICK_LEFT;
    }
    else if (BIT_IS_SET(PIND, 0))
    {
        current_input = JOYSTICK_RIGHT;
    }
    else if (BIT_IS_SET(PINB, 0))
    {
        current_input = JOYSTICK_CENTER;
    }

    return current_input == input;
}

// Return a random speed between 0 and the speed provided
float random_speed(uint8_t speed)
{
    srand(get_elapsed_time() * rand());
    return ((float)rand() / (float)RAND_MAX) * speed;
}

// Helper function used to detect if two sprites have collided
int sprite_collision(Sprite *sprite1, Sprite *sprite2)
{
    if (sprite1->draw && sprite2->draw) // If both sprites are being drawn to the screen
    {
        return sprite1->x + sprite1->width >= sprite2->x && sprite1->x <= sprite2->x + sprite1->width && sprite1->y + sprite1->height >= sprite2->y && sprite1->y <= sprite2->y + sprite2->height;
    }
    else
    {
        return false;
    }
}

void check_borders_jerry()
{
    // Check borders for jerry

    if (jerry.x >= width - jerry.width)
    {
        can_move_right = false;
    }
    else
    {
        can_move_right = true;
    }

    if (jerry.x <= 1)
    {
        can_move_left = false;
    }
    else
    {
        can_move_left = true;
    }

    if (jerry.y >= height - jerry.height)
    {
        can_move_down = false;
    }
    else
    {
        can_move_down = true;
    }

    if (jerry.y < STATUS_BAR_HEIGHT + 2)
    {
        can_move_up = false;
    }
    else
    {
        can_move_up = true;
    }
}

void check_borders_tom()
{
    if (tom.x >= width - tom.width) // Right wall
    {
        tom_randomly_turn(120, 240);
    }
    else if (tom.x <= 1) // Left Wall
    {
        tom_randomly_turn(-60, 60);
    }
    else if (tom.y >= height - tom.height) // Bottom wall
    {
        tom_randomly_turn(30, 150);
    }
    else if (tom.y <= STATUS_BAR_HEIGHT + 2) // Top wall
    {
        tom_randomly_turn(210, 330);
    }
}

void check_borders_fireworks()
{
    Sprite *firework;
    for (size_t i = 0; i < MAX_FIREWORKS; i++)
    {
        firework = &fireworks[i];
        if (firework->draw)
        {
            if (firework->x >= width - firework->width || firework->x <= 1 || firework->y >= height - firework->height || firework->y < STATUS_BAR_HEIGHT + 2)
            {
                firework->draw = false;
                number_of_fireworks--;
            }
        }
    }
}

// Check to see if jerry, tom or the fireworks have collided with the screen borders
void check_borders()
{
    // Check borders for jerry
    check_borders_jerry();

    // Check borders for tom
    check_borders_tom();

    // Check borders for fireworks
    check_borders_fireworks();
}

void wall_collisions()
{
    // Detect wall collisions
}

// Detect to see if jerry has collided with cheese, adding 1 point
void collect_cheese()
{
    Sprite *cheese_piece;
    for (size_t i = 0; i < MAX_CHEESE; i++)
    {
        cheese_piece = &cheese[i];
        if (sprite_collision(&jerry, cheese_piece))
        {
            cheese_piece->draw = false;
            score++;
            number_of_cheese--;
        }
    }
}

// Detect to see if jerry has collided with a mousetrap, if so, take 1 life from jerry
void hit_traps()
{
    Sprite *mousetrap;
    for (size_t i = 0; i < MAX_TRAPS; i++)
    {
        mousetrap = &mousetraps[i];
        if (sprite_collision(&jerry, mousetrap))
        {
            mousetrap->draw = false;
            lives--;
            number_of_traps--;
        }
    }
}

// Detect to see if the fireworks have collided with tom, which will reset toms position, and give jerry 1 point
void hit_fireworks()
{
    Sprite *firework;
    for (size_t i = 0; i < MAX_FIREWORKS; i++)
    {
        firework = &fireworks[i];
        if (firework->draw)
        {
            if (sprite_collision(firework, &tom))
            {
                score++;
                firework->draw = false;
                reset_tom();
            }
        }
    }
}

// Detect if tom and jerry have collided, if so, reset their positions
void tom_and_jerry_collision()
{
    if (sprite_collision(&tom, &jerry))
    {
        lives--;
        reset_jerry();
        reset_tom();
    }
}

// Check to see if jerry has collided with the door, which results in jerry proceeding to the next level
void door_collision()
{
    if (sprite_collision(&jerry, &door))
    {
        current_level++;
    }
}

// Check to see if the player want's to pause the game
void check_pause_input(void)
{
    // Check if right button is pressed
    if (button_pressed(BUTTON_RIGHT))
    {
        block_input(BUTTON_RIGHT);
        pause_game();
    }
}

// If the left button is pressed, go to the next level
void next_level()
{
    if (button_pressed(BUTTON_LEFT))
    {
        block_input(BUTTON_LEFT);
        current_level++;
    }
}

// Update jerry's movement
void jerry_movement()
{
    double speed_modulator = get_adc_value(ADC_LEFT);
    double new_speed = speed_modulator * JERRY_MAX_SPEED;

    jerry.dx = jerry.dy = new_speed;

    if (button_pressed(JOYSTICK_UP))
    {
        if (can_move_up)
        {
            jerry.y -= jerry.dy;
        }
    }
    else if (button_pressed(JOYSTICK_DOWN))
    {
        if (can_move_down)
            jerry.y += jerry.dy;
    }
    else if (button_pressed(JOYSTICK_LEFT))
    {
        if (can_move_left)
            jerry.x -= jerry.dx;
    }
    else if (button_pressed(JOYSTICK_RIGHT))
    {
        if (can_move_right)
            jerry.x += jerry.dx;
    }
    else if (button_pressed(JOYSTICK_CENTER))
    {
        block_input(JOYSTICK_CENTER);
        spawn_firework();
    }
}

// Update tom's movement
void tom_movement()
{
    double speed_modulator = get_adc_value(ADC_LEFT);

    if (!paused)
    {
        tom.x += tom.dx * speed_modulator;
        tom.y += tom.dy * speed_modulator;
    }
}

// Change Tom's direction randomly, between two numbers which represent the degrees
void tom_randomly_turn(int from, int to)
{
    srand(get_elapsed_time() * rand());

    // randomly generate new speed
    tom.dx = tom.dy = random_speed(TOM_MAX_SPEED);

    //  randomly generate new direction to move towards
    double radians = (rand() % to + from) * M_PI / 180;
    double s = sin(radians);
    double c = cos(radians);
    double dx = c * tom.dx + s * tom.dy;
    double dy = -s * tom.dx + c * tom.dy;
    tom.dx = dx;
    tom.dy = dy;
}

// Make sure the fireworks constantly update their direction to follow Tom
void firework_seek()
{
    int d, t1, t2;
    Sprite *firework;
    if (number_of_fireworks > 0)
    {
        for (size_t i = 0; i < MAX_FIREWORKS; i++)
        {
            firework = &fireworks[i];
            if (firework->draw)
            {
                t1 = tom.x - firework->x;
                t2 = tom.y - firework->y;
                d = sqrt(t1 * t1 + t2 * t2);
                firework->dx = t1 * FIREWORK_SPEED / d;
                firework->dy = t2 * FIREWORK_SPEED / d;
                firework->x += firework->dx;
                firework->y += firework->dy;
            }
        }
    }
}

// Spawn cheese randomly, at 2 second intervals around the level
void spawn_cheese()
{
    srand(get_elapsed_time() * rand());
    int x, y;
    Sprite *cheese_piece;
    if (number_of_cheese <= 5)
    {
        if (time_seconds % 2 == 0 && time_seconds != 0) // If it is at a 2 second interval
        {
            if (can_spawn_cheese)
            {
                for (size_t i = 0; i < MAX_CHEESE; i++)
                {
                    cheese_piece = &cheese[i];
                    // Fith one isnt drawing because it isnt being reached
                    if (cheese_piece->draw == false)
                    {
                        do
                        {
                            x = rand() % width + 1;
                            y = rand() % height + STATUS_BAR_HEIGHT;
                        } while (!space_empty(x, y, cheese_piece));

                        cheese_piece->x = x;
                        cheese_piece->y = y;
                        cheese_piece->draw = true;

                        number_of_cheese++;
                    }
                    else
                    {
                        continue;
                    }
                    can_spawn_cheese = false;
                    break;
                }
            }
        }
        else
        {
            can_spawn_cheese = true;
        }
    }
}

// Spawn mousetraps at Toms position, every 3 seconds
void spawn_mousetraps()
{
    srand(get_elapsed_time() * rand());
    Sprite *mousetrap;
    if (number_of_traps <= 5)
    {
        if (time_seconds % 3 == 0 && time_seconds != 0) // If it is at a 3 second interval
        {
            if (can_spawn_traps)
            {
                for (size_t i = 0; i < MAX_TRAPS; i++)
                {
                    mousetrap = &mousetraps[i];
                    if (mousetrap->draw == false)
                    {
                        mousetrap->x = tom.x + (TOM_WIDTH / 2);
                        mousetrap->y = tom.y + (TOM_HEIGHT / 2);
                        mousetrap->draw = true;
                    }
                    else
                    {
                        continue;
                    }
                    can_spawn_traps = false;
                    break;
                }
            }
        }
        else
        {
            can_spawn_traps = true;
        }
    }
}

void spawn_door()
{
    srand(get_elapsed_time() * rand());
    int x, y;
    if (score >= 5 && !door_spawned)
    {
        do
        {
            x = rand() % width + 1;
            y = rand() % height + STATUS_BAR_HEIGHT;
        } while (!space_empty(x, y, &door));

        door.x = x;
        door.y = y;
        door.draw = true;
        door_spawned = true;
    }
}

void spawn_firework()
{
    if (score >= 3)
    {
        Sprite *firework;
        for (size_t i = 0; i < MAX_FIREWORKS; i++)
        {
            firework = &fireworks[i];
            if (firework->draw == false)
            {
                fireworks->x = jerry.x;
                fireworks->y = jerry.y;
                fireworks->draw = true;
                number_of_fireworks++;
            }
            else
            {
                continue;
            }
            break;
        }
    }
}

// Reset jerry to his original position
void reset_jerry()
{
    jerry.x = JERRY_INITIAL_X_POSITION;
    jerry.y = JERRY_INITIAL_Y_POSITION;
}

// Reset tom to his original postiion
void reset_tom()
{
    tom.x = TOM_INITIAL_X_POSITION;
    tom.y = TOM_INITIAL_Y_POSITION;
}

// Setup the sprites before playing the game
void setup_sprites()
{
    // Setup characters
    tom.x = TOM_INITIAL_X_POSITION;
    tom.y = TOM_INITIAL_Y_POSITION;
    tom.dx = 1;
    tom.dy = 0;
    tom.bitmap = tom_image;
    tom.width = TOM_WIDTH;
    tom.height = TOM_HEIGHT;
    tom.draw = true;

    jerry.x = 0;
    jerry.y = 9;
    jerry.dx = jerry.dy = JERRY_MAX_SPEED;
    jerry.bitmap = jerry_image;
    jerry.width = JERRY_WIDTH;
    jerry.height = JERRY_HEIGHT;
    jerry.draw = true;

    // Setup collection objects
    door.bitmap = door_image;
    door.height = DOOR_HEIGHT;
    door.width = DOOR_WIDTH;

    // Cheese
    for (size_t i = 0; i < MAX_CHEESE; i++)
    {
        cheese[i].bitmap = cheese_image;
        cheese[i].draw = false;
        cheese[i].width = CHEESE_WIDTH;
        cheese[i].height = CHEESE_HEIGHT;
    }

    // Mousetraps
    for (size_t i = 0; i < MAX_TRAPS; i++)
    {
        mousetraps[i].bitmap = mousetrap_image;
        mousetraps[i].draw = false;
        mousetraps[i].width = TRAPS_WIDTH;
        mousetraps[i].height = TRAPS_HEIGHT;
    }

    // Fireworks
    for (size_t i = 0; i < MAX_FIREWORKS; i++)
    {
        fireworks[i].bitmap = firework_image;
        fireworks[i].draw = false;
        fireworks[i].width = FIREWORK_WIDTH;
        fireworks[i].height = FIREWORK_HEIGHT;
    }
}

// Draw the sprites to the screen
void draw_sprites()
{
    draw_bitmap(&jerry);
    draw_bitmap(&tom);
    draw_bitmap(&door);

    for (size_t i = 0; i < MAX_CHEESE; i++)
    {
        draw_bitmap(&cheese[i]);
    }

    for (size_t i = 0; i < MAX_TRAPS; i++)
    {
        draw_bitmap(&mousetraps[i]);
    }

    // Fireworks
    for (size_t i = 0; i < MAX_FIREWORKS; i++)
    {
        draw_bitmap(&fireworks[i]);
    }
}

// Draw the walls tot he screen
void draw_walls()
{
    draw_line(18, 15, 13, 25, FG_COLOUR);
    draw_line(23, 35, 25, 45, FG_COLOUR);
    draw_line(45, 10, 60, 10, FG_COLOUR);
    draw_line(58, 25, 72, 30, FG_COLOUR);
}

// Display the status bar within the game
void game_information()
{
    // Update the global time variables
    calculate_current_time();

    draw_line(0, STATUS_BAR_HEIGHT, width, STATUS_BAR_HEIGHT, '-');

    char status_str[60];

    // Format the values into strings
    snprintf(status_str, sizeof(status_str), "%d:%d:%d     %02d:%02d", current_level, lives, score, time_minutes, time_seconds);

    draw_string(0, 0, status_str, FG_COLOUR);
}

// Display the start screen before the game starts, press the right button to start the game
void start_screen()
{
    draw_string((width / 2) - 30, height / 6, "Tom and Jerry", FG_COLOUR);
    draw_string((width / 2) - 30, height / 2.35, "Harry Newton", FG_COLOUR);
    draw_string((width / 2) - 30, height / 1.5, "n10133810", FG_COLOUR);

    // Put the memory into the screen buffer
    show_screen();

    for (;;)
    {
        if (button_pressed(BUTTON_RIGHT))
        {
            block_input(BUTTON_RIGHT);
            break;
        }
    }
}

// Constantly check if the game is over
void check_if_game_over()
{
    if (lives <= 0 || current_level > levels)
    {
        game_over = true;
    }
}

// Display the game over screen, asking the user if they want to restart
void game_over_screen()
{
    clear_screen();

    draw_string(0.25 * width, height / 7, "Game Over", FG_COLOUR);
    draw_string(0.25 * width, height / 2, "Restart?", FG_COLOUR);
    draw_string(0.1 * width, height / 1.5, "(right button)", FG_COLOUR);

    show_screen();

    for (;;)
    {
        if (button_pressed(BUTTON_RIGHT))
        {
            soft_reset();
            break;
        }
    }
}

// Send information over the serial connection to the computer, showing game state information
void send_game_information()
{
    char timestamp_str[25];
    char current_level_str[25];
    char lives_str[25];
    char score_str[15];
    char fireworks_str[30];
    char cheese_str[30];
    char cheese_consumed_str[40];
    char mousetraps_str[30];
    char paused_str[20];

    snprintf(timestamp_str, sizeof(timestamp_str), "Timestamp: %02d:%02d\r\n", time_minutes, time_seconds);
    snprintf(current_level_str, sizeof(current_level_str), "Current Level: %d\r\n", current_level);
    snprintf(lives_str, sizeof(lives_str), "Jerry's Lives: %d\r\n", lives);
    snprintf(score_str, sizeof(score_str), "Score: %d\r\n", score);
    snprintf(fireworks_str, sizeof(fireworks_str), "Number of Fireworks: %d\r\n", number_of_fireworks);
    snprintf(cheese_str, sizeof(cheese_str), "Number of Cheese: %d\r\n", number_of_cheese);
    snprintf(cheese_consumed_str, sizeof(cheese_consumed_str), "Amount of cheese consumed: %d\r\n", score);
    snprintf(mousetraps_str, sizeof(mousetraps_str), "Number of Mousetraps: %d\r\n", number_of_traps);
    snprintf(paused_str, sizeof(paused_str), "Paused: %s\r\n\n", paused ? "True" : "False");

    usb_serial_send(timestamp_str);
    usb_serial_send(current_level_str);
    usb_serial_send(lives_str);
    usb_serial_send(score_str);
    usb_serial_send(fireworks_str);
    usb_serial_send(cheese_str);
    usb_serial_send(cheese_consumed_str);
    usb_serial_send(mousetraps_str);
    usb_serial_send(paused_str);
}

// Recieve serial input from the computer to control the game
void recieve_serial_input()
{
    int16_t char_code = usb_serial_getchar();

    switch ((char)char_code)
    {
    case 'w':
        if (can_move_up)
            jerry.y -= jerry.dy;
        break;
    case 'a':
        if (can_move_left)
            jerry.x -= jerry.dx;
        break;
    case 's':
        if (can_move_down)
            jerry.y += jerry.dy;
        break;
    case 'd':
        if (can_move_right)
            jerry.x += jerry.dx;
        break;
    case 'f':
        spawn_firework();
        break;
    case 'p':
        pause_game();
        break;
    case 'l':
        current_level++;
        break;
    case 'i':
        send_game_information();
        break;
    }
}

void setup()
{
    // Clear the screen buffer
    clear_screen();

    // Setup teensy input and outputs, debouncing and serial communication
    setup_teensy_controls();

    // Display the name of the game, student name and student number
    start_screen();

    // Setup the sprtes used for the game
    setup_sprites();

    // Generate first level
}

void process(void)
{
    /// SPAWNING FUNCTIONS ///
    spawn_cheese();
    spawn_mousetraps();
    spawn_door();
    /// SPAWNING FUNCTIONS ///

    // Clear the screen buffer
    clear_screen();

    // Serial functions
    recieve_serial_input();

    /// MOVE ELEMENTS ON THE SCREEN ///
    jerry_movement(); // Detect input to move jerry
    tom_movement();   // Move tom at a random direction and speed
    firework_seek();
    /// MOVE ELEMENTS ON THE SCREEN ///

    /// DETECT COLLISIONS ///
    check_borders();           // Check if sprites collide with the borders
    collect_cheese();          // Collisions for the cheese
    hit_traps();               // Collisions for the moustraps
    door_collision();          // Collision for the door
    tom_and_jerry_collision(); // Collision for when Tom catches jerry
    /// DETECT COLLISIONS ///

    /// DRAW TO THE SCREEN ///
    game_information(); // Draw the status bar holding information regarding the game state
    draw_sprites();     // Render the sprites
    draw_walls();       // Draw the walls within the level
    /// DRAW TO THE SCREEN ///

    next_level(); // Check to see if the user wants to got to the next level

    check_pause_input(); // Check if the user wants to pause the game

    check_if_game_over(); // Check if game over

    show_screen(); // Put memory into the screen buffer
}

int main(void)
{
    // Setup the game
    setup();

    for (;;)
    {
        // Delay for 10 miliseconds
        _delay_ms(10);

        // Play the game
        process();

        // If the game is over, break the loop
        if (game_over)
            break;
    }

    // Display the game over screen and ask if user wants to restart the game
    game_over_screen();
}
