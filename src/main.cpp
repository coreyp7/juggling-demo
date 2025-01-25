#include <stdio.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
//static SDL_Joystick *joystick = NULL;
static SDL_Gamepad *gamepad = NULL;
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
void print_joysticks();

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

        // input handling (move into function)
        SDL_Event event;
        while(SDL_PollEvent(&event)){
            if (event.type == SDL_EVENT_QUIT) {
                running = 0;  /* end the program, reporting success to the OS. */
            } else if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
                /* this event is sent for each hotplugged stick, but also each already-connected joystick during SDL_Init(). */
                if (gamepad == NULL) {  /* we don't have a stick yet and one was added, open it! */
                    gamepad = SDL_OpenGamepad(event.gdevice.which);
                }
            } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) { // from example
                if (gamepad && (SDL_GetGamepadID(gamepad) == event.gdevice.which)) {
                    SDL_CloseGamepad(gamepad);  /* our joystick was unplugged. */
                    gamepad = NULL;
                }
            }
        }
        print_joysticks();
        // input handling (end)

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

    if (gamepad) {
        SDL_CloseGamepad(gamepad);
    }

}

int initEverything(){
    int i;

    SDL_SetAppMetadata("Example Input Joystick Polling", "1.0", "com.example.input-joystick-polling");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD)) {
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



void print_joysticks(){
    if (!gamepad) {  /* we have a stick opened? */
        return;
    }

    //const float size = 30.0f;
    //int total;

    /*
    int total = SDL_GetNumJoystickAxes(joystick);
    for (int i = 0; i < total; i++) {
        const float val = (((float) SDL_GetJoystickAxis(joystick, i)) / 32767.0f);
        printf("Axis%i: %f\n", i, val); 
    }
    */
    Sint16 leftStickX = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX);
    Sint16 leftStickY = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY);
    printf("leftStickX: %i, leftStickY: %i\n", leftStickX, leftStickY);

    Sint16 rightStickX = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX);
    Sint16 rightStickY = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY);
    printf("rightStickX: %i, rightStickY: %i\n", rightStickX, rightStickY);

}


