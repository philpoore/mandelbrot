#include <SDL2/SDL.h>
#include <chrono>
#include <complex>
#include <iostream>
#include <vector>

const char *title = "Mandelbrot";
const int windowWidth = 1280;
const int windowHeight = 1080;

typedef struct {
    double min_x;
    double max_x;
    double min_y;
    double max_y;
} BoundsRect;

void set_pixel(SDL_Surface *bitmap, int x, int y, uint8_t r, uint8_t g,
               uint8_t b) {
    uint32_t offset = ((y * bitmap->w) + x) * 3;
    *((uint8_t *)bitmap->pixels + offset + 0) = r;
    *((uint8_t *)bitmap->pixels + offset + 1) = g;
    *((uint8_t *)bitmap->pixels + offset + 2) = b;
}

void draw_mandelbrot(SDL_Surface *bitmap, BoundsRect *brect) {
    auto time_start = std::chrono::high_resolution_clock::now();
    double range_x = brect->max_x - brect->min_x;
    double range_y = brect->max_y - brect->min_y;

    double step_x = range_x / windowWidth;
    double step_y = range_y / windowHeight;

    auto mag2 = [](std::complex<double> z) {
        return z.imag() * z.imag() + z.real() * z.real();
    };

    double sy = brect->min_y;

    for (int y = 0; y < windowHeight; y++, sy += step_y) {
        // std::cout << "y: " << y << "\n";
        double sx = brect->min_x;
        for (int x = 0; x < windowWidth; x++, sx += step_x) {
            std::complex<double> z(sx, sy);
            auto c = z;

            uint8_t i = 0;
            while (mag2(z) < 4) {
                z = (z * z) + c;
                i++;

                if (i == 255) {
                    i = 0;
                    break;
                }
            }

            set_pixel(bitmap, x, y, i, i, i);
        }
    }

    auto time_end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        time_end - time_start);
    std::cout << "duration: " << duration.count() << "ms\n";
}

double scale(int v, int min, int max, double min_d, double max_d) {
    double f = ((double)v - min) / (max - min);
    return f * (max_d - min_d) + min_d;
}

std::vector<BoundsRect> history;

int main() {
    BoundsRect brect = {-2, 2, -2, 2};

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window =
        SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         windowWidth, windowHeight, 0);

    if (window == NULL) {
        printf("Could not create window: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Renderer *renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    SDL_Surface *bitmap = SDL_CreateRGBSurfaceWithFormat(
        0, windowWidth, windowHeight, 24, SDL_PIXELFORMAT_RGB24);

    bool running = true;
    bool mouse_down = false;
    bool dragging = false;

    auto mouse_down_pos = std::make_pair<int, int>(0, 0);
    auto mouse_pos = std::make_pair<int, int>(0, 0);

    bool command_pressed = false;

    SDL_Rect drag_rect;
    bool need_redraw = true;

    while (running) {
        if (need_redraw) {
            // Redraw the mandelbrow
            draw_mandelbrot(bitmap, &brect);
            need_redraw = false;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, bitmap);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);

        SDL_RenderCopy(renderer, texture, nullptr, nullptr);

        // If we are dragging currently, draw the rectangle overlay.
        if (dragging) {
            drag_rect.x = mouse_down_pos.first;
            drag_rect.y = mouse_down_pos.second;
            drag_rect.w = mouse_pos.first - mouse_down_pos.first;
            drag_rect.h = mouse_pos.second - mouse_down_pos.second;
            SDL_SetRenderDrawColor(renderer, 0, 234, 0, 255);
            SDL_RenderDrawRect(renderer, &drag_rect);
        }

        SDL_RenderPresent(renderer);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                SDL_MouseButtonEvent *mouse_event =
                    (SDL_MouseButtonEvent *)&event;
                std::cout << "[Mouse Down] x: " << mouse_event->x
                          << " y: " << mouse_event->y << "\n";

                mouse_down = true;
                mouse_down_pos.first = mouse_event->x;
                mouse_down_pos.second = mouse_event->y;
            } else if (event.type == SDL_MOUSEBUTTONUP) {
                SDL_MouseButtonEvent *mouse_event =
                    (SDL_MouseButtonEvent *)&event;
                std::cout << "[Mouse Up] x: " << mouse_event->x
                          << " y: " << mouse_event->y << "\n";

                if (dragging) {
                    // We were dragging, we should now zoom in.
                    // Work out where we want to zoom into.
                    int x1 = drag_rect.x;
                    int x2 = drag_rect.x + drag_rect.w;
                    int y1 = drag_rect.y;
                    int y2 = drag_rect.y + drag_rect.h;
                    int min_xi = std::min(x1, x2);
                    int max_xi = std::max(x1, x2);
                    int min_yi = std::min(y1, y2);
                    int max_yi = std::max(y1, y2);

                    BoundsRect brect_new;
                    brect_new.min_x =
                        scale(min_xi, 0, windowWidth, brect.min_x, brect.max_x);
                    brect_new.max_x =
                        scale(max_xi, 0, windowWidth, brect.min_x, brect.max_x);
                    brect_new.min_y = scale(min_yi, 0, windowHeight,
                                            brect.min_y, brect.max_y);
                    brect_new.max_y = scale(max_yi, 0, windowHeight,
                                            brect.min_y, brect.max_y);

                    // Add the current position to history so we can
                    // undo if we want to.
                    history.push_back(brect);
                    brect = brect_new;
                    need_redraw = true;
                }

                mouse_down = false;
                dragging = false;
            } else if (event.type == SDL_MOUSEMOTION) {
                if (mouse_down) {
                    SDL_MouseMotionEvent *mouse_event =
                        (SDL_MouseMotionEvent *)&event;
                    std::cout << "[Mouse Dragging] x: " << mouse_event->x
                              << " y: " << mouse_event->y << "\n";

                    dragging = true;
                    mouse_pos.first = mouse_event->x;
                    mouse_pos.second = mouse_event->y;
                }
            } else if (event.type == SDL_KEYDOWN) {
                SDL_KeyboardEvent *keyboard_event = (SDL_KeyboardEvent *)&event;

                if (keyboard_event->keysym.scancode == SDL_SCANCODE_LGUI) {
                    command_pressed = true;
                }
            } else if (event.type == SDL_KEYUP) {
                SDL_KeyboardEvent *keyboard_event = (SDL_KeyboardEvent *)&event;

                if (keyboard_event->keysym.scancode == SDL_SCANCODE_LGUI) {
                    command_pressed = false;
                }

                if (keyboard_event->keysym.scancode == SDL_SCANCODE_Z &&
                    command_pressed) {
                    // Undo
                    if (history.size()) {
                        brect = history.back();
                        history.pop_back();
                        need_redraw = true;
                    }
                }
            }
        }
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
