#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <SDL.h>
#include <SDL_image.h>
#include <stack>
#include <random>
#include <ctime>

using namespace std;

unsigned char memory[4096] = {0}; //4096 bytes of RAM
unsigned char registers[16] = {0}; //16 8-bit general purpose registers
char delay_timer; //is decremented at a rate of 60Hz until it reaches 0
char sound_timer; //gives off a beeping sound as long as it’s not 0

int PC = 0; //program counter - stores the currently executing address
char SP = 0; //stack counter - used to point to the topmost level of the stack.
int I = 0; //index register - points at locations in memory
stack<int> routine_stack; //Used to store the address that the interpreter should return to when finished with a subroutine

int frequency = 700;
const char* keyPressed = nullptr;
const int SCREEN_WIDTH = 64;
const int SCREEN_HEIGHT = 32;
int display[SCREEN_HEIGHT][SCREEN_WIDTH] = { 0 }; 

SDL_Window* window;
SDL_Surface* surface;

bool initDisplay() {
	//Initialization flag
	bool success = true;

	//Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
		success = false;
	}
	else
	{
		//Create window
		window = SDL_CreateWindow("CHIP-8 Display", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH * 10, SCREEN_HEIGHT * 10, SDL_WINDOW_SHOWN);
		if (window == NULL)
		{
			printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
			success = false;
		}
		else
		{
			//Get window surface
			surface = SDL_GetWindowSurface(window);
		}
	}

	return success;
}

int initFont() {
	char f0[] = { (char)0xF0, (char)0x90, (char)0x90, (char)0x90, (char)0xF0 }; // 0
	char f1[] = { (char)0x20, (char)0x60, (char)0x20, (char)0x20, (char)0x70 }; // 1
	char f2[] = { (char)0xF0, (char)0x10, (char)0xF0, (char)0x80, (char)0xF0 }; // 2
	char f3[] = { (char)0xF0, (char)0x10, (char)0xF0, (char)0x10, (char)0xF0 }; // 3
	char f4[] = { (char)0x90, (char)0x90, (char)0xF0, (char)0x10, (char)0x10 }; // 4
	char f5[] = { (char)0xF0, (char)0x80, (char)0xF0, (char)0x10, (char)0xF0 }; // 5
	char f6[] = { (char)0xF0, (char)0x80, (char)0xF0, (char)0x90, (char)0xF0 }; // 6
	char f7[] = { (char)0xF0, (char)0x10, (char)0x20, (char)0x40, (char)0x40 }; // 7
	char f8[] = { (char)0xF0, (char)0x90, (char)0xF0, (char)0x90, (char)0xF0 }; // 8
	char f9[] = { (char)0xF0, (char)0x90, (char)0xF0, (char)0x10, (char)0xF0 }; // 9
	char fA[] = { (char)0xF0, (char)0x90, (char)0xF0, (char)0x90, (char)0x90 }; // A
	char fB[] = { (char)0xE0, (char)0x90, (char)0xE0, (char)0x90, (char)0xE0 }; // B
	char fC[] = { (char)0xF0, (char)0x80, (char)0x80, (char)0x80, (char)0xF0 }; // C
	char fD[] = { (char)0xE0, (char)0x90, (char)0x90, (char)0x90, (char)0xE0 }; // D
	char fE[] = { (char)0xF0, (char)0x80, (char)0xF0, (char)0x80, (char)0xF0 }; // E
	char fF[] = { (char)0xF0, (char)0x80, (char)0xF0, (char)0x80, (char)0x80 }; // F

	int startAddress = 0x50;
	char* font[16] = { f0,f1,f2,f3,f4,f5,f6,f7,f8,f9,fA,fB,fC,fD,fE,fF };

	for (int i = 0; i < 16; i++) {
		for (int j = 0; j < 5; j++) {
			memory[startAddress + i * 5 + j] = font[i][j];
		}
	}
	return startAddress;
}

char decode_keypress(char keyPress) {
	switch (keyPress) {
	case '0':
		return 0x0;
	case '1':
		return 0x1;
	case '2':
		return 0x2;
	case '3':
		return 0x3;
	case '4':
		return 0x4;
	case '5':
		return 0x5;
	case '6':
		return 0x6;
	case '7':
		return 0x7;
	case '8':
		return 0x8;
	case '9':
		return 0x9;
	case 'A':
		return 0xA;
	case 'B':
		return 0xB;
	case 'C':
		return 0xC;
	case 'D':
		return 0xD;
	case 'E':
		return 0xE;
	case 'F':
		return 0xF;
	default: 
		//Any non-keypad (hexadecimal) key
		return 'N';
		break;
	}
}

void destroyDisplay()
{
	//Deallocate surface
	SDL_FreeSurface(surface);
	surface = NULL;

	//Destroy window
	SDL_DestroyWindow(window);
	window = NULL;

	//Quit SDL subsystems
	SDL_Quit();
}

int loadRom(string filename) {
	ifstream rom_file;
	int memoryAddress = 0x200; // program starts at 0x200
	string filePath = "Roms/" + filename;

	rom_file.open(filePath, ios::binary | ios::ate);
	if (rom_file.is_open()) {
		streampos size = rom_file.tellg(); // get file size
		rom_file.seekg(0, ios::beg); // move to the beginning of the file
		rom_file.read(reinterpret_cast<char*>(memory + memoryAddress), size); // read all characters from the file
		rom_file.close();
		return size;
	}
	else {
		return -1; // file not found
	}
}

// Instruction Methods:

int instruction_clear() {
	for (int i = 0; i < 32; i++) {
		for (int j = 0; j < 64; j++) {
			display[i][j] = 0;
		}
	}

	//// Create a surface for the screen
	//SDL_Surface* surface = SDL_GetWindowSurface(window);
	SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 0, 0, 0)); // Fill the surface with black

	// Draw the entire display to the surface
	for (int x = 0; x < SCREEN_WIDTH; x++) {
		for (int y = 0; y < SCREEN_HEIGHT; y++) {
			SDL_Rect pixelRect = { x * 10, y * 10, 10, 10 }; // Create a rectangle for the pixel
			SDL_FillRect(surface, &pixelRect, SDL_MapRGB(surface->format, 255, 255, 255)); // Draw a white pixel
		}
	}
	// Blit the surface to the window
	SDL_UpdateWindowSurface(window);
	return 0;
}

int instruction_subroutine(char secondNibble, char thirdNibble, char fourthNibble) {
	int curr_routine = PC;
	routine_stack.push(curr_routine);
	PC = (secondNibble << 8) + (thirdNibble << 4) + (fourthNibble);
	return PC;
}

int instruction_subroutine_return() {
	if (routine_stack.empty()) {
		return 0;
	}
	PC = routine_stack.top();
	routine_stack.pop();
	return PC;
}

int instruction_jump(char secondNibble, char thirdNibble, char fourthNibble) {
	PC = (secondNibble << 8) + (thirdNibble << 4) + (fourthNibble);
	return PC;
}

int instruction_jump_w_offset(char secondNibble, char thirdNibble, char fourthNibble) {
	int value = ((secondNibble << 8) + (thirdNibble << 4) + (fourthNibble)) + registers[0];
	PC = value;
	return PC;
}

int instruction_set(char register_num, char thirdNibble, char fourthNibble) {
	registers[register_num] = (thirdNibble << 4) + fourthNibble;
	return 0;
}

void instruction_skip() {
	PC = PC + 2;
}

int instruction_add(char register_num, char thirdNibble, char fourthNibble) {
	char additive = (thirdNibble << 4) + fourthNibble;
	registers[register_num] += additive;
	return 0;
}

int instruction_set_index(char secondNibble, char thirdNibble, char fourthNibble) {
	I = (secondNibble << 8) + (thirdNibble << 4) + (fourthNibble);
	return I;
}

int instruction_bitshift_right(char secondNibble, char thirdNibble) {
	registers[secondNibble] = registers[thirdNibble];
	char bit_shifted = registers[secondNibble] & 1;
	if (bit_shifted == 1) {
		registers[15] = 1;
	}
	else {
		registers[15] = 0;
	}
	registers[secondNibble] = registers[secondNibble] >> 1;
	return 0;
}

int instruction_bitshift_left(char secondNibble, char thirdNibble) {
	registers[secondNibble] = registers[thirdNibble];
	char bit_shifted = registers[secondNibble] & 128;
	if (bit_shifted == 1) {
		registers[15] = 1;
	}
	else {
		registers[15] = 0;
	}
	registers[secondNibble] = registers[secondNibble] << 1;
	return 0;
}
int instruction_store(char secondNibble) {
	for (int i = 0; i <= secondNibble; i++) {
		memory[I + i] = registers[i];
	}
	return 0;
}

int instruction_load(char secondNibble) {
	for (int i = 0; i <= secondNibble; i++) {
		registers[i] = memory[I + i];
	}
	return 0;
}

int instruction_dec_convert(char secondNibble) {
	int num = registers[secondNibble];

	unsigned char hundreds = num / 100;    // Calculate the hundreds digit
	unsigned char tens = (num / 10) % 10;  // Calculate the tens digit
	unsigned char units = num % 10;        // Calculate the units digit
	
	memory[I] = hundreds;
	memory[I + 1] = tens;
	memory[I + 2] = units;

	return I;
}

int instruction_add_to_index(char secondNibble) {
	int value = registers[secondNibble];
	I = I + value;
	return I;
}

int instruction_font_char(char secondNibble) {
	int startAddress = 0x50;
	char _char = registers[secondNibble] & 0xF;
	I = memory[startAddress + _char * 5];
	return 0;
}

int instruction_random(char secondNibble, char thirdNibble, char fourthNibble) {
	// Create a random number generator engine
	random_device rd;
	mt19937 gen(rd());
	uniform_int_distribution<int> dis(0, 99);

	// Generate a random number
	int randomNum = dis(gen);
	
	randomNum = randomNum & ((thirdNibble << 4) + (fourthNibble));
	registers[secondNibble] = randomNum;
	return 0;
}

int instruction_get_key(char secondNibble) {
	if (keyPressed == nullptr) {
		instruction_skip();
	}
	else {
		char keyPress = decode_keypress((char)*keyPressed);
		if (keyPress == 'N') {
			instruction_skip();
		}
		else {
			registers[secondNibble] = keyPress;
		}
	}
	return 0;
}

int instruction_draw(char x_coor, char y_coor, char n) {
	int sprite_start = I; //memory address where sprite begins
	char length = n;
	char x = (registers[x_coor]); //x coordinate where sprite will start
	char y = (registers[y_coor]); //y coordinate where sprite will start 
	char x__coor = 0;
	char y__coor = 0;

	// Create a surface for the screen
	SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 0, 0, 0)); // Fill the surface with black

	// starting at x200, read bit of byte and set it to coordinate
	for (int row = 0; row < length; row++) {
		// move on to next byte and y coordinate
		for (int col = 0; col < 8; col++) {
			char current_bit = (((memory[I + (row)] << col) & 0x80) >> 7);
			current_bit = current_bit ^ (display[y + row][x + col]);
			if ((display[y + row][x + col] == 1) && (current_bit == 0)) {
				registers[15] = 1;
			}
			x_coor = (x + col) & 0b01111111; //clips at 64
			y_coor = (y + row) & 0b00111111; //clips at 32
			display[y_coor][x_coor] = current_bit;
		}
	}

	// Draw the entire display to the surface
	for (int x = 0; x < SCREEN_WIDTH; x++) {
		for (int y = 0; y < SCREEN_HEIGHT; y++) {
			SDL_Rect pixelRect = { x * 10, y * 10, 10, 10 }; // Create a rectangle for the pixel
			if (display[y][x] == 1) {
				SDL_FillRect(surface, &pixelRect, SDL_MapRGB(surface->format, 255, 255, 255)); // Draw a white pixel 	
			}
			else {
				SDL_GetError();
				SDL_FillRect(surface, &pixelRect, SDL_MapRGB(surface->format, 0, 0, 0)); // Draw a black pixel
			}
		}
	}
	// Blit the surface to the window
	SDL_UpdateWindowSurface(window);
	return 0;
}


int decode_instruction(int instruction, int PC) {
	unsigned char firstNibble = (instruction & 0xF000) >> 12;
	unsigned char secondNibble = (instruction & 0x0F00) >> 8;
	unsigned char thirdNibble = (instruction & 0x00F0) >> 4;
	unsigned char fourthNibble = (instruction & 0x000F);
	int sum = 0, difference = 0;
	char key = 0;

	//printf("%x%x%x%x\n", firstNibble, secondNibble, thirdNibble, fourthNibble);

   	switch (firstNibble)
	{
	case 0x0:
		switch (thirdNibble)
		{
		case 0xE:
			switch (fourthNibble) {
			case 0x0: //00E0
				instruction_clear();
				break;
			case 0xE: //00EE
				instruction_subroutine_return();
				break;
			}
		default:
			break;
		}
		break;

	case 0x1:
		instruction_jump(secondNibble, thirdNibble, fourthNibble);
		break;

	case 0x2:
		instruction_subroutine(secondNibble, thirdNibble, fourthNibble);
		break;

	case 0x3:
		sum = (thirdNibble << 4) + fourthNibble;
		if (registers[secondNibble] == sum) {
			instruction_skip();
		}
		break;

	case 0x4:
		if (registers[secondNibble] != ((thirdNibble << 4) + fourthNibble)) {
			instruction_skip();
		}
		break;

	case 0x5:
		if (registers[secondNibble] == registers[thirdNibble]) {
			instruction_skip();
		}
		break;

	case 0x6:
		instruction_set(secondNibble, thirdNibble, fourthNibble);
		break;

	case 0x7:
		instruction_add(secondNibble, thirdNibble, fourthNibble);
		break;

	case 0x8:
		switch (fourthNibble)
		{
		case 0x0:
			registers[secondNibble] = registers[thirdNibble];
			break;
		case 0x1:
			registers[secondNibble] = registers[secondNibble] | registers[thirdNibble];
			break;
		case 0x2:
			registers[secondNibble] = registers[secondNibble] & registers[thirdNibble];
			break;
		case 0x3:
			registers[secondNibble] = registers[secondNibble] ^ registers[thirdNibble];
			break;
		case 0x4:
			sum = (registers[secondNibble] + registers[thirdNibble]);
			registers[secondNibble] = sum;
			if (sum > 255) {
				registers[15] = 1;
			}
			else {
				registers[15] = 0;
			}
			break;

		case 0x5:
			difference = registers[secondNibble] - registers[thirdNibble];
			if (registers[secondNibble] > registers[thirdNibble]) {
				registers[15] = 1;
			}
			else {
				registers[15] = 0;
			}
			registers[secondNibble] = difference;
			break;

		case 0x6:
			instruction_bitshift_right(secondNibble, thirdNibble);
			break;

		case 0x7:
			difference = registers[thirdNibble] - registers[secondNibble];

			if (registers[thirdNibble] > registers[secondNibble]) {
				registers[15] = 1;
			}
			else {
				registers[15] = 0;
			}
			registers[secondNibble] = difference;
			break;

		case 0xe:
			instruction_bitshift_left(secondNibble, thirdNibble);
			break;

		default:
			break;
		}
		break;

	case 0x9:
		if (registers[secondNibble] != registers[thirdNibble]) {
			instruction_skip();
		}
		break;

	case 0xA:
		instruction_set_index(secondNibble, thirdNibble, fourthNibble);
		break;
	case 0xB:
		instruction_jump_w_offset(secondNibble, thirdNibble, fourthNibble);
		break;
	case 0xC:
		instruction_random(secondNibble, thirdNibble, fourthNibble);
		break;
	case 0xD:
		instruction_draw(secondNibble, thirdNibble, fourthNibble);
		break;
	case 0xE:
		switch (thirdNibble)
		{
		case 0x9: //EX9E
			if (keyPressed != NULL) {
				key = decode_keypress((char) *keyPressed);
				if (key == registers[secondNibble]) {
					instruction_skip();
				}
			}
			break;
		case 0xA: //EXA1
			if (keyPressed != NULL) {
				key = decode_keypress((char) *keyPressed);
				if (key != registers[secondNibble]) {
					instruction_skip();
				}
			}
			break;
		}
	case 0xF:
		switch (thirdNibble)
		{
		case 0x0:
			instruction_get_key(secondNibble);
			break;
		case 0x1:
			instruction_add_to_index(secondNibble);
			break;
		case 0x2:
			instruction_font_char(secondNibble);
			break;
		case 0x3:
			instruction_dec_convert(secondNibble);
			break;
		case 0x5:
			instruction_store(secondNibble);
			break;
		case 0x6:
			instruction_load(secondNibble);
			break;
		default:
			break;
		}
	}

	return 0;
}


int main(int argc, char* argv[]) {
	int startAddress = 0x200;
	PC = startAddress;
	int instruction = 0;
	double delay;

	//int sizeOfRom = loadRom("IBMLogo.ch8");
	//int sizeOfRom = loadRom("3-corax+.ch8");
	//int sizeOfRom = loadRom("test_opcode.ch8");
	//int sizeOfRom = loadRom("4-flags.ch8");
	//int sizeOfRom = loadRom("5-quirks.ch8");
	//int sizeOfRom = loadRom("6-keypad.ch8");
	//int sizeOfRom = loadRom("Tetris [Fran Dachille, 1991].ch8");
	//int sizeOfRom = loadRom("pong.ch8");
	int sizeOfRom = loadRom("Space Invaders [David Winter].ch8");

	initFont();
	if (!initDisplay()) {
		printf("Failed to initialize display\n");
	}
	else {
		bool quit = false; //Main loop flag
		SDL_Event e; //Event handler
		//While application is running
		while (!quit)
		{
			//Handle events on queue
			while (SDL_PollEvent(&e) != 0)
			{
				//User requests quit
				if (e.type == SDL_QUIT) 
				{
					quit = true;
				}
				else if (e.type == SDL_KEYDOWN) {
					SDL_Keycode keyCode = e.key.keysym.sym;
					keyPressed = SDL_GetKeyName(keyCode);
				}
			}
			if (keyPressed != nullptr) {
				cout << "\nKey: " << keyPressed << "\n";
			}
			if (PC < startAddress + sizeOfRom) {
				// Delay
				delay = 0.012;
				delay *= CLOCKS_PER_SEC;
				clock_t now = clock();
				while (clock() - now < delay);
				// Fetch instruction
				instruction = (memory[PC] << 8) + (memory[PC + 1]);
				cout << PC << " ";
				PC = PC + 2; //next instruction
 				// Decode instruction
				decode_instruction(instruction, PC);
				keyPressed = nullptr;
			}
		}
		destroyDisplay();
	}
	return 0;
}

