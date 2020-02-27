#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>


#ifdef _WIN32
	#include <windows.h>
	#include <conio.h>
#endif

// Height of the field
#define HEIGHT 25
// Width of the field
#define WIDTH 60
// Drawings per second
#define FPS 6


// Sleeps for a given period of milliseconds
void wait(unsigned int ms)
{
#ifdef _WIN32
	Sleep(ms);
#else
	struct timespec ts;
	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000;		// equals (ms % 1_000) / 1_000_000
	nanosleep(&ts, &ts);
#endif
}


long getNanoTime()
{
	struct timespec ts;
	int i = timespec_get(&ts, TIME_UTC);
	return (long)ts.tv_sec * 1000000000L + ts.tv_nsec;
}


// Tells how the game looks and whether inputs must be read
typedef enum { EXIT = 0, MAIN_MENU = 1, SNAKE_GAME = 2, SNAKE_GAME_SPAWNPROCESS = 3, PAUSE = 4, GAME_OVER = 5 } GameState;
typedef enum { AIR = 0, WALL = 1, SNAKE_HEAD = 2, SNAKE_PART = 3, FOOD = 4 } GameElementID;
typedef enum { NORTH = 0, EAST = 1, SOUTH = 2, WEST = 3 } Direction;


typedef struct {
	short score;
	short highscore;
} ScoreBoard;


typedef struct {
	GameState state;
	Direction dir;
} Input;


typedef struct {
	short x;
	short y;
} Position;


struct SnakePart {
	Position pos;
	struct SnakePart *previous;
};
typedef struct SnakePart SnakePart;


typedef struct {
	ScoreBoard* score;
	Input* inp;
	SnakePart* snake;
	char* image;
	GameElementID* field;
} Setup;


// Creates new SnakeElement in memory and returns its pointer
SnakePart* createSnakePart(Position pos, SnakePart *previous)
{
	SnakePart *part = (SnakePart*) malloc(sizeof(SnakePart));
	if (part == NULL) exit(-1);
	part->pos = pos;
	part->previous = previous;
	return part;
}


Position *updatePos(Position *pos, Direction dir)
{
	switch (dir)
	{
	case NORTH:
		pos->y--;
		break;
	case EAST:
		pos->x++;
		break;
	case SOUTH:
		pos->y++;
		break;
	case WEST:
		pos->x--;
		break;
	default:
		break;
	}

	// That there will be no error regarding error bounds!
	if (pos->x >= WIDTH) pos->x = WIDTH - 1;
	else if (pos->x < 0) pos->x = 0;
	if (pos->y >= HEIGHT) pos->y = HEIGHT - 1;
	else if (pos->y < 0) pos->y = 0;

	return pos;
}


// Evaluates Direction for an element based on the position of the previous one
// Note that part->previous MUST NOT BE NULL!
Direction evalDir(SnakePart* part)
{
	if (part->pos.x == part->previous->pos.x)
	{
		if (part->pos.y > part->previous->pos.y) return NORTH;
		else return SOUTH;
	}
	else
	{
		if (part->pos.x > part->previous->pos.x) return WEST;
		else return EAST;
	}
}


Direction getReverse(Direction dir)
{
	switch (dir)
	{
	case NORTH:
		return SOUTH;
	case EAST:
		return WEST;
	case SOUTH:
		return NORTH;
	case WEST:
		return EAST;
	default:
		return -1;
	}
}


char getChar(GameElementID id)
{
	char c = 0x0;
	switch (id)
	{
	case AIR:
		c = 0x20;		// Equivalent to Space in ASCII
		break;
	case WALL:
		c = 0x23;		// Equivalent to # in ASCII
		break;
	case SNAKE_HEAD:
		c = 0x7F;
		break;
	case SNAKE_PART:
		c = 0xDB;
		break;
	case FOOD:
		c = 0xFE;
		break;
	default:
		c = 21;			// Equivalent to ! in ASCII
		break;
	}
	return c;
}


void insertString(char *dest, char src[], int lastPos)
{
	short length = 0, index;
	while (src[length]) length++;
	for (index = 0; index < length; index++)
		*(dest + lastPos - (length - index)) = src[index];
}


// Initializes all values with 0
void init_field(GameElementID* field)
{
	// Init with AIR
	{
		short i;	// WIDTH * HEIGHT can exceed sizeof(byte)
		for (i = 0; i < WIDTH * HEIGHT; i++)
			*(field + i) = AIR;
	}
	// Set Outer Borders
	{
		byte x, y;
		for (x = 0; x < WIDTH; x++) {
			*(field + x) = WALL;
			*(field + (HEIGHT - 1) * WIDTH + x) = WALL;
		}
		for (y = 1; y < HEIGHT-1; y++)
		{
			*(field + y*WIDTH) = WALL;
			*(field + (y + 1) * WIDTH - 1) = WALL;
		}
	}
}


Input *getInputs(Input *inp)
{
	Input newInp = { inp->state, inp->dir };
	while (_kbhit())
	{
		switch (getch())
		{
		case 'w':
			if (inp->dir != SOUTH)
				newInp.dir = NORTH;
			break;
		case 'a':
			if (inp->dir != EAST)
				newInp.dir = WEST;
			break;
		case 's':
			if (inp->dir != NORTH)
				newInp.dir = SOUTH;
			break;
		case 'd':
			if (inp->dir != WEST)
				newInp.dir = EAST;
			break;
		case 'q':
			newInp.state = EXIT;
			break;
		case 'p':
			if (inp->state == MAIN_MENU) newInp.state = SNAKE_GAME;
			break;
		case 0x1B:
			if (inp->state == PAUSE)
				newInp.state = SNAKE_GAME;
			else if (inp->state == GAME_OVER)
				newInp.state = MAIN_MENU;
			else if (inp->state == MAIN_MENU)
				newInp.state = EXIT;
			else
				newInp.state = PAUSE;
			break;
		}
	}
	inp->dir = newInp.dir;
	inp->state = newInp.state;
	return inp;
}


Setup* setUp(short highscore)
{
	GameElementID* field = malloc(sizeof(GameElementID) * WIDTH * HEIGHT);
	init_field(field);

	char* image = malloc(sizeof(char) * (WIDTH) * (HEIGHT + 1));

	Position pos = { 10, 10 };
	SnakePart* snake = createSnakePart(*updatePos(&pos, SOUTH), createSnakePart(pos, NULL));

	ScoreBoard* score = (ScoreBoard*) malloc(sizeof(ScoreBoard));
	if (score == NULL) exit(-1);
	score->highscore = highscore;
	score->score = 0;

	Input* input = (Input*) malloc(sizeof(Input));
	if (input == NULL) exit(-1);
	input->dir = SOUTH;
	input->state = MAIN_MENU;

	Setup* setup = (Setup*)malloc(sizeof(Setup));
	if (setup == NULL || input == NULL || score == NULL || snake == NULL || image == NULL || field == NULL) exit(-1);
	setup->field = field;
	setup->image = image;
	setup->inp = input;
	setup->score = score;
	setup->snake = snake;

	return setup;
}


void cleanUp(SnakePart *snake, GameElementID *field, char *image, ScoreBoard *score, Input *input)
{
	SnakePart* previous = snake;
	SnakePart* previous2 = previous;
	while (previous->previous != NULL)
	{
		previous2 = previous->previous;
		free(previous);
		previous = previous2;
	}
	free(field);
	free(image);
	free(input);
}


SnakePart* updateSnakePos(SnakePart *snake, Direction dir)	// Could probably also be solved with recursion...
{
	SnakePart *previous = snake;
	while (previous->previous != NULL)
	{
		updatePos(&previous->pos, evalDir(previous));
		previous = previous->previous;
	}
	updatePos(&previous->pos, dir);
	return previous;
}


GameState checkSnake(SnakePart* head, SnakePart* tail, GameElementID* field, ScoreBoard* score)
{
	GameElementID id = *(field + head->pos.y * WIDTH + head->pos.x);
	switch (id)
	{
	case FOOD:
		score->score++;
		if (score->score > score->highscore) score->highscore++;
		return SNAKE_GAME_SPAWNPROCESS;
	case SNAKE_PART:
	case WALL:
		return GAME_OVER;
	}
	return SNAKE_GAME;
}


void updateField(GameElementID *field, SnakePart *snake, Position lastPos)
{
	srand(getNanoTime());
	*(field + lastPos.y * WIDTH + lastPos.x) = AIR;
	
	short pointerDelta = 0;
	short counter = 1;

	do
	{
		counter++;
		pointerDelta = snake->pos.y * WIDTH + snake->pos.x;
		*(field + pointerDelta) = SNAKE_PART;
	} while ((snake = snake->previous)->previous != NULL);

	*(field + snake->pos.y * WIDTH + snake->pos.x) = SNAKE_HEAD;

	// Spawn Actual Fruits
	bool spawn = ((rand()) % 30) == 0;
	if (spawn)
	{
		srand(getNanoTime());
		counter = (rand()) % ((HEIGHT-2) * (WIDTH-2) - counter);
		short i, a;
		for (i = 0, a = 0; a < counter; i++)
			if (*(field + i) == AIR) a++;
		*(field + --i) = FOOD;
	}
}


int draw(Input *lastInput, short currentFrame, SnakePart *snake, GameElementID *field, char *image, ScoreBoard *score)
{
	// UPDATE

	// Clean up for potential new game!
	bool isMainMenu = lastInput->state == MAIN_MENU;
	lastInput = getInputs(lastInput);

	if (lastInput->state == SNAKE_GAME || lastInput->state == SNAKE_GAME_SPAWNPROCESS)
	{
		Position pos = snake->pos;
		SnakePart* head = updateSnakePos(snake, lastInput->dir);
		lastInput->state = checkSnake(head, snake, field, score);
		updateField(field, snake, pos);
		
		// Actual update of screen image buffer
		short x, y;
		for (y = 0; y < HEIGHT; y++)
		{
			for (x = 0; x < WIDTH; x++)
			{
				*(image + y * (WIDTH + 1) + x) = getChar(*(field + y * WIDTH + x));
			}
			*(image + y * (WIDTH + 1) + WIDTH) = '\n';
		}
	}
	else if (lastInput->state == MAIN_MENU)
	{
		strcpy_s(image, WIDTH * (HEIGHT + 1), "\
                                                           \n\
     ###          ###        ##        ## ##        ##     \n\
     ####        ####       ####       ## ###       ##     \n\
     ## ##      ## ##      ##  ##      ## ## ##     ##     \n\
     ##  ##    ##  ##     ##    ##     ## ##  ##    ##     \n\
     ##   ##  ##   ##    ##      ##    ## ##    ##  ##     \n\
     ##    ####    ##   ############   ## ##     ## ##     \n\
     ##     ##     ##  ##          ##  ## ##       ###     \n\
     ##            ## ##            ## ## ##        ##     \n\
                                                           \n\
   ###          ### ########## ##        ## ##        ##   \n\
   ####        #### ##         ###       ## ##        ##   \n\
   ## ##      ## ## ##         ## ##     ## ##        ##   \n\
   ##  ##    ##  ## ######     ##  ##    ## ##        ##   \n\
   ##   ##  ##   ## ##         ##    ##  ## ##        ##   \n\
   ##    ####    ## ##         ##     ## ## ##        ##   \n\
   ##     ##     ## ##         ##       ###  ##      ##    \n\
   ##            ## ########## ##        ##    ######      \n\
                                                           \n\
                                                           \n\
###########################################################\n\
# Welcome to ASCII-Snake! Press:                          #\n\
#  (p) to play                                            #\n\
#  (q) to quit                                            #\n\
###########################################################");
	}
	else if (lastInput->state == GAME_OVER)
	{
		strcpy_s(image, WIDTH * (HEIGHT+1), "\
                                                          \n\
   ########          ##        ###          ### ##########\n\
 ###       ##       ####       ####        #### ##        \n\
##                 ##  ##      ## ##      ## ## ##        \n\
##                ##    ##     ##  ##    ##  ## ######    \n\
##     ######    ##      ##    ##   ##  ##   ## ##        \n\
##          #   ############   ##    ####    ## ##        \n\
 ###      ###  ##          ##  ##     ##     ## ##        \n\
   ########   ##            ## ##            ## ##########\n\
                                                          \n\
     ######     ##            ## ########## ########    ##\n\
   ##      ##    ##          ##  ##         ##     ##   ##\n\
 ##          ##   ##        ##   ##         ##      ##  ##\n\
 ##          ##    ##      ##    ######     ##     ##   ##\n\
 ##          ##     ##    ##     ##         ########    ##\n\
 ##          ##      ##  ##      ##         ##     ##   ##\n\
   ##      ##         ####       ##         ##      ##    \n\
     ######            ##        ########## ##       ## ##\n\
                                                          \n\
                                                          \n\
##########################################################\n\
# Score:                                        Point(s) #\n\
# Highscore:                                    Point(s) #\n\
# Press (q) to quit or (Esc) to return to the Main-Menu! #\n\
##########################################################");
		char buffer[5] = { 0x0, 0x0, 0x0, 0x0 };
		sprintf_s(buffer, 5, "%d", score->score);
		insertString(image, buffer, 1286);
		int i;
		for (i = 0; i < 5; i++) buffer[i] = 0x0;
		sprintf_s(buffer, 5, "%d", score->score > score->highscore ? score->score : score->highscore);
		insertString(image, buffer, 1345);
	}


	// RENDER

	// TODO: Try to improve this!
	// Clear Screen!
	system("@cls||clear");

	if (lastInput->state == PAUSE) printf("[PAUSED]\n");

	printf("%s", image);

	if (lastInput->state != GAME_OVER && lastInput->state != MAIN_MENU && lastInput->state != EXIT)
	{
		char c[] = "\
Score:            Point(s)\n\
High-Score:       Point(s)";

		char buffer[5] = { 0x0, 0x0, 0x0, 0x0 };
		sprintf_s(buffer, 5, "%d", score->score);
		insertString(c, buffer, 17);
		int i;
		for (i = 0; i < 5; i++) buffer[i] = 0x0;
		sprintf_s(buffer, 5, "%d", score->score > score->highscore ? score->score : score->highscore);
		insertString(c, buffer, 44);
		printf("%s", c);
	}

	if (lastInput->state == MAIN_MENU && !(isMainMenu))
	{
		cleanUp(snake, field, image, score, lastInput);
		lastInput = NULL;
	}

	return lastInput;
}


int main()
{
	printf("Starting ASCII-Snake");
	
	// Showing Loading Animation
	{
		time_t msec;
		short i;
		for (i = 0; i < 10; i++)
		{
			printf(".");
			msec = time(NULL) * 1000;
			wait(200 + (rand() * msec) % 600);
		}
		printf("\n");
	}

	printf("Loaded!\n");
	wait(200);
	
	{
		// TODO: Loading Animation + Actual init in parallel!
		GameElementID* field = NULL;
		char* image = NULL;
		SnakePart* snake = NULL;
		ScoreBoard* score = NULL;
		Input* inp = NULL;
		{
			Setup* setup = setUp(0);
			field = setup->field;
			image = setup->image;
			snake = setup->snake;
			score = setup->score;
			inp = setup->inp;
		}
		GameState currentState = SNAKE_GAME;
		unsigned short targetDelta = 1000 / FPS;
		short currentFrame = 0;
		long start;
		short highscore = 0;

		while (currentState)
		{
			start = getNanoTime();

			inp = draw(inp, currentFrame, snake, field, image, score);
			if (inp == NULL)
			{
				field = NULL;
				image = NULL;
				snake = NULL;
				score = NULL;
				{
					Setup* setup = setUp(highscore);
					field = setup->field;
					image = setup->image;
					snake = setup->snake;
					score = setup->score;
					inp = setup->inp;
				}
			}
			currentState = inp->state;
			
			if (currentFrame - FPS >= -1) currentFrame = 0;
			else currentFrame++;

			if (currentState == SNAKE_GAME_SPAWNPROCESS)
			{
				Position pos = { snake->pos.x, snake->pos.y };
				Direction dir = getReverse(evalDir(snake));
				snake = createSnakePart(*updatePos(&pos, dir), snake);
			}
			highscore = score->highscore;
			wait((int) (targetDelta - (getNanoTime()-start)/1000000000L));
		}

		// Clean up potential mess
		cleanUp(snake, field, image, score, inp);

		printf("\n\nYour Highscore was: %d Point(s)\n", highscore);
		printf("Exiting");
		// Showing Loading Animation
		{
			time_t msec;
			short i;
			for (i = 0; i < 10; i++)
			{
				printf(".");
				wait(500);
			}
			printf("\n");
		}
	}

	return 0;
}
