#include <stdio.h>
#include <vector>
#include <string>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>

typedef struct {
    Sint16 lsX, lsY, rsX, rsY;
    bool lTrigPressed, lTrigHeld;
    bool rTrigPressed, rTrigHeld;
    bool isSouthBtnHeld;
} GamepadInfo;

typedef struct {
    SDL_FRect rect;
    float xVel, yVel;
    bool isHeld;
    Uint64 lastTimeHeld;
} Ball;

enum HandStatus {
    OPEN,
    CLOSING,
    CLOSED,
    OPENING
};

typedef struct {
    SDL_FRect rect;
    SDL_FRect srcRect; // rect on texture to render
    SDL_FRect prevPos;
    Uint32 prevPosTicks;
    Ball* heldBall;
    HandStatus status;
    Uint64 closingStartTicks;
    Uint64 openingStartTicks;
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
Hand* hands[2] = {&rightHand, &leftHand};
//hands[1] = &leftHand;
//Ball ball = {{600, 0, 150, 150}, 0, 0, false};
std::vector<Ball*> balls;

int handSpeed = 17;
float handThrowForce = 42.f;
float handThrowForceVerticalMultiplier = 1.8;
int gravity = 1600;
//int gravity = 800;
//int gravity = 15;

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
GamepadInfo getGamepadInfo(GamepadInfo prevInput);
bool isColliding(SDL_FRect rect1, SDL_FRect rect2);
int handleEvent(SDL_Event event);
void catchBallIfPossible(GamepadInfo input, Ball* ball);
void resetBallPositions();
void releaseBalls(GamepadInfo input, SDL_FRect oldLeftHandPos, SDL_FRect oldRightHandPos);
void renderDebugStuff();
void updateHandState(GamepadInfo input);

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
    GamepadInfo currInput = {};
    GamepadInfo prevInput = {};

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
        prevInput = currInput;
        currInput = getGamepadInfo(prevInput);
        //printf("%i, %i\n", prevInput.lTrigHeld, prevInput.rTrigHeld);
        // input handling (end)

        simulate(currInput);

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

    // reset ball positions if pressed
    if(input.isSouthBtnHeld){
       resetBallPositions(); 
    }

    SDL_FRect oldLeftHandPos = leftHand.rect;
    SDL_FRect oldRightHandPos = rightHand.rect;

    // Move hands from joystick movement
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

    releaseBalls(input, oldLeftHandPos, oldRightHandPos);


    // ball fisics
    // loop through all balls and simulate 
    for(int i=0; i<balls.size(); i++){
        Ball* ball = balls[i];
        if(ball->isHeld){
            // If a ball is being held, then just move it with its hand.
            if(ball == leftHand.heldBall){
                ball->rect.x = leftHand.rect.x;
                ball->rect.y = leftHand.rect.y;
            } else if(ball == rightHand.heldBall){
                ball->rect.x = rightHand.rect.x;
                ball->rect.y = rightHand.rect.y;
            }
        } else {
            // This ball is in the air.
            ball->yVel += dt * gravity;
            ball->rect.y += ball->yVel * dt;
            ball->rect.x += ball->xVel * dt;

            // Pickup ball if colliding with hand (and trigger pressed)
            // TODO: differentiate  between "trigger held" and "trigger newly pushed"
            catchBallIfPossible(input, ball);
        } 
    }

    updateHandState(input);
}

void render(){
    // Clear window before rendering
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_GetWindowSize(window, &WINDOW_W, &WINDOW_H);

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

    renderDebugStuff();

    // Push everything in buffered renderer to front.
    SDL_RenderPresent(renderer);
}

GamepadInfo getGamepadInfo(GamepadInfo prevInput){
    if (!gamepad) {  /* we have a stick opened? */
        return {0, 0, 0, 0};
    }

    Sint16 leftStickX = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX);
    Sint16 leftStickY = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY);
    Sint16 rightStickX = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX);
    Sint16 rightStickY = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY);

    Sint16 leftTrigger = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    Sint16 rightTrigger = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);

    bool isSouthBtnPressed = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH);

    // Keep values in between -100 and 100.
    // Need to ignore "deadzone" values (around -5 to 5)
    Sint16 lsX = (leftStickX / 32767.0f) * 100;
    Sint16 lsY = (leftStickY / 32767.0f) * 100;
    
    Sint16 rsX = (rightStickX / 32767.0f) * 100;
    Sint16 rsY = (rightStickY / 32767.0f) * 100;
    
    Sint16 lTrigger = (leftTrigger / 32767.0f) * 100;
    Sint16 rTrigger = (rightTrigger / 32767.0f) * 100;
    
    bool lTrigHeld = lTrigger > 10;
    bool rTrigHeld = rTrigger > 10;
    
    bool lTrigPressed = false;
    bool rTrigPressed = false;

    // Now determining if triggers have been pressed right now.
    if(lTrigHeld && !prevInput.lTrigHeld){
        lTrigPressed = true; 
    } 

    if(rTrigHeld && !prevInput.rTrigHeld){
        rTrigPressed = true;
    }

    GamepadInfo inputInfo = {lsX, lsY, rsX, rsY, 
                            lTrigPressed, lTrigHeld, rTrigPressed, rTrigHeld,
                            isSouthBtnPressed};
    return inputInfo;
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

// hardcoded for 3 balls
void resetBallPositions(){
    balls[0]->rect = {rightHand.rect.x, rightHand.rect.y-1000, 150, 150};
    balls[0]->xVel = 0;
    balls[0]->yVel = 0;
    balls[0]->isHeld = false;
    balls[1]->rect = {leftHand.rect.x, leftHand.rect.y-200, 150, 150};
    balls[1]->xVel = 0;
    balls[1]->yVel = 0;
    balls[1]->isHeld = false;
    balls[2]->rect = {rightHand.rect.x, rightHand.rect.y-200, 150, 150};
    balls[2]->xVel = 0;
    balls[2]->yVel = 0;
    balls[2]->isHeld = false;
}

void catchBallIfPossible(
    GamepadInfo input,
    Ball* ball
){
    if(ball->lastTimeHeld + 400 > SDL_GetTicks()){
        // Ball cannot be caught yet, its been released recently. 
        return;
    }

    if(
        isColliding(rightHand.rect, ball->rect) && 
        rightHand.status == CLOSING &&
        rightHand.heldBall == NULL 
    ){
        ball->isHeld = true;
        rightHand.heldBall = ball;
        rightHand.status = CLOSED;
        ball->xVel = 0;
        ball->yVel = 0;
    } else if(
        isColliding(leftHand.rect, ball->rect) && 
        leftHand.status == CLOSING &&
        leftHand.heldBall == NULL
    ){
        ball->isHeld = true;
        leftHand.heldBall = ball;
        leftHand.status = CLOSED;
        ball->xVel = 0;
        ball->yVel = 0;
    }
}

void releaseBalls(GamepadInfo input, SDL_FRect oldLeftHandPos, SDL_FRect oldRightHandPos){
    // Check if either hand collides and put ball in caught state with the hand
    // Draw vector with last position and send ball that way.
    if(leftHand.heldBall != NULL && !input.lTrigHeld){
        leftHand.heldBall->isHeld = false;
        
        SDL_FRect vector = {
            leftHand.rect.x - oldLeftHandPos.x, 
            leftHand.rect.y - oldLeftHandPos.y
        };
        leftHand.heldBall->xVel = vector.x * handThrowForce;
        leftHand.heldBall->yVel = vector.y * handThrowForce * handThrowForceVerticalMultiplier; 
        leftHand.heldBall->lastTimeHeld = SDL_GetTicks();
        
        leftHand.heldBall = NULL; 
    }
    if(rightHand.heldBall != NULL && !input.rTrigHeld){
        rightHand.heldBall->isHeld = false;
        
        // Draw vector with last position and send ball that way.
        SDL_FRect vector = {
            rightHand.rect.x - oldRightHandPos.x, 
            rightHand.rect.y - oldRightHandPos.y
        };
        rightHand.heldBall->xVel = vector.x * handThrowForce;
        rightHand.heldBall->yVel = vector.y * handThrowForce * handThrowForceVerticalMultiplier; 
        rightHand.heldBall->lastTimeHeld = SDL_GetTicks();

        rightHand.heldBall = NULL; 
    }
}

void renderDebugStuff(){
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
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

        std::string canBeHeldStr = std::to_string(ball->lastTimeHeld + 500 > SDL_GetTicks());
        const char* canBeHeldCStr = canBeHeldStr.c_str(); 

        SDL_RenderDebugText(renderer, x, y-15, ballAddressCStr);
        SDL_RenderDebugText(renderer, x, y, xyCStr);
        SDL_RenderDebugText(renderer, x, y+15, xyCVelStr);
        SDL_RenderDebugText(renderer, x, y+30, isHeldCStr);
        SDL_RenderDebugText(renderer, x, y+45, canBeHeldCStr);
    }

    // Draw hands
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderRect(renderer, &(rightHand.rect));
    SDL_RenderRect(renderer, &(leftHand.rect));
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    for(int i=0; i<2; i++){
        Hand* hand = hands[i];

        std::string heldBallAddressStr = std::to_string(reinterpret_cast<uintptr_t>(hand->heldBall));     
        const char* heldBallAddressCStr = heldBallAddressStr.c_str();

        std::string statusStr; 
        switch(hand->status){
            case OPEN: statusStr = "OPEN"; break;
            case CLOSING: statusStr = "CLOSING"; break;
            case CLOSED: statusStr = "CLOSED"; break;
            case OPENING: statusStr = "OPENING"; break;
        }
        const char* statusCStr = statusStr.c_str();

        SDL_RenderDebugText(renderer, 
            hand->rect.x + hand->rect.w, 
            hand->rect.y, 
            heldBallAddressCStr);

        SDL_RenderDebugText(renderer, 
            hand->rect.x + hand->rect.w, 
            hand->rect.y + 15,
            statusCStr);
    }
}

void updateHandState(GamepadInfo input){
    // Update status depending on triggers.
    // If closing, check ticks to see if its time to close the hand without a ball.

    // Left
    if(input.lTrigPressed && leftHand.status == OPEN){
        leftHand.status = CLOSING;
        leftHand.closingStartTicks = SDL_GetTicks();
    }

    if(input.lTrigHeld && leftHand.status == CLOSING && 
        leftHand.closingStartTicks + 125 < SDL_GetTicks()){
        // the hand is closed now 
        leftHand.status = CLOSED;
        leftHand.closingStartTicks = 0;
    }

    // Start opening
    if(input.lTrigHeld == false && leftHand.status == CLOSED){
        leftHand.status = OPENING;
        leftHand.openingStartTicks = SDL_GetTicks();
    } else if(leftHand.status == OPENING && 
                leftHand.openingStartTicks + 95 < SDL_GetTicks()){
        leftHand.status = OPEN;
        leftHand.openingStartTicks = 0;
        // TODO: if trigger is held when switching from OPENING to OPEN,
        // then we want to instantly transition from OPEN to CLOSING.
        // Game feel thing.
        if(input.lTrigHeld){
            leftHand.status = CLOSING;
            leftHand.closingStartTicks = SDL_GetTicks();
        }
    }

    // Right
    // Left
    if(input.rTrigPressed && rightHand.status == OPEN){
        rightHand.status = CLOSING;
        rightHand.closingStartTicks = SDL_GetTicks();
    }

    if(input.rTrigHeld && rightHand.status == CLOSING && 
        rightHand.closingStartTicks + 125 < SDL_GetTicks()){
        // the hand is closed now 
        rightHand.status = CLOSED;
        rightHand.closingStartTicks = 0;
    }

    // Start opening
    if(input.rTrigHeld == false && rightHand.status == CLOSED){
        rightHand.status = OPENING;
        rightHand.openingStartTicks = SDL_GetTicks();
    } else if(rightHand.status == OPENING && 
                rightHand.openingStartTicks + 95 < SDL_GetTicks()){
        rightHand.status = OPEN;
        rightHand.openingStartTicks = 0;
        // TODO: if trigger is held when switching from OPENING to OPEN,
        // then we want to instantly transition from OPEN to CLOSING.
        // Game feel thing.
        if(input.rTrigHeld){
            rightHand.status = CLOSING;
            rightHand.closingStartTicks = SDL_GetTicks();
        }
    }
    

    // will do same thing for right hand later
}

