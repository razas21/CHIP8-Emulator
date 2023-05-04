#include <stdio.h>
#include <SDL.h>
#include <SDL_image.h>
#include <iostream>

#define WIDTH 1280
#define HEIGHT 720

int display(int argc, char* argv[]) {
	SDL_Window* window = nullptr;
	SDL_Surface* windowSurface = nullptr;
	SDL_Surface* imageSurface = nullptr;

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		std::cout << "Video Initialization Error" << SDL_GetError() << std::endl;
	else {
		window = SDL_CreateWindow("Test Title", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN);
		if (window == NULL)
			std::cout << "Window creation error: " << SDL_GetError() << std::endl;
		else {

			// Window Created 
			windowSurface = SDL_GetWindowSurface(window);
			imageSurface = SDL_LoadBMP("bitmap_test.bmp");

			if (imageSurface == NULL)
				std::cout << "Image loading Error: " << SDL_GetError() << std::endl;
			else {
				SDL_BlitSurface(imageSurface, NULL, windowSurface, NULL);

				// Freeing up memory
				SDL_FreeSurface(imageSurface);
				imageSurface = nullptr;
				window = nullptr;
				windowSurface = nullptr;
				SDL_UpdateWindowSurface(window);
				SDL_Delay(3000);
			}


		}
	}


	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}