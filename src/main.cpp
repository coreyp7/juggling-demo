#include <stdio.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Joystick *joystick = NULL;
static SDL_Color colors[64];

int fpsCap = 60;
Uint32 frameTimeToComplete = -1; 
Uint32 frameStart;
Uint32 frameEnd;

// Tick counters for performance measuring only
Uint64 startTime;
Uint64 endTime;
Uint64 frameLength;

// Simulate stuff
Uint64 lastUpdate = 0;
float dt = 0;

int initEverything();
int handleEvents();
void simulate();
void render();

int main(int argc, char* argv[]) {
    if(initEverything() != SDL_APP_CONTINUE){
        SDL_Log("Someting went bad");
        return -1;
    }
    else {
        SDL_Log("Everythings all good!");
    }
    
    int running = 1;
    while(running){
        frameStart = SDL_GetTicks();
        startTime = SDL_GetPerformanceCounter();
        
        int result = handleEvents();
        if(result == SDL_APP_SUCCESS){
            running = 0;
        }
        simulate();

        render();

        frameTimeToComplete = SDL_GetTicks() - frameStart;
        if(1000/ fpsCap > frameTimeToComplete){
            SDL_Delay((1000 / fpsCap) - frameTimeToComplete);
        } 

        endTime = SDL_GetPerformanceCounter();
        frameLength = (endTime - startTime) / static_cast<double>(SDL_GetPerformanceFrequency());
        printf("%i\n", frameLength);
    }

    if (joystick) {
        SDL_CloseJoystick(joystick);
    }

}

int initEverything(){
    int i;

    SDL_SetAppMetadata("Example Input Joystick Polling", "1.0", "com.example.input-joystick-polling");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("examples/input/joystick-polling", 640, 480, 0, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    for (i = 0; i < SDL_arraysize(colors); i++) {
        colors[i].r = SDL_rand(255);
        colors[i].g = SDL_rand(255);
        colors[i].b = SDL_rand(255);
        colors[i].a = 255;
    }

    IMG_LoadTexture(renderer, "assets/test.png");

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

void simulate(){
    dt = (SDL_GetTicks() - lastUpdate) / 1000.f;
    lastUpdate = SDL_GetTicks();
}

void render(){
    // Clear window before rendering
    int winw = 640, winh = 480;
    const char *text = "hello, please.";
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_GetWindowSize(window, &winw, &winh);

    // Show debug text
    float x, y;
    x = (((float) winw) - (SDL_strlen(text) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE)) / 2.0f;
    y = (((float) winh) - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) / 2.0f;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(renderer, x, y, text);

    // Push everything in buffered renderer to front.
    SDL_RenderPresent(renderer);
}

int handleEvents(){
    SDL_Event event;
    while(SDL_PollEvent(&event)){
        if (event.type == SDL_EVENT_QUIT) {
            return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
        } else if (event.type == SDL_EVENT_JOYSTICK_ADDED) {
            /* this event is sent for each hotplugged stick, but also each already-connected joystick during SDL_Init(). */
            if (joystick == NULL) {  /* we don't have a stick yet and one was added, open it! */
                joystick = SDL_OpenJoystick(event.jdevice.which);
                if (!joystick) {
                    SDL_Log("Failed to open joystick ID %u: %s", (unsigned int) event.jdevice.which, SDL_GetError());
                }
            }
        } else if (event.type == SDL_EVENT_JOYSTICK_REMOVED) {
            if (joystick && (SDL_GetJoystickID(joystick) == event.jdevice.which)) {
                SDL_CloseJoystick(joystick);  /* our joystick was unplugged. */
                joystick = NULL;
            }
        }
        return SDL_APP_CONTINUE;  /* carry on with the program! */    
    }
}





/**
This function is just to keep the demo code around so I can refer to it later
when I want to start capturing controller input.
It includes event capture and rendering in the same function, so I don't want
to use it.
*/
void demo_example(){
    int winw = 640, winh = 480;
    const char *text = "Plug in a joystick, please.";
    float x, y;
    int i;

    if (joystick) {  /* we have a stick opened? */
        text = SDL_GetJoystickName(joystick);
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_GetWindowSize(window, &winw, &winh);

    /* note that you can get input as events, instead of polling, which is
       better since it won't miss button presses if the system is lagging,
       but often times checking the current state per-frame is good enough,
       and maybe better if you'd rather _drop_ inputs due to lag. */

    if (joystick) {  /* we have a stick opened? */
        const float size = 30.0f;
        int total;

        /* draw axes as bars going across middle of screen. We don't know if it's an X or Y or whatever axis, so we can't do more than this. */
        total = SDL_GetNumJoystickAxes(joystick);
        y = (float) ((winh - (total * size)) / 2);
        x = ((float) winw) / 2.0f;
        for (i = 0; i < total; i++) {
            const SDL_Color *color = &colors[i % SDL_arraysize(colors)];
            const float val = (((float) SDL_GetJoystickAxis(joystick, i)) / 32767.0f);  /* make it -1.0f to 1.0f */
            const float dx = x + (val * x);
            const SDL_FRect dst = { dx, y, x - SDL_fabsf(dx), size };
            SDL_SetRenderDrawColor(renderer, color->r, color->g, color->b, color->a);
            SDL_RenderFillRect(renderer, &dst);
            y += size;
        }

        /* draw buttons as blocks across top of window. We only know the button numbers, but not where they are on the device. */
        total = SDL_GetNumJoystickButtons(joystick);
        x = (float) ((winw - (total * size)) / 2);
        for (i = 0; i < total; i++) {
            const SDL_Color *color = &colors[i % SDL_arraysize(colors)];
            const SDL_FRect dst = { x, 0.0f, size, size };
            if (SDL_GetJoystickButton(joystick, i)) {
                SDL_SetRenderDrawColor(renderer, color->r, color->g, color->b, color->a);
            } else {
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            }
            SDL_RenderFillRect(renderer, &dst);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, color->a);
            SDL_RenderRect(renderer, &dst);  /* outline it */
            x += size;
        }

        /* draw hats across the bottom of the screen. */
        total = SDL_GetNumJoystickHats(joystick);
        x = ((float) ((winw - (total * (size * 2.0f))) / 2.0f)) + (size / 2.0f);
        y = ((float) winh) - size;
        for (i = 0; i < total; i++) {
            const SDL_Color *color = &colors[i % SDL_arraysize(colors)];
            const float thirdsize = size / 3.0f;
            const SDL_FRect cross[] = { { x, y + thirdsize, size, thirdsize }, { x + thirdsize, y, thirdsize, size } };
            const Uint8 hat = SDL_GetJoystickHat(joystick, i);

            SDL_SetRenderDrawColor(renderer, 90, 90, 90, 255);
            SDL_RenderFillRects(renderer, cross, SDL_arraysize(cross));

            SDL_SetRenderDrawColor(renderer, color->r, color->g, color->b, color->a);

            if (hat & SDL_HAT_UP) {
                const SDL_FRect dst = { x + thirdsize, y, thirdsize, thirdsize };
                SDL_RenderFillRect(renderer, &dst);
            }

            if (hat & SDL_HAT_RIGHT) {
                const SDL_FRect dst = { x + (thirdsize * 2), y + thirdsize, thirdsize, thirdsize };
                SDL_RenderFillRect(renderer, &dst);
            }

            if (hat & SDL_HAT_DOWN) {
                const SDL_FRect dst = { x + thirdsize, y + (thirdsize * 2), thirdsize, thirdsize };
                SDL_RenderFillRect(renderer, &dst);
            }

            if (hat & SDL_HAT_LEFT) {
                const SDL_FRect dst = { x, y + thirdsize, thirdsize, thirdsize };
                SDL_RenderFillRect(renderer, &dst);
            }

            x += size * 2;
        }
    }

    x = (((float) winw) - (SDL_strlen(text) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE)) / 2.0f;
    y = (((float) winh) - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) / 2.0f;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(renderer, x, y, text);
    SDL_RenderPresent(renderer);


}
