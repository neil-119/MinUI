/*
The MIT License (MIT)

Copyright (c) 2015 Neil Rao

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "mingui.h"

#include <iostream>
#include <ctime>



#define HAS_LEFT_WALL (1 << 0)
#define HAS_TOP_WALL (1 << 1)
#define HAS_RIGHT_WALL (1 << 2)
#define HAS_BOTTOM_WALL (1 << 3)
#define HAS_PIECE (1 << 4)
#define HAS_ENCODED_LETTER (1 << 5)

#define WALLVERT  '|'
#define WALLHORIZ '-'
#define SPACE     ' '
#define NOPIECE   '.'

const int board_size = 16; // size of square board
const int IdOffset   = 42; // Y offset to place robot
const int squareSideLength = 16; // the length of each block in pixels
const int ox = 270, oy = 120; // the top left pos of the board (where to render)

enum KBKEY { KBOTH, KBESC, KBUP, KBLEFT, KBDOWN, KBRIGHT };
enum POSINFO { NORMAL, WIN };

#define MAX_ROBOTS 4

typedef struct square_t
{
	int contents;
	char piece;
} square_t;


// kept globally for simplicity so teaching can focus on UI and not stack vs. heap stuff
int cur_robot;
char dest_letter;
int origin_robot;
char** buffer;
square_t** board;


// This function is called when the exit game button is clicked.
// Returning 0 will prevent the event from propagating.
int onExitButtonClicked(const char* event, Widget button, WidgetEvent e)
{
	exit_game();
	return 0;
}

// All this will do is put a border around the specified robot in the range [1, MAXROBOTS]
void selectRobot(int id)
{
	for (int i = 1; i <= MAX_ROBOTS; i++)
	{
		// Get the robot by its UI identifier
		char rId[] = { '0' + i, 0 };
		Widget r = get_widget_by_id(rId);

		// If it's valid
		if (r)
		{
			// Get the robot image in the container
			Widget robot = get_child(r, 1);

			// If the robot image is valid
			if (robot)
				set_border(robot, "white", id == i ? 2 : 0); // put a border around it
		}
	}
}

// When a robot is clicked, we'll select it.
int onRobotClicked(const char* event, Widget robotContainer, WidgetEvent e)
{
	// Get the widget's identifier
	const char* id = get_widget_class(robotContainer);

	// If it's an invalid widget, allow the click to continue
	if (strlen(id) == 0)
		return 1;

	// select the robot
	cur_robot = id[0] - '0';

	selectRobot(cur_robot);
	widget_refresh_all();

	return 0; // don't allow the click to continue further
}


// allocs space for an array of pieces
square_t** alloc_board(const int size_x, const int size_y)
{
	square_t** board = (square_t**)malloc(sizeof(square_t*) * size_y);

	if (!board)
	{
		std::cout << "Could not create board: malloc() failed" << std::endl;
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < size_y; i++)
		if (!(board[i] = (square_t*)calloc(size_x, sizeof(square_t))))
		{
			std::cout << "Could not create row: malloc() failed" << std::endl;
			exit(EXIT_FAILURE);
		}

	return board;
}

// frees the board
void free_board(square_t** board, const int size_y)
{
	for (int i = 0; i < size_y; i++)
		free(board[i]);

	free(board);
}

// parses a line and sets contents of square
void set_contents(square_t** board, const int size_x, char* line, char dest_char)
{
	int colIndx = 0;
	char* token = strtok(line, " ");
	square_t* square = NULL;

	// @todo this should do some basic checking on the data file in case the file is corrupt/invalid
	while (token != NULL)
	{
		// state
		switch (colIndx)
		{
		case 0:
		{
			int index = atoi(token);

			// convert index to row major order
			int row = index / size_x;
			int col = index - (row * size_x);

			square = &board[row][col];
			break;
		}

		case 1:
		{
			// set wall as being active if it is enabled
			for (int i = 0; i < 4; i++)
				if (token[i] == '1')
					square->contents |= 1 << i;

			break;
		}

		case 2:
		{
			// only add this piece if it's the destination
			if (token[0] == dest_char)
			{
				square->contents |= HAS_PIECE;
				square->piece = token[0];
			}
			break;
		}
		};

		token = strtok(NULL, " ");
		colIndx++;
	}
}

// decoupled size calculation
void calc_board_display_sizes(int& size_x, int& size_y)
{
	// The x is the number of pieces, so we want roughly a spot for each piece,
	// spaces, and walls.
	(size_x <<= 1)++;

	// there are top and bottom walls at the beginning and end, and padding between rows
	(size_y <<= 1)++;
}

// allocates space for the render buffer.
// By decoupling the board data from the actual rendering,
// we can opt to use OpenGL or whatever for the extra credit.
char** alloc_board_display(int size_x, int size_y)
{
	// update board sizes
	calc_board_display_sizes(size_x, size_y);

	// allocate char array to hold board output
	char** board_o = (char**)malloc(sizeof(char*) * size_y);
	for (int i = 0; i < size_y; i++)
	{
		board_o[i] = (char*)malloc(sizeof(char) * size_x);

		for (int j = 0; j < size_x; j++)
			board_o[i][j] = ' '; // default to space
	}

	return board_o;
}

// deallocs board display
void free_board_display(char** board_o, int size_y)
{
	// free
	for (int i = 0; i < size_y; i++)
		free(board_o[i]);
	free(board_o);
}

// writes the board to the char array
void write_board(square_t** board, char** board_o, int size_x, int size_y)
{
	// now draw it
	for (int i = 0; i < size_y; i++)
		for (int j = 0; j < size_x; j++)
		{
			// the square
			square_t* square = &board[i][j];

			// the actual index in the char array for the piece
			int board_i = (i << 1) + 1;
			int board_j = (j << 1) + 1;

			// @todo optimize

			// do we need a left wall?
			if (j == 0 || square->contents & HAS_LEFT_WALL)
			{
				board_o[board_i][board_j - 1] = WALLVERT;

				if (j == 0)
				{
					board_o[board_i - 1][board_j - 1] = WALLVERT;
					board_o[board_i + 1][board_j - 1] = WALLVERT;
				}
			}
			else if (board_o[board_i][board_j - 1] != WALLVERT)
				board_o[board_i][board_j - 1] = SPACE;

			// do we need a right wall?
			int boardRightWallReached = j == (size_x - 1);
			if (boardRightWallReached || square->contents & HAS_RIGHT_WALL)
			{
				board_o[board_i][board_j + 1] = WALLVERT;

				if (boardRightWallReached)
				{
					board_o[board_i - 1][board_j + 1] = WALLVERT;
					board_o[board_i + 1][board_j + 1] = WALLVERT;
				}
			}
			else if (board_o[board_i][board_j + 1] != WALLVERT)
				board_o[board_i][board_j + 1] = SPACE;

			// do we need a top wall?
			if (i == 0 || square->contents & HAS_TOP_WALL)
				board_o[board_i - 1][board_j] = WALLHORIZ;
			else if (board_o[board_i - 1][board_j] != WALLHORIZ)
				board_o[board_i - 1][board_j] = SPACE;

			// do we need a bottom wall?
			if (i == (size_y - 1) || square->contents & HAS_BOTTOM_WALL)
				board_o[board_i + 1][board_j] = WALLHORIZ;
			else if (board_o[board_i + 1][board_j] != WALLHORIZ)
				board_o[board_i + 1][board_j] = SPACE;

			// what do we put for this piece?
			if (square->contents & HAS_PIECE)
				board_o[board_i][board_j] = square->piece;
			else
				board_o[board_i][board_j] = NOPIECE;
		}
}

// loads the board elements
void load_board(char** board_o, int size_x, int size_y, const int ox, const int oy, const int squareSideLength)
{
	// account for walls and padding
	calc_board_display_sizes(size_x, size_y);

	// draw each line
	for (int i = 0; i < size_y; i++)
	{
		for (int j = 0; j < size_x; j++)
		{
			if (board_o[i][j] == WALLVERT || board_o[i][j] == WALLHORIZ || i == 0 || i == size_y - 1)
			{
				Widget wall = create_image("brick.png");
				set_width(wall, squareSideLength);
				set_height(wall, squareSideLength);
				set_position(wall, ox + j * squareSideLength, oy + i * squareSideLength);
			}
			else if (board_o[i][j] >= 'A' && board_o[i][j] <= 'Z')
			{
				// The id of the robot as a string
				char id[] = { board_o[i][j], 0 };

				// create the text element
				Widget letter = create_text(id);
				set_text_size(letter, 18);
				set_text_style(letter, FONT_BOLD);
				set_text_color(letter, "#6AFF59");

				// Position it
				set_position(letter, ox + j * squareSideLength, oy + i * squareSideLength - 7);
			}
			else if (board_o[i][j] >= '1' && board_o[i][j] <= '0' + MAX_ROBOTS)
			{
				// The id of the robot as a string
				char id[] = { board_o[i][j], 0 };

				// create a container to store the robot image and its number
				Widget robotContainer = create_container();
				
				// create the text element
				Widget robId = create_text(id);
				set_attribute(robId, "class", id);
				attach(robId, robotContainer); // attach it to the container

				// Style the text and container
				set_text(robotContainer, id);
				set_attribute(robotContainer, "id", id); // set the robot's id and name
				set_attribute(robotContainer, "class", id);

				set_text_color(robotContainer, "white");
				set_text_size(robotContainer, 22);
				set_text_style(robotContainer, FONT_BOLD);
				set_layer(robotContainer, 9);
				set_width(robotContainer, squareSideLength);
				set_height(robotContainer, squareSideLength + 10);
				set_position(robotContainer, ox + j * squareSideLength, oy + i * squareSideLength - IdOffset);

				// Create the robot image and attach it
				Widget robot = create_image("robot.png");
				set_width(robot, squareSideLength);
				set_height(robot, squareSideLength + 10);
				set_position(robot, 0, 0);
				set_attribute(robot, "class", id);
				attach(robot, robotContainer);

				// bind it to the click handler
				bind_event(robotContainer, "click", onRobotClicked);
			}
		}
	}
}

// Reads a file and returns a pointer to an array where each index is a line from the file.
// Feel free to use this in your future programs if you want, this whole file is released under
// a very open MIT license.
static char** GetLinesFromFile(const char* filename, int& maxrows)
{
	// Open the file
	FILE* f;
#ifdef WIN32
	fopen_s(&f, filename, "r");
#else
	f = fopen(filename, "r");
#endif

	// Starting line max length, starting array length
	const int stlnmax = 100, strowmax = 13000;

	// Start with the initial and grow as necessary.
	int r = 0, lnmax = stlnmax, rowmax = strowmax;

	// Buffers and array of strings
	char* buffer = (char*)malloc(sizeof(char) * lnmax);
	char** v = (char**)calloc(rowmax, sizeof(char*));

	// Exit on failure
	if (buffer == NULL || v == NULL || f == NULL)
		return NULL;

	// Loop through file until it terminates. Note that on EOF, we need to store the final buffer as the loop
	// body won't run.
	for (int c = 0, ch = fgetc(f); ; ch = fgetc(f), c++)
	{
		if (c == lnmax)
			buffer = (char*)realloc(buffer, sizeof(char) * (lnmax <<= 1));

		// If we reached the end of the line, save it and move on.
		if (ch == '\n' || ch == '\r' || ch == EOF)
		{
			// Resize as needed
			if (r == rowmax)
				v = (char**)realloc(v, sizeof(char*) * (rowmax <<= 1));

			// Save it
			buffer[c] = '\0';

			// get its length, and if it's not blank, add it
			if (strlen(buffer) > 0)
				v[r++] = buffer;
			else
				free(buffer);

			if (ch == EOF)
				break;

			// Reset line max and allocate space for new buffer
			buffer = (char*)malloc(sizeof(char) * (lnmax = stlnmax));

			// Restart line count
			c = -1;

			// Go on to next line.
			continue;
		}

		buffer[c] = ch;
	}

	maxrows = r;
	return v;
}

// free it when done.
void free_lines_array(char** v, int rowmax)
{
	for (int i = 0; i < rowmax && v[i] != 0; i++)
		free(v[i]);
	free(v);
}

// better random value calculation
// see http://stackoverflow.com/a/6852396/1860415
long random_at_most(long max)
{
	unsigned long
		// max <= RAND_MAX < ULONG_MAX, so this is okay.
		num_bins = (unsigned long)max + 1,
		num_rand = (unsigned long)RAND_MAX + 1,
		bin_size = num_rand / num_bins,
		defect = num_rand % num_bins;

	long x;
	do {
		x = rand();
	}
	// This is carefully written not to overflow
	while (num_rand - defect <= (unsigned long)x);

	// Truncated division is intentional
	return x / bin_size;
}

// parse data file
char** parse_data(const char* filename, square_t** board, int size_x, int& maxrows, char& dest_letter, int& origin_robot)
{
	char** lines = GetLinesFromFile(filename, maxrows);

	if (!lines)
	{
		std::cout << "File '" << filename << "' could not be opened. Are you sure that it exists?" << std::endl;
		exit(EXIT_FAILURE);
	}

	// set to 1 to enable random selection
	const int enableRandom = 0;

	// select destination and source
	dest_letter = enableRandom ? 'A' + random_at_most(atoi(lines[0]) - 1) : 'M';
	origin_robot = enableRandom ? random_at_most(MAX_ROBOTS - 1) + 1 : 2;

	// read larger data segment line by line
	for (int i = 2; i < (maxrows - 4); i++)
		set_contents(board, size_x, lines[i], dest_letter);

	// find out the four robot locations
	for (int i = (maxrows - MAX_ROBOTS), robot = 1; i < maxrows; i++, robot++)
	{
		// convert to row major
		int index = atoi(lines[i]);
		int row = index / size_x;
		int col = index - (row * size_x);
		square_t* sq = &board[row][col];

		// place robot
		char buffer[2];
#ifdef _WIN32
		itoa(robot, buffer, 10);
#else
		snprintf(buffer, sizeof(buffer), "%d", robot);
#endif
		sq->contents |= HAS_PIECE;
		sq->piece = buffer[0];
	}

	return lines;
}

// returns a value depending on key press
KBKEY get_arrow_key()
{
	const Uint8 *state = SDL_GetKeyboardState(NULL);
	
	if (state[SDL_SCANCODE_RIGHT])
		return KBRIGHT;
	else if (state[SDL_SCANCODE_UP])
		return KBUP;
	else if (state[SDL_SCANCODE_DOWN])
		return KBDOWN;
	else if (state[SDL_SCANCODE_LEFT])
		return KBLEFT;
	else if (state[SDL_SCANCODE_ESCAPE])
		return KBESC;

	return KBOTH;
}

// finds the specified robot
square_t* find_robot(int robot, square_t** board, const int size_x, const int size_y, int& posx, int& posy)
{
	for (int i = 0; i < size_y; i++)
		for (int j = 0; j < size_x; j++)
			if (board[i][j].contents & HAS_PIECE && board[i][j].piece == ('0' + robot))
			{
				posx = j;
				posy = i;
				return &board[i][j];
			}

	return NULL;
}

// moves the specified robot in the specified direction
POSINFO move_robot(int robot, KBKEY direction, square_t** board, const int size_x, const int size_y, char winningPos, int winningRobot)
{
	// must be valid robot
	if (robot <= 0 || robot > MAX_ROBOTS)
		return NORMAL;

	// must be valid direction
	if (direction != KBUP && direction != KBLEFT && direction != KBRIGHT && direction != KBDOWN)
		return NORMAL;

	// find this robot
	int x, y;
	square_t* r = find_robot(robot, board, size_x, size_y, x, y);
	if (!robot)
		return NORMAL;

	// current positions
	int curx = x, cury = y;

	// move it yo'
	for (;;)
	{
		// figure out if there's any kind of block in the specified direction
		if ((direction == KBUP && board[cury][curx].contents & HAS_TOP_WALL)
			|| (direction == KBDOWN && board[cury][curx].contents & HAS_BOTTOM_WALL)
			|| (direction == KBLEFT && board[cury][curx].contents & HAS_LEFT_WALL)
			|| (direction == KBRIGHT && board[cury][curx].contents & HAS_RIGHT_WALL))
			break;

		// get the next position
		int nextx = curx, nexty = cury;
		if (direction == KBLEFT || direction == KBRIGHT)
			nextx += direction == KBLEFT ? -1 : 1;
		else if (direction == KBUP || direction == KBDOWN)
			nexty += direction == KBUP ? -1 : 1;

		// can we not move any more?
		if (nextx < 0 || nextx >= size_x || nexty < 0 || nexty >= size_y)
			break;

		// is there a wall here?
		if ((direction == KBUP && board[nexty][nextx].contents & HAS_BOTTOM_WALL)
			|| (direction == KBDOWN && board[nexty][nextx].contents & HAS_TOP_WALL)
			|| (direction == KBLEFT && board[nexty][nextx].contents & HAS_RIGHT_WALL)
			|| (direction == KBRIGHT && board[nexty][nextx].contents & HAS_LEFT_WALL))
			break;

		// is there a different piece already there?
		if (board[nexty][nextx].contents & HAS_PIECE && board[nexty][nextx].piece != r->piece)
		{
			// did we win? @todo abstract out into event handler
			if (board[nexty][nextx].piece == winningPos)
			{
				if (robot == winningRobot)
					return WIN;

				// if some other robot made it to the winning position,
				// embed the letter's byte into contents and then
				// let the robot take its place.
				board[nexty][nextx].contents |= HAS_ENCODED_LETTER | (board[nexty][nextx].piece << (sizeof(int) - 1) * 8);
				curx = nextx;
				cury = nexty;
			}
			break;
		}

		// keep going
		curx = nextx;
		cury = nexty;
	}

	// check if we have moved any
	if (curx != x || cury != y)
	{
		// if we have, then swap the positions
		board[cury][curx].piece = r->piece;
		board[cury][curx].contents |= HAS_PIECE;

		// for the case of encoded letters, replace with the original value
		if (board[y][x].contents & HAS_ENCODED_LETTER)
		{
			// get the letter
			board[y][x].piece = board[y][x].contents >> (sizeof(int) - 1) * 8;

			// "remove" the embedded letter
			board[y][x].contents &= ~HAS_ENCODED_LETTER & 0x00FFFFFF;
		}
		else
			board[y][x].contents &= ~HAS_PIECE;
	}

	return NORMAL;
}


// This function is called every frame. When 0 is returned, the game exits.
int my_game()
{
	// get key
	KBKEY k = get_arrow_key();

	// process input
	switch (k)
	{
	case KBUP:
	case KBDOWN:
	case KBLEFT:
	case KBRIGHT:
	{
		if (move_robot(cur_robot, k, board, board_size, board_size, dest_letter, origin_robot) == WIN)
			return 0;

		// write to buffer
		write_board(board, buffer, board_size, board_size);

		// find the widget
		char rc = '0' + cur_robot;
		char id[] = { rc, 0 };
		Widget robot = get_widget_by_id(id);

		// find the robot in the buffer
		int x = board_size, y = board_size;
		calc_board_display_sizes(x, y);

		for (int i = 0; i < y; i++)
			for (int j = 0; j < x; j++)
			{
				if (buffer[i][j] == rc)
				{
					set_position(robot, ox + j * squareSideLength, oy + i * squareSideLength - IdOffset);
					return 1;
				}
			}

		break;
	}

	default:
		break;
	}

	return 1;
}


// starting point
int main(int argc, char **argv)
{
	// initialize the game window
	create_window("Program 3", 1024, 768);

	// change the window's background color (RGB / red green blue)
	set_window_background_color(0, 0, 179);

	// you need to load a font in order to use it.
	// here, we will load a font from the fonts folder.
	load_font("./fonts/ClassicRobotBoldItalic.ttf");


	//////////////////////////////////////////////////////////////////
	// Let's add some title text
	//////////////////////////////////////////////////////////////////

	// add some text
	Widget titleText = create_text("Program 3: Robots");

	// set the font by its name
	set_font(titleText, "Classic Robot");

	// Make it bold and italic
	set_text_style(titleText, FONT_BOLD_AND_ITALIC);

	// Make it big (imagine using a word processor)
	set_text_size(titleText, 28);

	// Now position it on the screen
	set_position(titleText, 400, 30);

	// Now set the text box's width. This is different from set_text_size().
	// A word processor analogy: imagine that you create a text box in Word
	// with some text in it. Above, set_text_size() will change the font size,
	// while set_width() below will resize the text box itself. In other words,
	// you change the width and height of the text box to word wrap the text.
	set_width(titleText, 400);

	// Make the text color white
	set_text_color(titleText, "white");


	//////////////////////////////////////////////////////////////////
	// Let's add an exit button
	//////////////////////////////////////////////////////////////////

	Widget exitButton = create_button("X (Exit)");

	// Set its width/height
	set_width(exitButton, 70);
	set_height(exitButton, 20);

	// Now position it on the screen
	set_position(exitButton, 14, 30);

	// Set the background color
	set_background_color(exitButton, "red");

	// Set the text color
	set_text_color(exitButton, "white");

	// Center the text inside
	set_text_center(exitButton);

	// Now we will bind the onExitButtonClicked function to the button's "click" event
	bind_event(exitButton, "click", onExitButtonClicked);


	//////////////////////////////////////////////////////////////////
	// To demo the UI
	//////////////////////////////////////////////////////////////////

	// Creating a text box example
	Widget textbox = create_form("text");

	// Set its text
	set_text(textbox, "Enter your name...");

	// Make it *look* like a text box
	set_border(textbox, "black", 1); // give it a black border that's 1px wide
	set_background_color(textbox, "#4d79ff"); // give it a blue-ish background (in hex)
	set_text_color(textbox, "white"); // give the text inside a white color
	set_height(textbox, 28);
	set_width(textbox, 200);

	// Move it down a bit and give it some padding
	set_position(textbox, 140, 30);
	set_prop(textbox, "padding", "3px");


	//////////////////////////////////////////////////////////////////
	// Let's implement the actual game
	//////////////////////////////////////////////////////////////////

	// seed
	srand(time(NULL));

	// note that we can have a non-square board if we wanted.
	board = alloc_board(board_size, board_size);

	// init board
	int maxlines;
	char** lines = parse_data("data.txt", board, board_size, maxlines, dest_letter, origin_robot);

	// the currently selected robot
	cur_robot = origin_robot;

	// The goal is to minimally modify the original console program

	buffer = alloc_board_display(board_size, board_size);
	write_board(board, buffer, board_size, board_size);


	//////////////////////////////////////////////////////////////////
	// Set up the page background
	//////////////////////////////////////////////////////////////////

	//Widget background = create_image("background.jpg");
	//set_layer(background, -10); // Move the background to.. well, some negative layer so it's out of the way.


	//////////////////////////////////////////////////////////////////
	// Load the walls and other info
	//////////////////////////////////////////////////////////////////

	// loop through the board and add the walls
	load_board(buffer, board_size, board_size, ox, oy, squareSideLength);

	// select the default robot
	selectRobot(cur_robot);


	Widget robotInfo = create_text(Rocket::Core::String(100, "Move Robot #%d to %c to win!", origin_robot, dest_letter).CString());
	set_text_size(robotInfo, 18);
	set_text_style(robotInfo, FONT_BOLD);
	set_text_color(robotInfo, "#6AFF59");
	set_position(robotInfo, 400, 80);


	//////////////////////////////////////////////////////////////////
	// Let's start the game
	//////////////////////////////////////////////////////////////////

	// start the game
	StartGame(my_game);

	// cleanup
	free_board(board, board_size);
	free_board_display(buffer, board_size);
	free_lines_array(lines, maxlines);

	// exit at end
	return 0;
};