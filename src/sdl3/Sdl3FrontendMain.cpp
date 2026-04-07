#include <SDL3/SDL.h>

#ifdef X88000_SDL3_HAS_IMGUI

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#endif

#include <stdio.h>

int main(int, char**) {
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO)) {
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}

	SDL_Window* pWindow = SDL_CreateWindow(
		"X88000 SDL3 Frontend (Prototype)",
		1024,
		768,
		0);
	if (pWindow == NULL) {
		fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		SDL_Quit();
		return 1;
	}

	SDL_Renderer* pRenderer = SDL_CreateRenderer(pWindow, NULL);
	if (pRenderer == NULL) {
		fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
		SDL_DestroyWindow(pWindow);
		SDL_Quit();
		return 1;
	}

#ifdef X88000_SDL3_HAS_IMGUI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;
	ImGui::StyleColorsDark();
	ImGui_ImplSDL3_InitForSDLRenderer(pWindow, pRenderer);
	ImGui_ImplSDLRenderer3_Init(pRenderer);
#endif

	bool bRunning = true;
	while (bRunning) {
		SDL_Event evt;
		while (SDL_PollEvent(&evt)) {
#ifdef X88000_SDL3_HAS_IMGUI
			ImGui_ImplSDL3_ProcessEvent(&evt);
#endif
			if (evt.type == SDL_EVENT_QUIT) {
				bRunning = false;
			}
			if ((evt.type == SDL_EVENT_KEY_DOWN) &&
				(evt.key.key == SDLK_ESCAPE))
			{
				bRunning = false;
			}
		}

#ifdef X88000_SDL3_HAS_IMGUI
		ImGui_ImplSDLRenderer3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
		ImGui::Begin("Status");
		ImGui::TextUnformatted("SDL3 + ImGui frontend bootstrap");
		ImGui::TextUnformatted("Next: connect emulator core loop");
		ImGui::End();
		ImGui::Render();
#endif

		SDL_SetRenderDrawColor(pRenderer, 18, 24, 30, 255);
		SDL_RenderClear(pRenderer);

#ifdef X88000_SDL3_HAS_IMGUI
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), pRenderer);
#endif

		SDL_RenderPresent(pRenderer);
	}

#ifdef X88000_SDL3_HAS_IMGUI
	ImGui_ImplSDLRenderer3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
#endif

	SDL_DestroyRenderer(pRenderer);
	SDL_DestroyWindow(pWindow);
	SDL_Quit();
	return 0;
}
