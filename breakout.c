#include <asm-generic/errno-base.h>
#include <inttypes.h>
#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <xmmintrin.h>

#define CIRCLE_RECT_COLLISION_EPSILON 0.000001f

#define VEC2_ZERO (Vector2){0.0f, 0.0f};

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 450

#define MAX_LIVES 5
#define MAX_POWERUPS 6

#define BALL_SPEED 200.0f
#define BALL_COLOR GRAY
#define BALL_RADIUS 8

#define BRICK_WIDTH 50.0f
#define BRICK_HEIGHT 20.0f
#define BRICK_HGAP 3
#define BRICK_VGAP 3
#define BRICK_VPAD 60
#define BRICK_HPAD (SCREEN_WIDTH - (BRICK_HCOUNT * (BRICK_WIDTH + BRICK_HGAP) - BRICK_HGAP)) / 2.0f
#define BRICK_HCOUNT 13
#define BRICK_VCOUNT 8
#define BRICK_COLOR LIGHTGRAY
#define BRICK_COLOR_ALT DARKGRAY

static inline float RSqrt(float x) {
    __m128 a = _mm_set_ss(x);
    float res = 0.0f;

    a = _mm_rsqrt_ss(a);
    _mm_store_ss(&res, a);

    return res;
}

static inline void Normalize2(Vector2 *vec) {
    if (vec == NULL) return;

    float rsqrt = RSqrt(vec->x * vec->x + vec->y * vec->y);
    vec->x *= rsqrt;
    vec->y *= rsqrt;
}

typedef struct GameState {
    bool gameOver;
    int points;
} GameState;

typedef struct Player {
    Rectangle rect;
    Color color;
    float speed;
    int lives;
} Player;

typedef struct Ball {
    int radius;
    Vector2 pos;
    Vector2 prevPos;
    Vector2 velocity;
    float speed;
    bool enabled;
} Ball;

// TODO: Maybe add power-ups
// At any time of the game, power-ups should
// appear over a brick (+- Player Size, +- Player Speed, + Life etc.)
typedef struct Brick {
    Rectangle rect;
    Color color;
    bool enabled;
} Brick;

typedef struct BrickWall {
    Brick **bricks;
    int remaining;
} BrickWall;

typedef void (*PowerUpF)(Player *player, Ball *ball);
typedef struct PowerUp {
    PowerUpF apply;
    const char *display;
    int threshold;
    bool acquired;
    float displayTimer;
} PowerUp;

void PowerUpIncPlayerSize(Player *player, Ball *ball);
void PowerUpIncPlayerSize2(Player *player, Ball *ball);

void PowerUpIncPlayerSpeed(Player *player, Ball *ball);
void PowerUpIncPlayerSpeed2(Player *player, Ball *ball);

void PowerUpDecPlayerSize(Player *player, Ball *ball);
void PowerUpDecPlayerSize2(Player *player, Ball *ball);

void PowerUpIncBallSpeed(Player *player, Ball *ball);
void PowerUpIncBallSpeed2(Player *player, Ball *ball);

void PowerUpDecBallSpeed(Player *player, Ball *ball);
void PowerUpDecBallSpeed2(Player *player, Ball *ball);

int InitGameState(GameState *state);

int InitPlayer(Player *player);
void DrawPlayer(const Player *player);
void UpdatePlayer(Player *player, float deltaTime);
Vector2 PlayerBottomMid(const Player *player);
Rectangle PlayerRect(const Player *player);

int InitBall(Ball *ball, const Player *player);
void DrawBall(const Ball *ball);
void UpdateBall(Ball *ball, Player *player, BrickWall *wall, GameState *state, float deltaTime);
void BallHandlePlayerCollision(Ball *ball, const Player *player);
void BallHandleBrickCollision(Ball *ball, const Brick *brick);
void BallHandleBrickCollisionAlt(Ball *ball, const Brick *brick);
void BallHandleArenaCollision(Ball *ball, Player *player, GameState *state);

int InitBrickWall(BrickWall *wall);
void DrawBrickWall(const BrickWall *wall);
Brick *BallCheckWallCollision(const BrickWall *wall, const Ball *ball);

int main(void) {
    const int width = 800;
    const int height = 450;

    SetTraceLogLevel(LOG_DEBUG);
    InitWindow(width, height, "Breakout");

    SetTargetFPS(60);

    GameState state;
    InitGameState(&state);

    Player player;
    InitPlayer(&player);

    Ball ball;
    InitBall(&ball, &player);

    BrickWall wall;
    InitBrickWall(&wall);

    size_t maxPoints = BRICK_HCOUNT * BRICK_VCOUNT;
    PowerUp powerUps[MAX_POWERUPS] = {
        (PowerUp){&PowerUpIncPlayerSpeed, "+ Speed", 0, false, 2.0f},
        (PowerUp){&PowerUpIncPlayerSpeed, "+ Speed", 0, false, 2.0f},
        (PowerUp){&PowerUpIncPlayerSize, "+ Size", 0, false, 2.0f},
        (PowerUp){&PowerUpIncPlayerSpeed, "+ Speed", 0, false, 2.0f},
        (PowerUp){&PowerUpIncPlayerSpeed2, "++ Speed", 0, false, 2.0f},
        (PowerUp){&PowerUpIncPlayerSize, "+ Size", 0, false, 2.0f},
    };

    for (size_t i = 0; i < MAX_POWERUPS; i++) {
        powerUps[i].threshold = (i + 1) * maxPoints / (MAX_POWERUPS + 1);
    }

    while (!WindowShouldClose()) {
        // Update
        if (state.gameOver) goto render;

        float deltaTime = GetFrameTime();

        UpdatePlayer(&player, deltaTime);
        UpdateBall(&ball, &player, &wall, &state, deltaTime);

        // Reward player
        for (size_t i = 0; i < MAX_POWERUPS; i++) {
            if (powerUps[i].acquired) {
                powerUps[i].displayTimer -= deltaTime;
                continue;
            }
            if (powerUps[i].threshold > state.points) continue;

            powerUps[i].acquired = true;
            powerUps[i].apply(&player, &ball);
        }

        // Game over if no bricks are remaining or no lives are left
        if (wall.remaining == 0 || player.lives == 0) {
            state.gameOver = true;
        }

        // Update points
        char pointsDisplay[16] = {0};
        snprintf(pointsDisplay, 15, "Points: %d", state.points);

        char speedDisplay[16] = {0};
        snprintf(speedDisplay, 15, "Speed: %.2f", player.speed);

        char sizeDisplay[16] = {0};
        snprintf(sizeDisplay, 15, "Size: %.2f", player.rect.width);

        // Render
    render:
        BeginDrawing();

        ClearBackground(RAYWHITE);
        DrawFPS(715, 10);
        DrawText(pointsDisplay, 10, 10, 20, DARKGREEN);

        DrawPlayer(&player);
        DrawBall(&ball);
        DrawBrickWall(&wall);

        // Draw lives
        for (int i = 0; i < player.lives; i++) {
            const int livesGap = 5.0f;
            Rectangle liveRec = {10.0f + i * (30.0f + livesGap), 435.0f, 30.0f, 10.0f};
            DrawRectangleRec(liveRec, RED);
        }

        for (size_t i = 0, j = 0; i < MAX_POWERUPS; i++) {
            if (!powerUps[i].acquired || powerUps[i].displayTimer <= 0) continue;

            int width = MeasureText(powerUps[i].display, 20);
            DrawText(powerUps[i].display, SCREEN_WIDTH - width - 5, 430 - j * 25, 20, DARKGREEN);
            j++;
        }

        DrawText(sizeDisplay, 10, 385, 20, DARKGREEN);
        DrawText(speedDisplay, 10, 410, 20, DARKGREEN);

        if (state.gameOver) {
            DrawText("Game Over!", 450, 240, 50, LIGHTGRAY);
            DrawText(pointsDisplay, 525, 300, 30, LIGHTGRAY);
        }

        EndDrawing();
    }

    CloseWindow();

    return 0;
}

void PowerUpIncPlayerSize(Player *player, Ball *ball) {
    if (player == NULL) return;
    (void)ball;

    player->rect.width += 35.0f;
}

void PowerUpIncPlayerSize2(Player *player, Ball *ball) {
    if (player == NULL) return;
    (void)ball;

    player->rect.width += 65.0f;
}

void PowerUpIncPlayerSpeed(Player *player, Ball *ball) {
    if (player == NULL) return;
    (void)ball;

    player->speed += 25.0f;
}

void PowerUpIncPlayerSpeed2(Player *player, Ball *ball) {
    if (player == NULL) return;
    (void)ball;

    player->speed += 50.0f;
}

void PowerUpDecPlayerSize(Player *player, Ball *ball) {
    if (player == NULL) return;
    (void)ball;

    player->rect.width -= 25.0f;
}

void PowerUpDecPlayerSize2(Player *player, Ball *ball) {
    if (player == NULL) return;
    (void)ball;

    player->rect.width -= 40.0f;
}

void PowerUpIncBallSpeed(Player *player, Ball *ball) {
    if (ball == NULL) return;
    (void)player;

    ball->speed += 2.0f;
}

void PowerUpIncBallSpeed2(Player *player, Ball *ball) {
    if (ball == NULL) return;
    (void)player;

    ball->speed += 5.0f;
}

void PowerUpDecBallSpeed(Player *player, Ball *ball) {
    if (ball == NULL) return;
    (void)player;

    ball->speed -= 2.0f;
}

void PowerUpDecBallSpeed2(Player *player, Ball *ball) {
    if (ball == NULL) return;
    (void)player;

    ball->speed -= 5.0f;
}

int InitGameState(GameState *state) {
    if (state == NULL) return EINVAL;

    state->gameOver = false;
    state->points = 0;

    return 0;
}

int InitPlayer(Player *player) {
    if (player == NULL) return EINVAL;

    player->rect = (Rectangle){350.0f, 410.0f, 100.0f, 20.0f};
    player->speed = 200.0f;
    player->color = GRAY;
    player->lives = MAX_LIVES;

    return 0;
}

void DrawPlayer(const Player *player) {
    if (player == NULL) return;

    DrawRectangleRec(player->rect, player->color);
}

void UpdatePlayer(Player *player, float deltaTime) {
    if (player == NULL) return;

    float movement = player->speed * deltaTime;

    if (IsKeyDown(KEY_RIGHT)) {
        player->rect.x += movement;
    }

    if (IsKeyDown(KEY_LEFT)) {
        player->rect.x -= movement;
    }

    int screenWidth = GetScreenWidth();
    if (player->rect.x < 0.0f) {
        player->rect.x = 0.0f;
    } else if (player->rect.x + player->rect.width >= screenWidth) {
        player->rect.x = screenWidth - player->rect.width;
    }
}

Vector2 PlayerBottomMid(const Player *player) {
    if (player == NULL) return (Vector2){0.0f, 0.0f};

    return (Vector2){
        player->rect.x + player->rect.width / 2.0f,
        player->rect.y + player->rect.height,
    };
}

Rectangle PlayerRect(const Player *player) {
    if (player == NULL) {
        return (Rectangle){0};
    }

    return (Rectangle){
        player->rect.x,
        player->rect.y,
        player->rect.width,
        player->rect.height,
    };
}

int InitBall(Ball *ball, const Player *player) {
    if (ball == NULL) return EINVAL;
    if (player == NULL) return EINVAL;

    ball->radius = BALL_RADIUS;
    ball->pos = (Vector2){PlayerBottomMid(player).x, player->rect.y - 15.0f};
    ball->prevPos = ball->pos;
    ball->speed = 0.0f;
    ball->velocity = VEC2_ZERO;
    ball->enabled = false;

    return 0;
}

void DrawBall(const Ball *ball) {
    if (ball == NULL) return;

    DrawCircleV(ball->pos, (float)ball->radius, BALL_COLOR);
}

void UpdateBall(Ball *ball, Player *player, BrickWall *wall, GameState *state, float deltaTime) {
    if (ball == NULL) return;
    if (player == NULL) return;

    // If ball is not enabled, either start by pressing SPACE
    // or attach the ball to the player
    if (!ball->enabled && IsKeyPressed(KEY_SPACE)) {
        ball->enabled = true;
        ball->speed = BALL_SPEED;
        ball->velocity.x = 0.0f;
        ball->velocity.y = -1.0f;
    } else if (!ball->enabled) {
        ball->pos.x = PlayerBottomMid(player).x;
        return;
    }

    // Check and handle collision with player
    BallHandlePlayerCollision(ball, player);

    // Check if we hit a brick
    Brick *collided = BallCheckWallCollision(wall, ball);
    if (collided != NULL) {
        collided->enabled = false;
        wall->remaining -= 1;
        state->points += 1;

        // Handle ball bounce from brick
        BallHandleBrickCollision(ball, collided);
    }

    // Handle ball bounce of arena walls
    BallHandleArenaCollision(ball, player, state);

    // Ball movement by velocity (normalized) and speed
    float speed = ball->speed * deltaTime;
    ball->prevPos = ball->pos;
    ball->pos.x += ball->velocity.x * speed;
    ball->pos.y += ball->velocity.y * speed;
}

void BallHandlePlayerCollision(Ball *ball, const Player *player) {
    if (ball == NULL) return;
    if (player == NULL) return;

    // No collision means nothing to do
    if (!CheckCollisionCircleRec(ball->prevPos, ball->radius, PlayerRect(player))) return;

    // If there is a collision, reflect the ball at an angle
    // with the bottom center of player. Velocity is normalized
    Vector2 collisionPoint = {ball->pos.x, ball->pos.y - ball->radius};
    Vector2 playerBottomMid = PlayerBottomMid(player);
    ball->velocity.x = collisionPoint.x - playerBottomMid.x;
    ball->velocity.y = collisionPoint.y - playerBottomMid.y;
    Normalize2(&(ball->velocity));

    // Increase ball speed in contact with player
    ball->speed += 5.0f;
}

void BallHandleBrickCollision(Ball *ball, const Brick *brick) {
    if (ball == NULL) return;
    if (brick == NULL) return;

    // Brick edge positions

    float sides[4] = {
        brick->rect.x,                      // left
        brick->rect.y,                      // top
        brick->rect.x + brick->rect.width,  // right
        brick->rect.y + brick->rect.height, // bottom
    };
    bool sideCollision[4] = {
        (ball->prevPos.x < sides[0]) && ball->velocity.x > EPSILON,  // left
        (ball->prevPos.y < sides[1]) && ball->velocity.y > EPSILON,  // top
        (ball->prevPos.x > sides[2]) && ball->velocity.x < -EPSILON, // right
        (ball->prevPos.y > sides[3]) && ball->velocity.y < -EPSILON, // bottom
    };

    if (sideCollision[0] && sideCollision[1]) { // left-top
        Vector2 distance = (Vector2){
            sides[0] - ball->prevPos.x, // distance to left
            sides[1] - ball->prevPos.y, // distance to top
        };

        if (distance.y < distance.x - CIRCLE_RECT_COLLISION_EPSILON) {
            // Left - closer to top
            ball->velocity.x = -ball->velocity.x;
            ball->pos.x = sides[0] - ball->radius;
        } else if (distance.y > distance.x + CIRCLE_RECT_COLLISION_EPSILON) {
            // Top - closer to left
            ball->velocity.y = -ball->velocity.y;
            ball->pos.y = sides[1] - ball->radius;
        } else {
            // Top-Left Corner - both distances are equal by a margin of CIRCLE_RECT_COLLISION_EPSILON
            ball->velocity = (Vector2){
                -ball->velocity.x,
                -ball->velocity.y,
            };
            ball->pos = (Vector2){
                sides[0] - ball->radius,
                sides[1] - ball->radius,
            };
        }
    } else if (sideCollision[0] && sideCollision[3]) { // left-bottom
        Vector2 distance = (Vector2){
            sides[0] - ball->prevPos.x, // distance to left
            ball->prevPos.y - sides[3], // distance to bottom
        };

        if (distance.y < distance.x - CIRCLE_RECT_COLLISION_EPSILON) {
            // Left - closer to bottom
            ball->velocity.x = -ball->velocity.x;
            ball->pos.x = sides[0] - ball->radius;
        } else if (distance.y > distance.x + CIRCLE_RECT_COLLISION_EPSILON) {
            // Bottom - closer to left
            ball->velocity.y = -ball->velocity.y;
            ball->pos.y = sides[3] + ball->radius;
        } else {
            // Bottom-Left Corner - both distances are equal by a margin of CIRCLE_RECT_COLLISION_EPSILON
            ball->velocity = (Vector2){
                -ball->velocity.x,
                -ball->velocity.y,
            };
            ball->pos = (Vector2){
                sides[0] - ball->radius,
                sides[3] + ball->radius,
            };
        }
    } else if (sideCollision[2] && sideCollision[1]) { // right-top
        Vector2 distance = (Vector2){
            ball->prevPos.x - sides[2], // distance to right
            sides[1] - ball->prevPos.y, // distance to top
        };

        if (distance.y < distance.x - CIRCLE_RECT_COLLISION_EPSILON) {
            // Right - closer to top
            ball->velocity.x = -ball->velocity.x;
            ball->pos.x = sides[2] + ball->radius;
        } else if (distance.y > distance.x + CIRCLE_RECT_COLLISION_EPSILON) {
            // Top - closer to right
            ball->velocity.y = -ball->velocity.y;
            ball->pos.y = sides[1] - ball->radius;
        } else {
            // Top-Right Corner - both distances are equal by a margin of CIRCLE_RECT_COLLISION_EPSILON
            ball->velocity = (Vector2){
                -ball->velocity.x,
                -ball->velocity.y,
            };
            ball->pos = (Vector2){
                sides[2] + ball->radius,
                sides[1] - ball->radius,
            };
        }
    } else if (sideCollision[2] && sideCollision[3]) { // right-bottom
        Vector2 distance = (Vector2){
            ball->prevPos.x - sides[2], // distance to right
            ball->prevPos.y - sides[3], // distance to bottom
        };

        if (distance.y < distance.x - CIRCLE_RECT_COLLISION_EPSILON) {
            // Right - closer to bottom
            ball->velocity.x = -ball->velocity.x;
            ball->pos.x = sides[2] + ball->radius;
        } else if (distance.y > distance.x + CIRCLE_RECT_COLLISION_EPSILON) {
            // Bottom - closer to right
            ball->velocity.y = -ball->velocity.y;
            ball->pos.y = sides[3] + ball->radius;
        } else {
            // Bottom-Right Corner - both distances are equal by a margin of CIRCLE_RECT_COLLISION_EPSILON
            ball->velocity = (Vector2){
                -ball->velocity.x,
                -ball->velocity.y,
            };
            ball->pos = (Vector2){
                sides[2] + ball->radius,
                sides[3] + ball->radius,
            };
        }
    } else if (sideCollision[0]) { // left
        ball->velocity.x = -ball->velocity.x;
        ball->pos.x = sides[0] - ball->radius;
    } else if (sideCollision[2]) { // right
        ball->velocity.x = -ball->velocity.x;
        ball->pos.x = sides[2] + ball->radius;
    } else if (sideCollision[1]) { // top
        ball->velocity.y = -ball->velocity.y;
        ball->pos.y = sides[1] - ball->radius;
    } else if (sideCollision[3]) { // bottom
        ball->velocity.y = -ball->velocity.y;
        ball->pos.y = sides[3] + ball->radius;
    }

    // Incrase ball speed in contact with bricks
    ball->speed += 2.0f;
}

void BallHandleArenaCollision(Ball *ball, Player *player, GameState *state) {
    if (ball == NULL) return;
    if (player == NULL) return;

    int width = GetScreenWidth();
    int height = GetScreenHeight();

    if (ball->prevPos.x < ball->radius) {
        // Left/Right wall collision
        ball->velocity.x = -ball->velocity.x;
        ball->pos.x = ball->radius;
    } else if (ball->prevPos.x > width - ball->radius) {
        // Right wall collision
        ball->velocity.x = -ball->velocity.x;
        ball->pos.x = width - ball->radius;
    }

    // Separate `if` for corners
    if (ball->prevPos.y < ball->radius) {
        // Top wall collision
        ball->velocity.y = -ball->velocity.y;
        ball->pos.y = ball->radius;
    } else if (ball->prevPos.y > height - ball->radius) {
        // Bottom wall collision, reduce one life and reset ball
        player->lives -= 1;
        if (state != NULL) state->points -= 10;

        InitBall(ball, player);
    }
}

int InitBrickWall(BrickWall *wall) {
    if (wall == NULL) return EINVAL;

    *wall = (BrickWall){NULL, BRICK_HCOUNT * BRICK_VCOUNT};

    bool altStart = false;

    wall->bricks = (Brick **)calloc(BRICK_VCOUNT, sizeof(Brick *));
    for (int r = 0; r < BRICK_VCOUNT; r++) {
        wall->bricks[r] = (Brick *)calloc(BRICK_HCOUNT, sizeof(Brick));

        bool alt = altStart;
        for (int c = 0; c < BRICK_HCOUNT; c++) {
            wall->bricks[r][c].rect = (Rectangle){
                BRICK_HPAD + c * (BRICK_WIDTH + BRICK_HGAP),
                BRICK_VPAD + r * (BRICK_HEIGHT + BRICK_VGAP),
                BRICK_WIDTH,
                BRICK_HEIGHT,
            };
            wall->bricks[r][c].color = alt ? BRICK_COLOR_ALT : BRICK_COLOR;
            wall->bricks[r][c].enabled = true;

            alt = !alt;
        }
        altStart = !altStart;
    }

    return 0;
}

void DrawBrickWall(const BrickWall *wall) {
    if (wall == NULL) return;

    for (int r = 0; r < BRICK_VCOUNT; r++) {
        for (int c = 0; c < BRICK_HCOUNT; c++) {
            Brick *brick = &(wall->bricks[r][c]);
            if (!brick->enabled) continue;

            DrawRectangleRec(brick->rect, brick->color);
        }
    }
}

Brick *BallCheckWallCollision(const BrickWall *wall, const Ball *ball) {
    if (wall == NULL) return NULL;
    if (ball == NULL) return NULL;

    for (int r = 0; r < BRICK_VCOUNT; r++) {
        for (int c = 0; c < BRICK_HCOUNT; c++) {
            Brick *brick = &(wall->bricks[r][c]);
            if (!brick->enabled) continue;
            if (!CheckCollisionCircleRec(ball->prevPos, ball->radius, brick->rect)) continue;

            return brick;
        }
    }

    return NULL;
}
