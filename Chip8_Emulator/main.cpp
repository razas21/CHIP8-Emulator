#include <string>
using namespace std;
#include <stdio.h>
#include <SDL.h>
#include <SDL_image.h>
#include <iostream>
#include <fstream>
#include <stack>


unsigned char memory[4096] = { 0 }; //4096 bytes of RAM
unsigned char registers[16]; //16 8-bit general purpose registers
char delay_timer; //is decremented at a rate of 60Hz until it reaches 0
char sound_timer; //gives off a beeping sound as long as it’s not 0

int PC; //program counter - stores the currently executing address
char SP; //stack counter - used to point to the topmost level of the stack.
int I; //index register - points at locations in memory
stack<int> routine_stack; //Used to store the address that the interpreter should return to when finished with a subroutine

int frequency = 700;
int display[32][64] = { 0 }; 

const int SCREEN_WIDTH = 64;
const int SCREEN_HEIGHT = 32;

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

	rom_file.open(filename, ios::binary | ios::ate);
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
	SDL_Surface* surface = SDL_GetWindowSurface(window);
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
	routine_stack.push(PC);
	PC = (secondNibble << 8) + (thirdNibble << 4) + (fourthNibble);
	return PC;
}

int instruction_subroutine_return() {
	PC = routine_stack.top();
	routine_stack.pop();
	return PC;
}

int instruction_jump(char secondNibble, char thirdNibble, char fourthNibble) {
	PC = (secondNibble << 8) + (thirdNibble << 4) + (fourthNibble);
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
	registers[register_num] = registers[register_num] + additive;
	return 0;
}

int instruction_set_index(char secondNibble, char thirdNibble, char fourthNibble) {
	I = (secondNibble << 8) + (thirdNibble << 4) + (fourthNibble);
	return I;
}

int instruction_draw(char x_coor, char y_coor, char n) {
	int sprite_start = I; //memory address where sprite begins
	char length = n;
	char x = (registers[x_coor]) & 0b00111111; //clips at 64
	char y = (registers[y_coor]) & 0b00011111; //clips at 32

	// Create a surface for the screen
	SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 255, 255, 255)); // Fill the surface with black

	// starting at x200, read bit of byte and set it to coordinate
	for (int row = 0; row < length; row++) {
		// move on to next byte and y coordinate
		for (int col = 0; col < 8; col++) {
			char current_bit = (((memory[I + (row)] << col) & 0x80) >> 7);
			current_bit = current_bit ^ (display[x + col][y + row]);
			if ((display[x + col][y + row] == 1) && (current_bit == 0)) {
				registers[15] = 1;
			}
			display[x + col][y + row] = current_bit;
		}
	}

	// Draw the entire display to the surface
	for (int x = 0; x < SCREEN_WIDTH; x++) {
		for (int y = 0; y < SCREEN_HEIGHT; y++) {
			SDL_Rect pixelRect = { x * 10, y * 10, 10, 10 }; // Create a rectangle for the pixel
			if (display[x][y] == 1) {
				SDL_FillRect(surface, &pixelRect, SDL_MapRGB(surface->format, 0, 0, 0)); // Draw a black pixel (inverted display)	
			}
			else {
				SDL_GetError();
				SDL_FillRect(surface, &pixelRect, SDL_MapRGB(surface->format, 255, 255, 255)); // Draw a white pixel
			}
		}
	}

	// Blit the surface to the window
	SDL_UpdateWindowSurface(window);

	return 0;
}

int instruction_draw_debug(char x_coor, char y_coor, char n) {
	int sprite_start = I; //memory address where sprite begins
	char length = n;
	char x = registers[x_coor];
	char y = registers[y_coor];

	// starting at x200, read bit of byte and set it to coordinate
	for (int row = 0; row < length; row++) {
		// move on to next byte and y coordinate
		for (int col = 0; col < 8; col++) {
			char current_bit = (((memory[I + (row)] << col) & 0x80) >> 7);
			current_bit = current_bit ^ (display[x + col][y + row]);
			if ((display[x + col][y + row] == 1) && (current_bit == 0)) {
				registers[15] = 1;
			}
			display[x + col][y + row] = current_bit;
		}
	}

	char pixel = 0;
	for (int x = 0; x < 32; x++) {
		for (int y = 0; y < 64; y++) {
			if (display[y][x] == 1) {
				pixel = '*';
			}
			else {
				pixel = ' ';
			}
			cout << pixel;
		}
	}
	return 0;
}

int decode_instruction(int instruction) {
	unsigned char firstNibble = (instruction & 0xF000) >> 12;
	unsigned char secondNibble = (instruction & 0x0F00) >> 8;
	unsigned char thirdNibble = (instruction & 0x00F0) >> 4;
	unsigned char fourthNibble = (instruction & 0x000F);
	int sum, difference;

	printf("%x%x%x%x\n", firstNibble, secondNibble, thirdNibble, fourthNibble);

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
		
		case 0x7:
			difference = registers[thirdNibble] - registers[secondNibble];
			
			if (registers[thirdNibble] > registers[secondNibble]) {
				registers[15] = 1;
			}
			else {
				registers[15] = 0;
			}
			registers[secondNibble] = difference;

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

	case 0xD:
		instruction_draw_debug(secondNibble, thirdNibble, fourthNibble);
		break;
	}

	return 0;
}


int main(int argc, char* argv[]) {
	int startAddress = 0x200;
	int PC = startAddress;
	int instruction;

	//int sizeOfRom = loadRom("IBMLogo.ch8");
	//int sizeOfRom = loadRom("3-corax+.ch8");
	int sizeOfRom = loadRom("test_opcode.ch8");
	//int sizeOfRom = loadRom("Chip8_Picture.ch8");


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
			}
			if (PC < startAddress + sizeOfRom) {
				// Fetch instruction
				instruction = (memory[PC] << 8) + (memory[PC + 1]);
				PC = PC + 2; //next instruction

				// Decode instruction
				decode_instruction(instruction);
			}

		}
		destroyDisplay();
	}

	return 0;
}

