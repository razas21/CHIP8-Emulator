#include <string>
using namespace std;
#include <stdio.h>
#include <SDL.h>
#include <SDL_image.h>
#include <iostream>
#include <fstream>

unsigned char memory[4096] = {0}; //4096 bytes of RAM
char registers[16]; //16 8-bit general purpose registers
char delay_timer; //is decremented at a rate of 60Hz until it reaches 0
char sound_timer; //gives off a beeping sound as long as it’s not 0

int PC; //program counter - stores the currently executing address
char SP; //stack counter - used to point to the topmost level of the stack.
int I; //index register - points at locations in memory
int stack[16]; //Used to store the address that the interpreter should return to when finished with a subroutine

int frequency = 700;
int display[32][64] = { 0 }; //temporary display before SDL implemented 

int initializeFont() {
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
	char * font[16] = {f0,f1,f2,f3,f4,f5,f6,f7,f8,f9,fA,fB,fC,fD,fE,fF};

	for (int i = 0; i < 16; i++) {
		for (int j = 0; j < 5; j++) {
			memory[startAddress + i*5 + j] = font[i][j];
		} 
	}
	return startAddress;
}

int loadRom(string filename) {
	ifstream rom_file;
	char mychar;
	int memoryAddress = 0x200; //program starts at 0x200 

	rom_file.open(filename);
	if (rom_file.is_open()) {
		while (rom_file.good()) {
			mychar = rom_file.get();
			memory[memoryAddress] = mychar;
			memoryAddress = memoryAddress + 1;
		}
	}
	int sizeOfRom = memoryAddress - 0x200;
	rom_file.close();
	return sizeOfRom;
}

// Instruction Methods:

int instruction_jump(char secondNibble, char thirdNibble, char fourthNibble) {
	PC = (secondNibble << 8) + (thirdNibble << 4) + (fourthNibble);
	return PC;
}

int instruction_set(char register_num, char thirdNibble, char fourthNibble) {
	registers[register_num] = (thirdNibble << 4) + fourthNibble;
	return 0;
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
	char x = registers[x_coor];
	char y = registers[y_coor];

	// Create a surface for the screen
	const int SCREEN_WIDTH = 64;
	const int SCREEN_HEIGHT = 32;
	SDL_Window* window = SDL_CreateWindow("CHIP-8 Display", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH * 10, SCREEN_HEIGHT * 10, SDL_WINDOW_SHOWN);
	SDL_Surface* surface = SDL_GetWindowSurface(window);
	SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 0, 0, 0)); // Fill the surface with black

	// starting at x200, read bit of byte and set it to coordinate
		// move on to next byte and y coordinate
	for (int row = 0; row < length; row++) {
		for (int col = 0; col < 8; col++) {
			char current_bit = (((memory[I + (row)] << col) & 0x80) >> 7);
			display[x + col][y + row] = current_bit;
		}
	}

	// Draw the entire display to the surface
	for (int x = 0; x < SCREEN_WIDTH; x++) {
		for (int y = 0; y < SCREEN_HEIGHT; y++) {
			SDL_Rect pixelRect = { x * 10, y * 10, 10, 10 }; // Create a rectangle for the pixel
			if (display[x][y] == 1) {
				SDL_FillRect(surface, &pixelRect, SDL_MapRGB(surface->format, 255, 255, 255)); // Draw a white pixel
			}
			else {
				SDL_FillRect(surface, &pixelRect, SDL_MapRGB(surface->format, 0, 0, 0)); // Draw a black pixel
			}
		}
	}

	// Blit the surface to the window
	SDL_UpdateWindowSurface(window);

	return 0;
}

int decodeInstruction(int instruction) {
	char firstNibble = instruction >> 12 & 0xF;
	char secondNibble = instruction >> 8 & 0xF;
	char thirdNibble = instruction >> 4 & 0xF;
	char fourthNibble = instruction & 0xF;

	switch (firstNibble) 
	{
	case 0x0:
		switch (thirdNibble)
		{
		case 0xE:
			switch (fourthNibble) {
			case 0x0: //00E0
				break;
			case 0xE: //00EE
				break;
			}
		default:
			break;
		}
		break;

	case 0x1:
		instruction_jump(secondNibble, thirdNibble, fourthNibble);
		break;

	case 0x6:
		instruction_set(secondNibble, thirdNibble, fourthNibble);
		break;
	
	case 0x7:
		instruction_add(secondNibble, thirdNibble, fourthNibble);
		break;

	case 0xA:
		instruction_set_index(secondNibble, thirdNibble, fourthNibble);
		break;

	case 0xD:
		instruction_draw(secondNibble, thirdNibble, fourthNibble);

	}

	return 0;
}


int main(int argc, char* argv[]) {
	initializeFont();
	int sizeOfRom = loadRom("IBMLogo.ch8");

	int startAddress = 0x200;
	int PC = startAddress;
	int instruction;

	while (1) {
		while (PC < startAddress + sizeOfRom) {
				// Fetch instruction
				instruction = (memory[PC]<<8) + memory[PC + 1];
				PC = PC + 2; //next instruction

				// Decode instruction
				// cout << instruction << " ";
				decodeInstruction(instruction);
		

			}
	}
	
		// Fetch instuction
		// Decode instruciton
		// Execute intruction
	return 0;
}

