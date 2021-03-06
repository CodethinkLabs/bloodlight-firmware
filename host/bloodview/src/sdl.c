/*
 * Copyright 2020 Codethink Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * \file
 * \brief Implementation of the SDL handling.
 *
 * This module handles interaction with SDL.
 */

#include <stdio.h>

#include <SDL2/SDL.h>

#include "sdl-tk/widget.h"

#include "graph.h"
#include "main-menu.h"

/** Mask of SDL subsystems we use. */
#define BL_SDL_INIT_MASK (SDL_INIT_VIDEO)

/**
 * SDL module context global data.
 */
static struct sdl_ctx
{
	SDL_Window   *win; /**< SDL window object. */
	SDL_Renderer *ren; /**< SDL renderer object. */

	struct sdl_tk_widget *main_menu;      /**< Main menu sdl-tk widget. */
	bool                  main_menu_open; /**< Whether the menu is open. */
	int                   main_menu_x;    /**< Menu x-coordinate. */
	int                   main_menu_y;    /**< Menu y-coordinate. */

	unsigned w; /**< Viewport width. */
	unsigned h; /**< Viewport height, */

	bool shift; /**< Whether shift is pressed. */
	bool ctrl;  /**< Whether ctrl is pressed. */

	SDL_Rect graph_rect; /**< Rectangle containing graphs. */
} ctx; /**< SDL module context global object. */

/* Exported interface, documented in sdl.h */
void sdl_fini(void)
{
	main_menu_destroy(ctx.main_menu);
	sdl_tk_text_fini();
	sdl_tk_colour_fini();

	if (ctx.ren != NULL) {
		SDL_DestroyRenderer(ctx.ren);
		ctx.ren = NULL;
	}
	if (ctx.win != NULL) {
		SDL_DestroyWindow(ctx.win);
		ctx.ren = NULL;
	}
	if (SDL_WasInit(BL_SDL_INIT_MASK) == BL_SDL_INIT_MASK) {
		SDL_Quit();
	}
}

/* Exported interface, documented in sdl.h */
bool sdl_init(const char *resources_dir_path,
		const char *config_dir_path,
		const char *config_file,
		const char *font_path)
{
	if (SDL_Init(BL_SDL_INIT_MASK) != 0) {
		fprintf(stderr, "SDL_Init Error: %s\n",
				SDL_GetError());
		goto error;
	}

	ctx.w = 1280;
	ctx.h = 720;

	ctx.win = SDL_CreateWindow("Bloodlight",
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED,
			ctx.w, ctx.h,
			SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	if (ctx.win == NULL) {
		fprintf(stderr, "SDL_CreateWindow Error: %s\n",
				SDL_GetError());
		goto error;
	}

	ctx.ren = SDL_CreateRenderer(ctx.win, -1,
			SDL_RENDERER_ACCELERATED |
			SDL_RENDERER_PRESENTVSYNC);
	if (ctx.ren == NULL) {
		fprintf(stderr, "SDL_CreateRenderer Error: %s\n",
				SDL_GetError());
		goto error;
	}

	if (!sdl_tk_colour_init()) {
		goto error;
	}

	if (!sdl_tk_text_init(ctx.ren, font_path)) {
		goto error;
	}

	ctx.main_menu = main_menu_create(
			resources_dir_path,
			config_dir_path,
			config_file);
	if (ctx.main_menu == NULL) {
		goto error;
	}

	ctx.main_menu_open = true;
	ctx.main_menu_x = ctx.w / 2;
	ctx.main_menu_y = ctx.h / 2;
	sdl_tk_widget_focus(
			ctx.main_menu,
			ctx.main_menu_open);

	ctx.graph_rect.x = 0;
	ctx.graph_rect.y = 0;
	ctx.graph_rect.w = ctx.w;
	ctx.graph_rect.h = ctx.h;

	return true;

error:
	sdl_fini();
	return false;
}

/**
 * Toggle whether the main menu is displayed.
 */
static void sdl__main_menu_toggle(void)
{
	ctx.main_menu_open = !ctx.main_menu_open;

	sdl_tk_widget_focus(ctx.main_menu, ctx.main_menu_open);
}

/**
 * Handle keyboard and mouse input.
 *
 * \param[in]  event  SDL event to handle.
 */
static void sdl__handle_input(SDL_Event *event)
{
	if (!sdl_tk_widget_input(ctx.main_menu, event, &ctx.graph_rect,
			ctx.main_menu_x, ctx.main_menu_y)) {
		switch (event->type) {
		case SDL_KEYDOWN:
			switch (event->key.keysym.sym) {
			case SDLK_ESCAPE:
				if (ctx.main_menu_open == false) {
					ctx.main_menu_x = ctx.w / 2;
					ctx.main_menu_y = ctx.h / 2;
				}
				sdl__main_menu_toggle();
				return;

			case SDLK_RSHIFT: /* Fall through */
			case SDLK_LSHIFT:
				ctx.shift = true;
				return;

			case SDLK_RCTRL: /* Fall through */
			case SDLK_LCTRL:
				ctx.ctrl = true;
				return;
			}
			break;

		case SDL_KEYUP:
			switch (event->key.keysym.sym) {
			case SDLK_RSHIFT: /* Fall through */
			case SDLK_LSHIFT:
				ctx.shift = false;
				return;

			case SDLK_RCTRL: /* Fall through */
			case SDLK_LCTRL:
				ctx.ctrl = false;
				return;
			}
			break;

		case SDL_MOUSEBUTTONDOWN:
			if (ctx.main_menu_open == false) {
				switch (event->button.button) {
				case SDL_BUTTON_RIGHT:
					SDL_GetMouseState(
							&ctx.main_menu_x,
							&ctx.main_menu_y);
					sdl__main_menu_toggle();
					return;
				}
			} else {
				sdl__main_menu_toggle();
				return;
			}
			break;
		}

		graph_handle_input(event, &ctx.graph_rect, ctx.shift, ctx.ctrl);
	}
}

/* Exported interface, documented in sdl.h */
void sdl_main_menu_close(void)
{
	ctx.main_menu_open = false;
	sdl_tk_widget_focus(
			ctx.main_menu,
			ctx.main_menu_open);
}

/* Exported interface, documented in sdl.h */
bool sdl_handle_input(void)
{
	static SDL_Event event;

	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_QUIT:
			return false;

		case SDL_WINDOWEVENT:
			switch (event.window.event) {
			case SDL_WINDOWEVENT_SIZE_CHANGED:
				ctx.w = event.window.data1;
				ctx.h = event.window.data2;
				ctx.graph_rect.w = ctx.w;
				ctx.graph_rect.h = ctx.h;
				break;
			}
			break;

		default:
			sdl__handle_input(&event);
			break;
		}
	}

	return true;
}

/* Exported interface, documented in sdl.h */
void sdl_present(void)
{
	SDL_Color bg = sdl_tk_colour_get(SDL_TK_COLOUR_BACKGROUND);

	SDL_SetRenderDrawColor(ctx.ren, bg.r, bg.g, bg.b, 255);
	SDL_RenderClear(ctx.ren);

	graph_render(ctx.ren, &ctx.graph_rect);

	main_menu_update();
	sdl_tk_widget_render(ctx.main_menu, &ctx.graph_rect,
			ctx.ren, ctx.main_menu_x, ctx.main_menu_y);
	SDL_RenderPresent(ctx.ren);
}
