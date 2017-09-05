#include "Draw.hpp"
#include "GL.hpp"

#include <SDL.h>
#include <glm/glm.hpp>

#include <chrono>
#include <iostream>
#include <random>

int main(int argc, char **argv) {
	//Configuration:
	struct {
		std::string title = "Hammer - Designed by bpx";
		glm::uvec2 size = glm::uvec2(720, 720);
	} config;

	//------------  initialization ------------

	//Initialize SDL library:
	SDL_Init(SDL_INIT_VIDEO);

	//Ask for an OpenGL context version 3.3, core profile, enable debug:
	SDL_GL_ResetAttributes();
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

	//create window:
	SDL_Window *window = SDL_CreateWindow(
		config.title.c_str(),
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		config.size.x, config.size.y,
		SDL_WINDOW_OPENGL /*| SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI*/
	);

	if (!window) {
		std::cerr << "Error creating SDL window: " << SDL_GetError() << std::endl;
		return 1;
	}

	//Create OpenGL context:
	SDL_GLContext context = SDL_GL_CreateContext(window);

	if (!context) {
		SDL_DestroyWindow(window);
		std::cerr << "Error creating OpenGL context: " << SDL_GetError() << std::endl;
		return 1;
	}

	#ifdef _WIN32
	//On windows, load OpenGL extensions:
	if (!init_gl_shims()) {
		std::cerr << "ERROR: failed to initialize shims." << std::endl;
		return 1;
	}
	#endif

	//Set VSYNC + Late Swap (prevents crazy FPS):
	if (SDL_GL_SetSwapInterval(-1) != 0) {
		std::cerr << "NOTE: couldn't set vsync + late swap tearing (" << SDL_GetError() << ")." << std::endl;
		if (SDL_GL_SetSwapInterval(1) != 0) {
			std::cerr << "NOTE: couldn't set vsync (" << SDL_GetError() << ")." << std::endl;
		}
	}

	//Hide mouse cursor (note: showing can be useful for debugging):
	SDL_ShowCursor(SDL_DISABLE);

	//------------  game state ------------

	struct LevelData {
		int balls_to_spawn;
		int hits_to_lose;
		float spawn_delay;
		float max_vel;
		LevelData(int balls_to_spawn, int hits_to_lose, float spawn_delay, float max_vel)
			: balls_to_spawn(balls_to_spawn), hits_to_lose(hits_to_lose), spawn_delay(spawn_delay), max_vel(max_vel)
		{};
	};

	const unsigned LEVEL_COUNT = 5;

	LevelData levels[LEVEL_COUNT] = {
		LevelData(10, 10, 2.5f, 1.0f),
		LevelData(15, 10, 3.0f, 1.0f),
		LevelData(10, 10, 2.0f, 2.0f),
		LevelData(5, 10, 5.0f, 6.0f),
		LevelData(10, 10, 1.0f, 3.0f) // 10 very fast balls
	};

	struct Object {
		glm::vec2 pos;
		glm::vec2 vel;
		glm::vec2 size;
		glm::u8vec4 color;
		bool alive;

		Object(glm::vec2 pos, glm::vec2 size, glm::u8vec4 color)
			: pos(pos), vel(0.0f, 0.0f), size(size), color(color), alive(true)
		{};
	};

	enum Progress {
		MENU,
		PLAYING,
		GAME_OVER
	};

	struct {
		glm::vec2 mouse = glm::vec2(0.0f, 0.0f);
		Object hammer = Object({ 0.0f, 0.0f }, { 0.29f, 0.07f }, { 0xFF, 0xFF, 0xFF, 0xFF });
		Progress progress = Progress::MENU;
		std::vector<Object> balls;
		unsigned level_index = 0;
		std::vector<Object> level_indicators;
	} state;

	float min_width = state.hammer.size.y;
	float max_width = state.hammer.size.x;

	auto step_down_hammer_width = [&]() -> void {
		state.hammer.size.x -= (max_width - min_width) / levels[state.level_index].hits_to_lose + 0.001f;

		if (state.hammer.size.x <= min_width)
		{
			state.progress = Progress::GAME_OVER;
			return;
		}
	};

	for (int i = 0; i < LEVEL_COUNT; i++) {
		float width = 2.0f / LEVEL_COUNT;
		// something to decorate the menu
		glm::u8vec4 color = i % 2 == 0 ? glm::u8vec4(0x00, 0xFF, 0x00, 0xFF) : glm::u8vec4(0xFF, 0x00, 0x00, 0xFF);
		state.level_indicators.emplace_back(glm::vec2(-1.0f + i * width + width / 2.0f, 0.95f), glm::vec2(width - 0.03f, 0.06f), color);
	}

	//------------  game loop ------------

	auto previous_time = std::chrono::high_resolution_clock::now();
	bool should_quit = false;

	int spawn_ball_index = 0;

	while (true) {
		static SDL_Event evt;
		while (SDL_PollEvent(&evt) == 1) {
			if (evt.type == SDL_MOUSEMOTION && state.progress != Progress::GAME_OVER) {
				state.hammer.pos.x = (evt.motion.x + 0.5f) / float(config.size.x) * 2.0f - 1.0f;;
			}
			else if (evt.type == SDL_MOUSEBUTTONDOWN)
			{
				// start playing on click if not playing
				if (state.progress == Progress::MENU || state.progress == Progress::GAME_OVER) {
					state.progress = Progress::PLAYING;

					state.level_index = 0;

					state.hammer.size.x = max_width;

					for (Object& indicator : state.level_indicators) {
						indicator.color.r = 0x00;
						indicator.color.g = 0x00;
						indicator.color.b = 0x00;
					}

					spawn_ball_index = 0;

					state.balls.clear();

					// create balls for first level
					for (int i = 0; i < levels[state.level_index].balls_to_spawn; i++) {
						state.balls.emplace_back(glm::vec2(-2.0f, -2.0f), glm::vec2(0.04f, 0.04f), glm::u8vec4(0xFF, 0x22, 0x22, 0xFF));
					}
				}
			} else if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_ESCAPE) {
				should_quit = true;
			} else if (evt.type == SDL_QUIT) {
				should_quit = true;
				break;
			}
		}
		if (should_quit) break;

		auto current_time = std::chrono::high_resolution_clock::now();
		float elapsed = std::chrono::duration< float >(current_time - previous_time).count();
		previous_time = current_time;

		static const float LEVEL_COOLDOWN_MAX = 4.0f;

		static float timer = 0.0f;
		static float level_cooldown = 2.0f; // start timer at 3 of 5 seconds so first delay is short

		timer += elapsed;
		level_cooldown += elapsed;

		// from https://www.guyrutenberg.com/2014/05/03/c-mt19937-example/
		static std::mt19937::result_type seed = time(0);
		static auto real_rand = std::bind(std::uniform_real_distribution<double>(0, 1),
                           std::mt19937(seed));
		static auto range = [&](float min, float max) -> float {
			return min + real_rand() * (max - min);
		};

		// update game state:
		switch (state.progress) {

			case Progress::MENU: {
				break;
			}

			case Progress::PLAYING: {
				// calcuate hammer bounds
				float h_x1 = state.hammer.pos.x - (state.hammer.size.x / 2.0f);
				float h_x2 = state.hammer.pos.x + (state.hammer.size.x / 2.0f);
				float h_y1 = state.hammer.pos.y - (state.hammer.size.y / 2.0f);
				float h_y2 = state.hammer.pos.y + (state.hammer.size.y / 2.0f);

				for (Object& ball : state.balls) {
					if (ball.alive) {
						ball.pos += elapsed * ball.vel;

						float b_x1 = ball.pos.x - (ball.size.x / 2.0f);
						float b_x2 = ball.pos.x + (ball.size.x / 2.0f);
						float b_y1 = ball.pos.y - (ball.size.y / 2.0f);
						float b_y2 = ball.pos.y + (ball.size.y / 2.0f);

						// finding direction of collision from https://stackoverflow.com/a/13349505
						float bottom_overlap = h_y2 - b_y1;
						float top_overlap = b_y2 - h_y1;
						float left_overlap = b_x2 - h_x1;
						float right_overlap = h_x2 - b_x1;

						// AABB from https://developer.mozilla.org/en-US/docs/Games/Techniques/2D_collision_detection#Axis-Aligned_Bounding_Box
						if (b_x1 < h_x2 &&
								b_x2 > h_x1 &&
								b_y1 < h_y2 &&
								b_y2 > h_y1) {
							ball.pos -= elapsed * ball.vel;

							if (top_overlap < bottom_overlap && top_overlap < left_overlap && top_overlap < right_overlap) {
								ball.vel.y = -ball.vel.y * 0.8f;
								ball.color.r *= 0.9f;
								step_down_hammer_width();
							} else if (bottom_overlap < top_overlap && bottom_overlap < left_overlap && bottom_overlap < right_overlap) {
								ball.vel.y = -ball.vel.y * 0.8f;
								ball.color.r *= 0.9f;
								step_down_hammer_width();
							}

							// erase-remove idiom from https://stackoverflow.com/a/3385251
							if (left_overlap < bottom_overlap && left_overlap < top_overlap && left_overlap < right_overlap) {
								ball.pos.y = -2.0f;
								ball.alive = false;
							} else if (right_overlap < bottom_overlap && right_overlap < left_overlap && right_overlap < top_overlap) {
								ball.pos.y = -2.0f;
								ball.alive = false;
							}
						}

						// bounce off the walls
						if (ball.vel.y < 0 && ball.pos.y < -1.0f) {
							ball.vel.y = -ball.vel.y;
						} else if (ball.vel.y > 0 && ball.pos.y > 1.0f) {
							ball.vel.y = -ball.vel.y;
						} else if (ball.vel.x > 0 && ball.pos.x > 1.0f) {
							ball.vel.x = -ball.vel.x;
						} else if (ball.vel.x < 0 && ball.pos.x < -1.0f) {
							ball.vel.x = -ball.vel.x;
						}
					}
				}

				// spawn balls with random location
				if (level_cooldown > LEVEL_COOLDOWN_MAX && timer > levels[state.level_index].spawn_delay && spawn_ball_index < state.balls.size()) {
					float spawnX = range(-1.0f, 1.0f);
					float spawnY = real_rand() < 0.5f ? -1.0f : 1.0f;

					// y = sqrt(max - x^2)
					Object &ball = state.balls.at(spawn_ball_index);
					ball.vel.x = range(-0.5f, 0.5f);
					ball.vel.y = spawnY * -(std::sqrt(levels[state.level_index].max_vel - ball.vel.x * ball.vel.x));

					ball.pos.x = spawnX;
					ball.pos.y = spawnY;

					timer = 0.0f;
					spawn_ball_index++;
				}

				bool won = true;

				// stupid loop to find win condition
				for (const Object& ball : state.balls) {
					if (ball.alive) {
						won = false;
						break;
					}
				}

				if (won) {
					state.level_indicators.at(state.level_index).color.r = 0xFF;
					state.level_indicators.at(state.level_index).color.g = 0xFF;
					state.level_indicators.at(state.level_index).color.b = 0xFF;

					state.level_index += 1;

					level_cooldown = 0.0f;

					if (state.level_index == LEVEL_COUNT) {
						state.progress = Progress::GAME_OVER;
						// unlock blue hammer
						if (state.level_index == LEVEL_COUNT) {
							state.hammer.color.r = 0x00;
							state.hammer.color.g = 0x00;
						}
					} else {
						state.hammer.size.x = max_width;

						spawn_ball_index = 0;

						state.balls.clear();

						// create balls for next level
						for (int i = 0; i < levels[state.level_index].balls_to_spawn; i++) {
							state.balls.emplace_back(glm::vec2(-2.0f, -2.0f), glm::vec2(0.04f, 0.04f), glm::u8vec4(0xFF, 0x22, 0x22, 0xFF));
						}
					}
				}
				break;
			}

			case Progress::GAME_OVER: {
				break;
			}
		}

		//draw output:
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);


		{ //draw game state:
			Draw draw;
			for (const Object indicator : state.level_indicators) {
				draw.add_rectangle(indicator.pos - indicator.size / 2.0f, indicator.pos + indicator.size / 2.0f, indicator.color);
			}
			for (const Object ball : state.balls) {
				draw.add_rectangle(ball.pos - ball.size / 2.0f, ball.pos + ball.size / 2.0f, ball.color);
			}
			draw.add_rectangle(
				state.hammer.pos - state.hammer.size / 2.0f,
				state.hammer.pos + state.hammer.size / 2.0f,
				state.hammer.color
			);
			draw.draw();
		}


		SDL_GL_SwapWindow(window);
	}


	//------------  teardown ------------

	SDL_GL_DeleteContext(context);
	context = 0;

	SDL_DestroyWindow(window);
	window = NULL;

	return 0;
}
