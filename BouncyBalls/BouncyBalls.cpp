#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <cmath>
#include <vector>

const int WIDTH = 777;
const int HEIGHT = 777;
const float GRAVITY = 0.21f;
const float BOUNCE_DAMPING = 0.87f;
const int TRAIL_LENGTH = 25;
const int CIRCLE_SEGMENTS = 32;

struct Ball {
    float x, y;
    float vx, vy;
    int r, g, b;
    int radius;
    std::vector<std::pair<float,float>> trail;
    float restTimer  = 0.0f;
    float opacity    = 255.0f;
    bool  fading     = false;
};

void DrawFilledCircle(SDL_Renderer* renderer, float cx, float cy, float radius, int r, int g, int b, int a) {
    const float PI = 3.1415926535f;

    std::vector<SDL_Vertex> vertices;
    std::vector<int> indices;

    vertices.reserve(CIRCLE_SEGMENTS + 2);
    indices.reserve(CIRCLE_SEGMENTS * 3);

    SDL_Vertex center{};
    center.position  = { cx, cy };
    center.color.r   = r / 255.0f;
    center.color.g   = g / 255.0f;
    center.color.b   = b / 255.0f;
    center.color.a   = a / 255.0f;
    center.tex_coord = { 0.0f, 0.0f };
    vertices.push_back(center);

    //ring vertices
    for (int i = 0; i <= CIRCLE_SEGMENTS; i++) {
        float angle = (float)i / CIRCLE_SEGMENTS * 2.0f * PI;

        SDL_Vertex v{};
        v.position  = { cx + radius * cosf(angle), cy + radius * sinf(angle) };
        v.color.r   = r / 255.0f;
        v.color.g   = g / 255.0f;
        v.color.b   = b / 255.0f;
        v.color.a   = a / 255.0f;
        v.tex_coord = { 0.0f, 0.0f };
        vertices.push_back(v);
    }

    //triangle fan indices
    for (int i = 1; i <= CIRCLE_SEGMENTS; i++) {
        indices.push_back(0);
        indices.push_back(i);
        indices.push_back(i + 1);
    }

    SDL_RenderGeometry(renderer,
                       NULL,
                       vertices.data(), (int)vertices.size(),
                       indices.data(), (int)indices.size());
}

Ball SpawnBall(float x, float y) {
    Ball b;
    b.x = x;
    b.y = y;
    b.vx = (rand() % 7) - 3;
    b.vy = (rand() % 5) - 5;
    b.r  = rand() % 255;
    b.g  = rand() % 255;
    b.b  = rand() % 255;
    b.radius = 10 + rand() % 20;
    return b;
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow("Bouncy Balls!", WIDTH, HEIGHT, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);

    //window icon
    SDL_Surface* icon = SDL_LoadBMP("icon.bmp");
    if (icon) {
        SDL_SetWindowIcon(window, icon);
        SDL_DestroySurface(icon);
    }

    //audio setup
    MIX_Init();
    MIX_Mixer* mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    MIX_Audio* audio = MIX_LoadAudio(mixer, "dubtep.mp3", true);
    MIX_Track* track = MIX_CreateTrack(mixer);
    MIX_SetTrackAudio(track, audio);
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, -1);
    MIX_PlayTrack(track, props);
    SDL_DestroyProperties(props);

    std::vector<Ball> balls;
    balls.push_back(SpawnBall(WIDTH / 2, HEIGHT / 2));

    float speed = 1.0f;
    bool fullscreen = false;
    bool running = true;
    SDL_Event event;

    while (running) {
        //input
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN) {
                switch (event.key.key) {
                    case SDLK_UP:   speed += 0.5f; break;
                    case SDLK_DOWN:
                        speed -= 0.5f;
                        if (speed < 0.1f) speed = 0.1f;
                        if (speed > 2.0f) speed = 2.0f;
                        break;
                    case SDLK_ESCAPE: running = false; break;
                    case SDLK_F:
                        fullscreen = !fullscreen;
                        SDL_SetWindowFullscreen(window, fullscreen);
                        break;
                }
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                balls.push_back(SpawnBall(event.button.x, event.button.y));
            }
        }

        //update
        for (int i = (int)balls.size() - 1; i >= 0; i--) {
            Ball& b = balls[i];

            if (!b.fading) {
                b.trail.push_back({ b.x, b.y });
                if ((int)b.trail.size() > TRAIL_LENGTH)
                    b.trail.erase(b.trail.begin());

                b.vy += GRAVITY * speed;
                b.x  += b.vx * speed;
                b.y  += b.vy * speed;

                if (b.x - b.radius <= 0) {
                    b.x = b.radius;
                    b.vx = -b.vx * BOUNCE_DAMPING;
                } else if (b.x + b.radius >= WIDTH) {
                    b.x = WIDTH - b.radius;
                    b.vx = -b.vx * BOUNCE_DAMPING;
                }
                if (b.y - b.radius <= 0) {
                    b.y = b.radius;
                    b.vy = -b.vy * BOUNCE_DAMPING;
                } else if (b.y + b.radius >= HEIGHT) {
                    b.y = HEIGHT - b.radius;
                    b.vy = -b.vy * BOUNCE_DAMPING;
                    b.vx *= BOUNCE_DAMPING;
                }

                //snap
                if (b.y + b.radius >= HEIGHT - 1) {
                    if (fabs(b.vy) < 0.5f) b.vy = 0.0f;
                    if (fabs(b.vx) < 0.5f) b.vx = 0.0f;
                }

                //rest check
                bool onFloor    = (b.y + b.radius >= HEIGHT - 1);
                bool slowEnough = (fabs(b.vy) <= 0.5f && fabs(b.vx) <= 0.5f);

                if (onFloor && slowEnough) {
                    b.restTimer += 0.005f;
                    if (b.restTimer >= 2.0f)
                        b.fading = true;
                } else {
                    b.restTimer = 0.0f;
                }
            }

            if (b.fading) {
                b.opacity -= 4.0f;
                b.trail.clear();
                if (b.opacity <= 0.0f) {
                    balls.erase(balls.begin() + i);
                    SDL_Log("Balls remaining: %d", (int)balls.size());
                    continue;
                }
            }
        }

        //clear screen
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        //draw trails and balls
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        for (Ball& b : balls) {
            for (int i = 0; i < (int)b.trail.size(); i++) {
                float t      = (float)i / TRAIL_LENGTH;
                int   alpha  = (int)(t * b.opacity * (180.0f / 255.0f));
                float radius = b.radius * t * 0.9f;
                DrawFilledCircle(renderer,
                    b.trail[i].first, b.trail[i].second,
                    radius, b.r, b.g, b.b, alpha
                );
            }
            DrawFilledCircle(renderer, b.x, b.y, b.radius, b.r, b.g, b.b, (int)b.opacity);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(5);
    }

    //cleanup
    MIX_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}