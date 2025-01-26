#include <stdio.h>
#include <vector>
#include <string>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>

typedef struct {
    Sint16 lsX, lsY, rsX, rsY, lTrigHeld, rTrigHeld;
    bool setup; 
} GamepadInfo;

typedef struct {
    SDL_FRect rect;
    float xVel, yVel;
    bool isHeld;
} Ball;

typedef struct {
    SDL_FRect rect;
    SDL_FRect srcRect; // rect on texture to render
    SDL_FRect prevPos;
    Uint32 prevPosTicks;
    Ball* heldBall;
} Hand;

int WINDOW_W = 1920;
int WINDOW_H = 1080;

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Gamepad *gamepad = NULL;

int fpsCap = 60;
Uint32 frameStart;
Uint32 frameTimeToComplete = -1; 

static SDL_Texture* handsTexture = NULL;
static SDL_Texture* ballTexture = NULL;

Hand rightHand = {{600, 700, 150, 150}, {680, 0, 680, 861}, NULL, 0};
Hand leftHand = {{200, 700, 150, 150}, {0, 0, 680, 861}, NULL, 0};
//Ball ball = {{600, 0, 150, 150}, 0, 0, false};
std::vector<Ball*> balls;

int handSpeed = 17;
float handThrowForce = 42.f;
//int gravity = 1600;
int gravity = 15;

// Tick counters for performance measuring only
Uint64 startTime;
Uint64 endTime;
Uint64 frameLength;

// Simulate stuff (delta time)
Uint64 lastUpdate = 0;
float dt = 0;

int initEverything();
int handleEvents();
void simulate(GamepadInfo input);
void render();
GamepadInfo getGamepadInfo();
bool isColliding(SDL_FRect rect1, SDL_FRect rect2);
int handleEvent(SDL_Event event);

int main(int argc, char* argv[]) {
    if(initEverything() != SDL_APP_CONTINUE){
        SDL_Log("Someting went bad");
        return -1;
    }
    else {
        SDL_Log("Everythings all good!");
    }
    
    int running = 1;

    // Setup gamestate (balls)
    Ball* ball1 = new Ball{{600, 0, 150, 150}, 0, 0, false};
    //Ball* ball2 = new Ball{{300, 0, 150, 150}, 0, 0, false};
    Ball* ball2 = new Ball{{leftHand.rect.x, leftHand.rect.y-200, 150, 150}, 0, 0, false};
    //Ball* ball3 = new Ball{{900, 0, 150, 150}, 0, 0, false};
    Ball* ball3 = new Ball{{rightHand.rect.x, rightHand.rect.y-200, 150, 150}, 0, 0, false};
    balls.push_back(ball1); 
    balls.push_back(ball2); 
    balls.push_back(ball3); 

    // Put two of the balls into our hands so its easy to start
    /*
    leftHand.heldBall = ball2;
    rightHand.heldBall = ball3;
    ball2->isHeld = true;
    ball3->isHeld = true;
    */

    while(running){
        frameStart = SDL_GetTicks();
        startTime = SDL_GetPerformanceCounter();

        // input handling
        SDL_Event event;
        while(SDL_PollEvent(&event)){
            int result = handleEvent(event);
            if(result == SDL_EVENT_QUIT){
                running = 0;
            }
        }
        GamepadInfo gamepad = getGamepadInfo();
        // input handling (end)

        simulate(gamepad);

        render();

        // Delay next frame if we're running fast
        frameTimeToComplete = SDL_GetTicks() - frameStart;
        if(1000/ fpsCap > frameTimeToComplete){
            SDL_Delay((1000 / fpsCap) - frameTimeToComplete);
        } 

        // TODO: this should prolly be above delaying of next frame.
        // also should draw this number in the window.
        endTime = SDL_GetPerformanceCounter();
        frameLength = (endTime - startTime) / static_cast<double>(SDL_GetPerformanceFrequency());
    }

    if (gamepad) {
        SDL_CloseGamepad(gamepad);
    }

}

int initEverything(){
    int i;

    SDL_SetAppMetadata("Example Input Joystick Polling", "1.0", "com.example.input-joystick-polling");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("examples/input/joystick-polling", WINDOW_W, WINDOW_H, 0, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    handsTexture = IMG_LoadTexture(renderer, "assets/hands.png");
    ballTexture = IMG_LoadTexture(renderer, "assets/ball.png");

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

void simulate(GamepadInfo input){
    dt = (SDL_GetTicks() - lastUpdate) / 1000.f;
    // TODO: look into if this is correct.
    lastUpdate = SDL_GetTicks();

    float xLeftOld = leftHand.rect.x;
    float yLeftOld = leftHand.rect.y;
    float xRightOld = rightHand.rect.x;
    float yRightOld = rightHand.rect.y;

    if(input.rsX > 10 || input.rsX < -10){
        rightHand.rect.x += input.rsX * dt * handSpeed;
    }
    if(input.rsY > 10 || input.rsY < -10){
        rightHand.rect.y += input.rsY * dt * handSpeed;
    }

    if(input.lsX > 10 || input.lsX < -10){
        leftHand.rect.x += input.lsX * dt * handSpeed;
    }
    if(input.lsY > 10 || input.lsY < -10){
        leftHand.rect.y += input.lsY * dt * handSpeed;
    }

    // Rewrite for many balls
    if(leftHand.heldBall != NULL && input.lTrigHeld < 10){
        leftHand.heldBall->isHeld = false;
        
        // Draw vector with last position and send ball that way.
        SDL_FRect vector = {leftHand.rect.x - xLeftOld, leftHand.rect.y - yLeftOld};
        leftHand.heldBall->xVel = vector.x * handThrowForce;
        leftHand.heldBall->yVel = vector.y * handThrowForce * 1.2; 
        
        leftHand.heldBall = NULL; 
    }
    if(rightHand.heldBall != NULL && input.rTrigHeld < 10){
        rightHand.heldBall->isHeld = false;
        
        // Draw vector with last position and send ball that way.
        SDL_FRect vector = {rightHand.rect.x - xRightOld, rightHand.rect.y - yRightOld};
        rightHand.heldBall->xVel = vector.x * handThrowForce;
        rightHand.heldBall->yVel = vector.y * handThrowForce * 1.2; 

        rightHand.heldBall = NULL; 
    }

    // ball fisics (2)   
    
    // If a ball is being held, then just move it with its hand.
    // Otherwise, simulate in the air.
    
    for(int i=0; i<balls.size(); i++){
        Ball* ball = balls[i];
        if(ball->isHeld){
            if(ball == leftHand.heldBall){
                ball->rect.x = leftHand.rect.x;
                ball->rect.y = leftHand.rect.y;
            } else if(ball == rightHand.heldBall){
                ball->rect.x = rightHand.rect.x;
                ball->rect.y = rightHand.rect.y;
            }
        } else {
            // This ball is in the air.
            //ball->yVel += dt * 1600; // apply gravity
            ball->yVel += dt * gravity;
            ball->rect.y += ball->yVel * dt;
            ball->rect.x += ball->xVel * dt;

            // Pickup ball if colliding with hand (and trigger pressed)
            // TODO: differentiate  between "trigger held" and "trigger newly pushed"
            if(
                isColliding(rightHand.rect, ball->rect) && 
                input.rTrigHeld > 10 &&
                rightHand.heldBall == NULL 
            ){
                ball->isHeld = true;
                rightHand.heldBall = ball;
                ball->xVel = 0;
                ball->yVel = 0;
            } else if(
                isColliding(leftHand.rect, ball->rect) && 
                input.lTrigHeld > 10 &&
                leftHand.heldBall == NULL
            ){
                ball->isHeld = true;
                leftHand.heldBall = ball;
                ball->xVel = 0;
                ball->yVel = 0;
            }
        } 
    }
}

void render(){
    // Clear window before rendering
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_GetWindowSize(window, &WINDOW_W, &WINDOW_H);

    // Show debug text (leaving here for later)
    /*
    float x, y;
    x = (((float) winw) - (SDL_strlen(text) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE)) / 2.0f;
    y = (((float) winh) - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) / 2.0f;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(renderer, x, y, text);
    */
    

    // Render hands
    //SDL_RenderTexture(renderer, hands, NULL, NULL);
    SDL_FRect rhRect = {rightHand.rect.x, rightHand.rect.y, 150, 150};
    SDL_RenderTexture(renderer, handsTexture, &(rightHand.srcRect), &rhRect);
    SDL_FRect lhRect = {leftHand.rect.x, leftHand.rect.y, 150, 150};
    SDL_RenderTexture(renderer, handsTexture, &(leftHand.srcRect), &lhRect);

    // Render balls
    for(int i=0; i<balls.size(); i++){
        Ball* ball = balls[i];
        SDL_FRect ballRect = {ball->rect.x, ball->rect.y, ball->rect.w, ball->rect.h};
        SDL_RenderTexture(renderer, ballTexture, NULL, &ballRect);
    }

    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderRect(renderer, &(rightHand.rect));
    SDL_RenderRect(renderer, &(leftHand.rect));

    // Just for debugging
    for(int i=0; i<balls.size(); i++){
        Ball* ball = balls[i];
        SDL_RenderRect(renderer, &(ball->rect));
    }

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    float x, y;
    for(int i=0; i<balls.size(); i++){
        // Draw ball info
        Ball* ball = balls[i];
        std::string ballAddressStr = std::to_string(reinterpret_cast<uintptr_t>(ball));
        const char* ballAddressCStr = ballAddressStr.c_str();

        x = ball->rect.x + ball->rect.w;
        y = ball->rect.y + ball->rect.w;
        std::string xStr = std::to_string(x);
        std::string yStr = std::to_string(y);
        std::string xyStr = "pos: "+ xStr + ", " + yStr;
        const char* xyCStr = xyStr.c_str(); 

        std::string xVelStr = std::to_string(ball->xVel);
        std::string yVelStr = std::to_string(ball->yVel);
        std::string xyVelStr = "velocity: "+ xVelStr + ", " + yVelStr;
        const char* xyCVelStr = xyVelStr.c_str(); 

        std::string isHeldStr = std::to_string(ball->isHeld);
        const char* isHeldCStr = isHeldStr.c_str();

        SDL_RenderDebugText(renderer, x, y-15, ballAddressCStr);
        SDL_RenderDebugText(renderer, x, y, xyCStr);
        SDL_RenderDebugText(renderer, x, y+15, xyCVelStr);
        SDL_RenderDebugText(renderer, x, y+30, isHeldCStr);
    }
    // debugging ends

    // Push everything in buffered renderer to front.
    SDL_RenderPresent(renderer);
}

GamepadInfo getGamepadInfo(){
    if (!gamepad) {  /* we have a stick opened? */
        return {0, 0, 0, 0, false};
    }

    Sint16 leftStickX = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX);
    Sint16 leftStickY = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY);
    Sint16 rightStickX = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX);
    Sint16 rightStickY = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY);

    Sint16 leftTrigger = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    Sint16 rightTrigger = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);

    // Keep values in between -100 and 100.
    // Need to ignore "deadzone" values (around -5 to 5)
    Sint16 lsX = (leftStickX / 32767.0f) * 100;
    Sint16 lsY = (leftStickY / 32767.0f) * 100;
    
    Sint16 rsX = (rightStickX / 32767.0f) * 100;
    Sint16 rsY = (rightStickY / 32767.0f) * 100;
    
    Sint16 lTrigger = (leftTrigger / 32767.0f) * 100;
    Sint16 rTrigger = (rightTrigger / 32767.0f) * 100;

    GamepadInfo info = {lsX, lsY, rsX, rsY, leftTrigger, rightTrigger, true};
    return info;
}


bool isColliding(SDL_FRect rect1, SDL_FRect rect2){
    if(rect1.x < rect2.x+rect2.w && rect1.x+rect2.w > rect2.x &&
        rect1.y < rect2.y+rect2.h && rect1.y+rect2.h > rect2.y){
        return true;
    }
    return false;
}

// This doesn't handle any gamepad events, that's done in getGamepadInfo().
// TODO: consider if this should be changed/refactored.
int handleEvent(SDL_Event event){
    if (event.type == SDL_EVENT_QUIT) {
        /* end the program, reporting success to the OS. */
        return SDL_EVENT_QUIT;
    } else if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
        /* this event is sent for each hotplugged stick, but also each already-connected joystick during SDL_Init(). */
        if (gamepad == NULL) {  /* we don't have a stick yet and one was added, open it! */
            gamepad = SDL_OpenGamepad(event.gdevice.which);
            if(gamepad != NULL){
                printf("Gamepad connected!\n");
            } else {
                printf("Problem connecting with gamepad.\n");
            }
        }
    } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) { // from example
        if (gamepad && (SDL_GetGamepadID(gamepad) == event.gdevice.which)) {
            SDL_CloseGamepad(gamepad);  /* our joystick was unplugged. */
            gamepad = NULL;
            printf("Gamepad removed.\n");
        }
    }
    return 0;
}


