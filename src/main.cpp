#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <iostream>
#include <algorithm>
#include <vector>
#include <fstream>
#include <sstream>
#include <cmath>

// Editor tool types
enum EditorTool {
    TOOL_PLATFORM_SOLID,
    TOOL_PLATFORM_FALLTHROUGH,
    TOOL_START,
    TOOL_FINISH,
    TOOL_DELETE
};

// Platform struct with type
struct Platform {
    SDL_Rect rect;
    bool isSolid; // true = solid, false = fall-through
};

const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;
const int WORLD_WIDTH = SCREEN_WIDTH * 8;   // 8 screens wide
const int WORLD_HEIGHT = SCREEN_HEIGHT * 2;  // 2 screens tall
const int GROUND_HEIGHT = 64; // Match ground texture height
const int PLAYER_SIZE = 64;
const int GRID_SIZE = 32;  // Grid snap size
const float GRAVITY = 0.5f;
const float MOVE_SPEED = 5.0f;
const float DOWNWARD_FORCE = 3.0f;   // Force added when pressing space while falling
const float BOUNCE_LEVEL0 = -3.0f;   // Small rebound when failed
const float BOUNCE_LEVEL1 = -10.0f;  // First height
const float BOUNCE_LEVEL2 = -14.0f;  // Second height
const float BOUNCE_LEVEL3 = -18.0f;  // Third height
const float BOUNCE_LEVEL4 = -22.0f;  // Fourth height
const float BOUNCE_LEVEL5 = -26.0f;  // Highest height

int main(int argc, char* argv[]) {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* groundTexture = nullptr;
    const int GROUND_TEXTURE_WIDTH = 256;

    // Player position and physics
    float playerX = SCREEN_WIDTH / 2.0f - PLAYER_SIZE / 2.0f;
    float playerY = WORLD_HEIGHT - GROUND_HEIGHT - PLAYER_SIZE;  // Start at bottom of 2-screen world
    float velocityY = 0.0f;
    bool isGrounded = true;
    bool wasInAir = false;
    int bounceLevel = 0; // 0 = small, 1-5 = progressively higher bounces
    bool spacePressed = false;
    bool spacePressedThisFall = false;
    int faceOffset = 0; // -5 for left, 0 for center, 5 for right
    float lastPlayerX = playerX;
    bool hadSmallRebound = false; // Track if we just did a small rebound

    // Camera - start at bottom of world
    float cameraX = 0;
    float cameraY = SCREEN_HEIGHT;  // Start camera at bottom screen

    // Editor mode variables
    bool editorMode = false;
    EditorTool currentTool = TOOL_PLATFORM_SOLID;
    SDL_Point startPoint = {100, SCREEN_HEIGHT - GROUND_HEIGHT - PLAYER_SIZE};
    SDL_Point finishPoint = {SCREEN_WIDTH - 100, SCREEN_HEIGHT - GROUND_HEIGHT - PLAYER_SIZE};
    bool hasStartPoint = false;
    bool hasFinishPoint = false;

    // Create platforms (now using vector with type)
    std::vector<Platform> platforms;

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Initialize SDL_image (PNG for ground)
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        std::cerr << "SDL_image could not initialize! IMG_Error: " << IMG_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // Initialize SDL_ttf
    if (TTF_Init() == -1) {
        std::cerr << "TTF could not initialize! TTF_Error: " << TTF_GetError() << std::endl;
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // Create window
    window = SDL_CreateWindow(
        "SDL Game",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (window == nullptr) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // Create renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // Load ground texture
    groundTexture = IMG_LoadTexture(renderer, "/Users/nicholas/Documents/code/claude1/assets/ground.png");
    if (groundTexture == nullptr) {
        std::cerr << "Failed to load ground texture! IMG_Error: " << IMG_GetError() << std::endl;
    }

    // Load font - using system font
    TTF_Font* font = TTF_OpenFont("/System/Library/Fonts/Helvetica.ttc", 14);
    if (font == nullptr) {
        std::cerr << "Failed to load font! TTF_Error: " << TTF_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        if (groundTexture) {
            SDL_DestroyTexture(groundTexture);
        }
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // Load level from file on startup
    std::ifstream levelFile("level.txt");
    if (levelFile.is_open()) {
        platforms.clear();
        // Load start point
        levelFile >> hasStartPoint >> startPoint.x >> startPoint.y;
        // Load finish point
        levelFile >> hasFinishPoint >> finishPoint.x >> finishPoint.y;
        // Load platforms
        size_t numPlatforms;
        levelFile >> numPlatforms;
        for (size_t i = 0; i < numPlatforms; i++) {
            Platform p;
            levelFile >> p.rect.x >> p.rect.y >> p.rect.w >> p.rect.h >> p.isSolid;
            platforms.push_back(p);
        }
        levelFile.close();
        std::cout << "Level loaded from level.txt (" << platforms.size() << " platforms)" << std::endl;

        // If start point was set in the file, move player there
        if (hasStartPoint) {
            playerX = startPoint.x;
            playerY = startPoint.y;
            // Update camera to center on start position
            cameraX = playerX + PLAYER_SIZE/2 - SCREEN_WIDTH/2;
            cameraY = playerY + PLAYER_SIZE/2 - SCREEN_HEIGHT/2;
            // Clamp camera to world bounds
            if (cameraX < 0) cameraX = 0;
            if (cameraX > WORLD_WIDTH - SCREEN_WIDTH) cameraX = WORLD_WIDTH - SCREEN_WIDTH;
            if (cameraY < 0) cameraY = 0;
            if (cameraY > WORLD_HEIGHT - SCREEN_HEIGHT) cameraY = WORLD_HEIGHT - SCREEN_HEIGHT;
            std::cout << "Player moved to start point: (" << playerX << ", " << playerY << ")" << std::endl;
        }
    } else {
        std::cout << "No level.txt found - starting with empty level" << std::endl;
    }

    // Main game loop
    bool quit = false;
    SDL_Event event;

    while (!quit) {
        // Handle events
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                quit = true;
            }
            else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    quit = true;
                }
                else if (event.key.keysym.sym == SDLK_e) {
                    editorMode = !editorMode;
                    std::cout << "Editor mode: " << (editorMode ? "ON" : "OFF") << std::endl;
                }
                // Editor tool selection
                else if (editorMode) {
                    if (event.key.keysym.sym == SDLK_1) {
                        currentTool = TOOL_PLATFORM_SOLID;
                        std::cout << "Tool: Solid Platform" << std::endl;
                    }
                    else if (event.key.keysym.sym == SDLK_2) {
                        currentTool = TOOL_PLATFORM_FALLTHROUGH;
                        std::cout << "Tool: Fall-through Platform" << std::endl;
                    }
                    else if (event.key.keysym.sym == SDLK_3) {
                        currentTool = TOOL_START;
                        std::cout << "Tool: Start Point" << std::endl;
                    }
                    else if (event.key.keysym.sym == SDLK_4) {
                        currentTool = TOOL_FINISH;
                        std::cout << "Tool: Finish Point" << std::endl;
                    }
                    else if (event.key.keysym.sym == SDLK_5) {
                        currentTool = TOOL_DELETE;
                        std::cout << "Tool: Delete" << std::endl;
                    }
                    // Camera controls in editor
                    else if (event.key.keysym.sym == SDLK_LEFT) {
                        cameraX -= SCREEN_WIDTH / 2;
                        if (cameraX < 0) cameraX = 0;
                    }
                    else if (event.key.keysym.sym == SDLK_RIGHT) {
                        cameraX += SCREEN_WIDTH / 2;
                        if (cameraX > WORLD_WIDTH - SCREEN_WIDTH) cameraX = WORLD_WIDTH - SCREEN_WIDTH;
                    }
                    else if (event.key.keysym.sym == SDLK_UP) {
                        cameraY -= SCREEN_HEIGHT / 2;
                        if (cameraY < 0) cameraY = 0;
                    }
                    else if (event.key.keysym.sym == SDLK_DOWN) {
                        cameraY += SCREEN_HEIGHT / 2;
                        if (cameraY > WORLD_HEIGHT - SCREEN_HEIGHT) cameraY = WORLD_HEIGHT - SCREEN_HEIGHT;
                    }
                    else if (event.key.keysym.sym == SDLK_s) {
                        // Save level
                        std::ofstream file("level.txt");
                        if (file.is_open()) {
                            // Save start and finish points
                            file << hasStartPoint << " " << startPoint.x << " " << startPoint.y << "\n";
                            file << hasFinishPoint << " " << finishPoint.x << " " << finishPoint.y << "\n";
                            // Save number of platforms
                            file << platforms.size() << "\n";
                            // Save each platform (x, y, w, h, isSolid)
                            for (const auto& p : platforms) {
                                file << p.rect.x << " " << p.rect.y << " " << p.rect.w << " " << p.rect.h << " " << p.isSolid << "\n";
                            }
                            file.close();
                            std::cout << "Level saved to level.txt" << std::endl;
                        }
                    }
                    else if (event.key.keysym.sym == SDLK_l) {
                        // Load level
                        std::ifstream file("level.txt");
                        if (file.is_open()) {
                            platforms.clear();
                            // Load start point
                            file >> hasStartPoint >> startPoint.x >> startPoint.y;
                            // Load finish point
                            file >> hasFinishPoint >> finishPoint.x >> finishPoint.y;
                            // Load platforms
                            size_t numPlatforms;
                            file >> numPlatforms;
                            for (size_t i = 0; i < numPlatforms; i++) {
                                Platform p;
                                file >> p.rect.x >> p.rect.y >> p.rect.w >> p.rect.h >> p.isSolid;
                                platforms.push_back(p);
                            }
                            file.close();
                            std::cout << "Level loaded from level.txt" << std::endl;
                        } else {
                            std::cout << "Could not open level.txt" << std::endl;
                        }
                    }
                }
                else if (event.key.keysym.sym == SDLK_SPACE) {
                    spacePressed = true;
                    // If on ground and not bouncing yet, start with initial bounce
                    if (isGrounded && bounceLevel == 0) {
                        velocityY = BOUNCE_LEVEL1;
                        bounceLevel = 1;
                        isGrounded = false;
                        std::cout << "Initial jump! Starting at level 1" << std::endl;
                    }
                    // If falling and close to ground - haven't pressed space yet
                    else if (!isGrounded && !spacePressedThisFall && velocityY > 0) {
                        // Progressively smaller timing windows (distance from ground) for higher levels
                        float distanceThreshold;
                        if (bounceLevel == 0) distanceThreshold = 80.0f;
                        else if (bounceLevel == 1) distanceThreshold = 70.0f;
                        else if (bounceLevel == 2) distanceThreshold = 60.0f;
                        else if (bounceLevel == 3) distanceThreshold = 50.0f;
                        else if (bounceLevel == 4) distanceThreshold = 40.0f;
                        else distanceThreshold = 30.0f; // Level 5 - hardest

                        // Calculate distance to ground
                        float groundY = WORLD_HEIGHT - GROUND_HEIGHT;
                        float distanceToGround = groundY - (playerY + PLAYER_SIZE);

                        // Check platforms below as well
                        float minDistance = distanceToGround;
                        for (const auto& platform : platforms) {
                            if ((platform.isSolid || bounceLevel > 0) &&
                                playerX + PLAYER_SIZE > platform.rect.x &&
                                playerX < platform.rect.x + platform.rect.w &&
                                playerY + PLAYER_SIZE < platform.rect.y) {
                                float distToPlatform = platform.rect.y - (playerY + PLAYER_SIZE);
                                if (distToPlatform >= 0 && distToPlatform < minDistance) {
                                    minDistance = distToPlatform;
                                }
                            }
                        }

                        if (minDistance <= distanceThreshold && minDistance > 0) {
                            spacePressedThisFall = true;
                            velocityY += DOWNWARD_FORCE;
                            std::cout << "Space pressed near ground! distance: " << minDistance << ", bounceLevel: " << bounceLevel << " (threshold: " << distanceThreshold << ")" << std::endl;
                        }
                    }
                    // Debug: Show why space press was rejected
                    else if (!isGrounded) {
                        std::cout << "Space press REJECTED - velocityY: " << velocityY << ", spacePressedThisFall: " << spacePressedThisFall << ", bounceLevel: " << bounceLevel << std::endl;
                    }
                }
            }
            else if (event.type == SDL_KEYUP) {
                if (event.key.keysym.sym == SDLK_SPACE) {
                    spacePressed = false;
                }
            }
            // Mouse events for editor
            else if (editorMode && event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    // Convert mouse to world coordinates and snap to grid
                    int worldX = event.button.x + (int)cameraX;
                    int worldY = event.button.y + (int)cameraY;
                    int snappedX = (worldX / GRID_SIZE) * GRID_SIZE;
                    int snappedY = (worldY / GRID_SIZE) * GRID_SIZE;

                    if (currentTool == TOOL_PLATFORM_SOLID || currentTool == TOOL_PLATFORM_FALLTHROUGH) {
                        // Add new platform (6 grid cells wide, 1 cell tall = 192x32)
                        Platform newPlatform;
                        newPlatform.rect = {snappedX, snappedY, GRID_SIZE * 6, GRID_SIZE};
                        newPlatform.isSolid = (currentTool == TOOL_PLATFORM_SOLID);
                        platforms.push_back(newPlatform);
                        std::cout << "Added " << (newPlatform.isSolid ? "solid" : "fall-through")
                                  << " platform at (" << snappedX << ", " << snappedY << ")" << std::endl;
                    }
                    else if (currentTool == TOOL_START) {
                        startPoint = {snappedX, snappedY};
                        hasStartPoint = true;
                        std::cout << "Set start point at (" << snappedX << ", " << snappedY << ")" << std::endl;
                    }
                    else if (currentTool == TOOL_FINISH) {
                        finishPoint = {snappedX, snappedY};
                        hasFinishPoint = true;
                        std::cout << "Set finish point at (" << snappedX << ", " << snappedY << ")" << std::endl;
                    }
                    else if (currentTool == TOOL_DELETE) {
                        // Delete platform at click position
                        for (size_t i = 0; i < platforms.size(); i++) {
                            SDL_Rect& p = platforms[i].rect;
                            if (worldX >= p.x && worldX <= p.x + p.w &&
                                worldY >= p.y && worldY <= p.y + p.h) {
                                platforms.erase(platforms.begin() + i);
                                std::cout << "Deleted platform" << std::endl;
                                break;
                            }
                        }
                    }
                }
            }
        }

        // Get keyboard state for continuous movement
        const Uint8* keystate = SDL_GetKeyboardState(nullptr);
        if (keystate[SDL_SCANCODE_LEFT] || keystate[SDL_SCANCODE_A]) {
            playerX -= MOVE_SPEED;
            if (playerX < 0) playerX = 0;
            faceOffset = -5; // Look left
        }
        else if (keystate[SDL_SCANCODE_RIGHT] || keystate[SDL_SCANCODE_D]) {
            playerX += MOVE_SPEED;
            if (playerX > WORLD_WIDTH - PLAYER_SIZE) {
                playerX = WORLD_WIDTH - PLAYER_SIZE;
            }
            faceOffset = 5; // Look right
        }
        else {
            faceOffset = 0; // Look center
        }

        // Check space state continuously for ground proximity detection
        if (!isGrounded && spacePressed && !spacePressedThisFall && velocityY > 0) {
            // Progressively smaller timing windows (distance from ground) for higher levels
            float distanceThreshold;
            if (bounceLevel == 0) distanceThreshold = 80.0f;
            else if (bounceLevel == 1) distanceThreshold = 70.0f;
            else if (bounceLevel == 2) distanceThreshold = 60.0f;
            else if (bounceLevel == 3) distanceThreshold = 50.0f;
            else if (bounceLevel == 4) distanceThreshold = 40.0f;
            else distanceThreshold = 30.0f; // Level 5 - hardest

            // Calculate distance to ground
            float groundY = WORLD_HEIGHT - GROUND_HEIGHT;
            float distanceToGround = groundY - (playerY + PLAYER_SIZE);

            // Check platforms below as well
            float minDistance = distanceToGround;
            for (const auto& platform : platforms) {
                if ((platform.isSolid || bounceLevel > 0) &&
                    playerX + PLAYER_SIZE > platform.rect.x &&
                    playerX < platform.rect.x + platform.rect.w &&
                    playerY + PLAYER_SIZE < platform.rect.y) {
                    float distToPlatform = platform.rect.y - (playerY + PLAYER_SIZE);
                    if (distToPlatform >= 0 && distToPlatform < minDistance) {
                        minDistance = distToPlatform;
                    }
                }
            }

            if (minDistance <= distanceThreshold && minDistance > 0) {
                spacePressedThisFall = true;
                velocityY += DOWNWARD_FORCE;
                std::cout << "Space detected near ground! distance: " << minDistance << ", bounceLevel: " << bounceLevel << std::endl;
            }
        }

        // Apply gravity only if not grounded
        if (!isGrounded) {
            velocityY += GRAVITY;
        }
        playerY += velocityY;

        bool bouncedThisFrame = false;

        // Update camera with side-scroller style scrolling (game mode only)
        if (!editorMode) {
            // Keep the player inside a comfortable box, like classic side-scrollers.
            float leftBound = cameraX + SCREEN_WIDTH * 0.35f;
            float rightBound = cameraX + SCREEN_WIDTH * 0.65f;
            float topBound = cameraY + SCREEN_HEIGHT * 0.25f;
            float bottomBound = cameraY + SCREEN_HEIGHT * 0.75f;

            if (playerX < leftBound) {
                cameraX = playerX - SCREEN_WIDTH * 0.35f;
            } else if (playerX + PLAYER_SIZE > rightBound) {
                cameraX = playerX + PLAYER_SIZE - SCREEN_WIDTH * 0.65f;
            }

            if (playerY < topBound) {
                cameraY = playerY - SCREEN_HEIGHT * 0.25f;
            } else if (playerY + PLAYER_SIZE > bottomBound) {
                cameraY = playerY + PLAYER_SIZE - SCREEN_HEIGHT * 0.75f;
            }

            // Clamp camera to world bounds
            if (cameraX < 0) cameraX = 0;
            if (cameraX > WORLD_WIDTH - SCREEN_WIDTH) cameraX = WORLD_WIDTH - SCREEN_WIDTH;
            if (cameraY < 0) cameraY = 0;
            if (cameraY > WORLD_HEIGHT - SCREEN_HEIGHT) cameraY = WORLD_HEIGHT - SCREEN_HEIGHT;
        }

        // Platform collision
        bool landedOnPlatform = false;
        if (!editorMode && velocityY >= 0) { // Only check when falling and not in editor mode
            for (size_t i = 0; i < platforms.size(); i++) {
                Platform& platform = platforms[i];
                SDL_Rect& pRect = platform.rect;

                // Check platform type: solid always collides, fall-through only when bouncing
                bool shouldCollide = platform.isSolid || (bounceLevel > 0);
                // Check if player is above platform and would intersect
                if (shouldCollide &&
                    playerX + PLAYER_SIZE > pRect.x &&
                    playerX < pRect.x + pRect.w &&
                    playerY + PLAYER_SIZE <= pRect.y &&
                    playerY + PLAYER_SIZE + velocityY >= pRect.y) {
                    playerY = pRect.y - PLAYER_SIZE;
                    landedOnPlatform = true;

                    // Landing logic
                    if (wasInAir) {
                        std::cout << "Landing on platform " << i << " - bounceLevel before: " << bounceLevel << ", spacePressedThisFall: " << spacePressedThisFall << std::endl;

                        if (spacePressedThisFall) {
                            int oldLevel = bounceLevel;
                            bounceLevel++;
                            if (bounceLevel > 5) bounceLevel = 5;

                            std::cout << "Space was pressed! Level " << oldLevel << " -> " << bounceLevel << std::endl;

                            if (bounceLevel == 1) {
                                velocityY = BOUNCE_LEVEL1;
                                std::cout << "Bouncing with BOUNCE_LEVEL1: " << BOUNCE_LEVEL1 << std::endl;
                                bouncedThisFrame = true;
                            } else if (bounceLevel == 2) {
                                velocityY = BOUNCE_LEVEL2;
                                std::cout << "Bouncing with BOUNCE_LEVEL2: " << BOUNCE_LEVEL2 << std::endl;
                                bouncedThisFrame = true;
                            } else if (bounceLevel == 3) {
                                velocityY = BOUNCE_LEVEL3;
                                std::cout << "Bouncing with BOUNCE_LEVEL3: " << BOUNCE_LEVEL3 << std::endl;
                                bouncedThisFrame = true;
                            } else if (bounceLevel == 4) {
                                velocityY = BOUNCE_LEVEL4;
                                std::cout << "Bouncing with BOUNCE_LEVEL4: " << BOUNCE_LEVEL4 << std::endl;
                                bouncedThisFrame = true;
                            } else if (bounceLevel == 5) {
                                velocityY = BOUNCE_LEVEL5;
                                std::cout << "Bouncing with BOUNCE_LEVEL5: " << BOUNCE_LEVEL5 << std::endl;
                                bouncedThisFrame = true;
                            }
                        } else {
                            // No space pressed - reduce bounce level by 1 instead of resetting
                            int oldLevel = bounceLevel;
                            bounceLevel = std::max(0, bounceLevel - 1);
                            std::cout << "No space pressed (platform) - level " << oldLevel << " -> " << bounceLevel << std::endl;

                            if (bounceLevel == 1) {
                                velocityY = BOUNCE_LEVEL1;
                                hadSmallRebound = false;
                                bouncedThisFrame = true;
                            } else if (bounceLevel == 2) {
                                velocityY = BOUNCE_LEVEL2;
                                hadSmallRebound = false;
                                bouncedThisFrame = true;
                            } else if (bounceLevel == 3) {
                                velocityY = BOUNCE_LEVEL3;
                                hadSmallRebound = false;
                                bouncedThisFrame = true;
                            } else if (bounceLevel == 4) {
                                velocityY = BOUNCE_LEVEL4;
                                hadSmallRebound = false;
                                bouncedThisFrame = true;
                            } else if (bounceLevel == 5) {
                                velocityY = BOUNCE_LEVEL5;
                                hadSmallRebound = false;
                                bouncedThisFrame = true;
                            } else {
                                // At level 0 on a platform, stop immediately (no rebound)
                                velocityY = 0;
                                hadSmallRebound = false;
                                std::cout << "No space pressed (platform) - stopping completely at level 0" << std::endl;
                            }
                        }

                        spacePressedThisFall = false;
                    } else {
                        velocityY = 0;
                    }

                    if (bouncedThisFrame) {
                        isGrounded = false;
                        wasInAir = true;
                    } else {
                        isGrounded = true;
                        wasInAir = false;
                    }
                    break;
                }
            }
        }

        // Ground collision (at bottom of world)
        float groundY = WORLD_HEIGHT - GROUND_HEIGHT - PLAYER_SIZE;
        if (!landedOnPlatform && playerY >= groundY) {
            playerY = groundY;

            // When landing, determine bounce based on whether space was pressed
            if (wasInAir) {
                std::cout << "Landing - bounceLevel before: " << bounceLevel << ", spacePressedThisFall: " << spacePressedThisFall << std::endl;

                if (spacePressedThisFall) {
                    // Increase bounce level (up to max 5)
                    int oldLevel = bounceLevel;
                    bounceLevel++;
                    if (bounceLevel > 5) bounceLevel = 5;
                    hadSmallRebound = false; // Reset rebound flag on success

                    std::cout << "Space was pressed! Level " << oldLevel << " -> " << bounceLevel << std::endl;

                    // Bounce based on current level
                    if (bounceLevel == 1) {
                        velocityY = BOUNCE_LEVEL1;
                        std::cout << "Bouncing with BOUNCE_LEVEL1: " << BOUNCE_LEVEL1 << std::endl;
                        bouncedThisFrame = true;
                    } else if (bounceLevel == 2) {
                        velocityY = BOUNCE_LEVEL2;
                        std::cout << "Bouncing with BOUNCE_LEVEL2: " << BOUNCE_LEVEL2 << std::endl;
                        bouncedThisFrame = true;
                    } else if (bounceLevel == 3) {
                        velocityY = BOUNCE_LEVEL3;
                        std::cout << "Bouncing with BOUNCE_LEVEL3: " << BOUNCE_LEVEL3 << std::endl;
                        bouncedThisFrame = true;
                    } else if (bounceLevel == 4) {
                        velocityY = BOUNCE_LEVEL4;
                        std::cout << "Bouncing with BOUNCE_LEVEL4: " << BOUNCE_LEVEL4 << std::endl;
                        bouncedThisFrame = true;
                    } else if (bounceLevel == 5) {
                        velocityY = BOUNCE_LEVEL5;
                        std::cout << "Bouncing with BOUNCE_LEVEL5: " << BOUNCE_LEVEL5 << std::endl;
                        bouncedThisFrame = true;
                    }
                } else {
                    // No space pressed - reduce bounce level by 1 instead of resetting
                    int oldLevel = bounceLevel;
                    bounceLevel = std::max(0, bounceLevel - 1);
                    std::cout << "No space pressed (ground) - level " << oldLevel << " -> " << bounceLevel << std::endl;

                    if (bounceLevel == 1) {
                        velocityY = BOUNCE_LEVEL1;
                        hadSmallRebound = false;
                        bouncedThisFrame = true;
                    } else if (bounceLevel == 2) {
                        velocityY = BOUNCE_LEVEL2;
                        hadSmallRebound = false;
                        bouncedThisFrame = true;
                    } else if (bounceLevel == 3) {
                        velocityY = BOUNCE_LEVEL3;
                        hadSmallRebound = false;
                        bouncedThisFrame = true;
                    } else if (bounceLevel == 4) {
                        velocityY = BOUNCE_LEVEL4;
                        hadSmallRebound = false;
                        bouncedThisFrame = true;
                    } else if (bounceLevel == 5) {
                        velocityY = BOUNCE_LEVEL5;
                        hadSmallRebound = false;
                        bouncedThisFrame = true;
                    } else {
                        // At level 0, keep the small rebound then stop behavior
                        if (!hadSmallRebound) {
                            velocityY = BOUNCE_LEVEL0;
                            hadSmallRebound = true;
                            bouncedThisFrame = true;
                            std::cout << "No space pressed (ground) - small rebound at level 0" << std::endl;
                        } else {
                            velocityY = 0;
                            hadSmallRebound = false;
                            std::cout << "No space pressed (ground) - stopping completely" << std::endl;
                        }
                    }
                }

                spacePressedThisFall = false;
            } else {
                velocityY = 0;
            }

            if (bouncedThisFrame) {
                isGrounded = false;
                wasInAir = true;
            } else {
                isGrounded = true;
                wasInAir = false;
            }
        } else if (!landedOnPlatform) {
            wasInAir = true;
            isGrounded = false;
        }

        // Clear screen with black color
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Draw ground (only when the bottom of the world is in view)
        int groundScreenY = WORLD_HEIGHT - GROUND_HEIGHT - (int)cameraY;
        if (groundScreenY < SCREEN_HEIGHT && groundScreenY + GROUND_HEIGHT > 0) {
            if (groundTexture) {
                int startX = ((int)cameraX / GROUND_TEXTURE_WIDTH) * GROUND_TEXTURE_WIDTH;
                for (int x = startX; x < cameraX + SCREEN_WIDTH + GROUND_TEXTURE_WIDTH; x += GROUND_TEXTURE_WIDTH) {
                    SDL_Rect dst = {
                        x - (int)cameraX,
                        groundScreenY,
                        GROUND_TEXTURE_WIDTH,
                        GROUND_HEIGHT
                    };
                    SDL_RenderCopy(renderer, groundTexture, nullptr, &dst);
                }
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_Rect ground = {
                    0 - (int)cameraX,
                    groundScreenY,
                    WORLD_WIDTH,
                    GROUND_HEIGHT
                };
                SDL_RenderFillRect(renderer, &ground);
            }
        }

        // Draw platforms
        for (size_t i = 0; i < platforms.size(); i++) {
            SDL_Rect screenRect = {
                platforms[i].rect.x - (int)cameraX,
                platforms[i].rect.y - (int)cameraY,
                platforms[i].rect.w,
                platforms[i].rect.h
            };

            // Different colors for solid vs fall-through
            if (platforms[i].isSolid) {
                SDL_SetRenderDrawColor(renderer, editorMode ? 150 : 200, 200, 200, 255); // Gray
            } else {
                SDL_SetRenderDrawColor(renderer, 100, 150, 255, 255); // Blue for fall-through
            }
            SDL_RenderFillRect(renderer, &screenRect);
        }

        // Draw editor UI
        if (editorMode) {
            // Draw start point if set
            if (hasStartPoint) {
                SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); // Green
                SDL_Rect startRect = {startPoint.x - (int)cameraX, startPoint.y - (int)cameraY, PLAYER_SIZE, PLAYER_SIZE};
                SDL_RenderFillRect(renderer, &startRect);
            }

            // Draw finish point if set
            if (hasFinishPoint) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255); // Yellow
                SDL_Rect finishRect = {finishPoint.x - (int)cameraX, finishPoint.y - (int)cameraY, PLAYER_SIZE, PLAYER_SIZE};
                SDL_RenderFillRect(renderer, &finishRect);
            }

            // Draw grid
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            int startX = ((int)cameraX / GRID_SIZE) * GRID_SIZE;
            int startY = ((int)cameraY / GRID_SIZE) * GRID_SIZE;
            for (int x = startX; x < cameraX + SCREEN_WIDTH; x += GRID_SIZE) {
                int screenX = x - (int)cameraX;
                SDL_RenderDrawLine(renderer, screenX, 0, screenX, SCREEN_HEIGHT);
            }
            for (int y = startY; y < cameraY + SCREEN_HEIGHT; y += GRID_SIZE) {
                int screenY = y - (int)cameraY;
                SDL_RenderDrawLine(renderer, 0, screenY, SCREEN_WIDTH, screenY);
            }

            // Draw UI panel
            SDL_SetRenderDrawColor(renderer, 40, 40, 40, 220);
            SDL_Rect uiPanel = {10, 10, 280, 180};
            SDL_RenderFillRect(renderer, &uiPanel);

            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
            SDL_Rect uiBorder = {10, 10, 280, 180};
            SDL_RenderDrawRect(renderer, &uiBorder);

            // Tool indicators with labels
            const char* toolNames[] = {"1: Solid Platform", "2: Fall-Through", "3: Start Point", "4: Finish Point", "5: Delete"};
            int toolColors[][3] = {{200,200,200}, {100,150,255}, {0,255,0}, {255,255,0}, {255,100,100}};

            for (int i = 0; i < 5; i++) {
                bool selected = (i == (int)currentTool);

                // Draw colored square indicator
                SDL_SetRenderDrawColor(renderer,
                    selected ? 255 : toolColors[i][0],
                    selected ? 255 : toolColors[i][1],
                    selected ? 255 : toolColors[i][2], 255);
                SDL_Rect toolRect = {20, 25 + i * 30, 20, 20};
                SDL_RenderFillRect(renderer, &toolRect);

                // Render text label
                SDL_Color textColor = selected ? SDL_Color{255, 255, 100, 255} : SDL_Color{200, 200, 200, 255};
                SDL_Surface* textSurface = TTF_RenderText_Blended(font, toolNames[i], textColor);
                if (textSurface) {
                    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                    if (textTexture) {
                        SDL_Rect textRect = {50, 25 + i * 30, textSurface->w, textSurface->h};
                        SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);
                        SDL_DestroyTexture(textTexture);
                    }
                    SDL_FreeSurface(textSurface);
                }
            }
        }

        // Draw character's head (64x64 box) - adjust for camera
        if (!editorMode) {
            // Calculate color based on proximity to ground (visual feedback for timing window)
            int r = 232, g = 151, b = 65; // Default tan color

            if (!isGrounded && !spacePressedThisFall && velocityY > 0) {
                // Calculate the current timing window threshold (distance from ground)
                float distanceThreshold;
                if (bounceLevel == 0) distanceThreshold = 80.0f;
                else if (bounceLevel == 1) distanceThreshold = 70.0f;
                else if (bounceLevel == 2) distanceThreshold = 60.0f;
                else if (bounceLevel == 3) distanceThreshold = 50.0f;
                else if (bounceLevel == 4) distanceThreshold = 40.0f;
                else distanceThreshold = 30.0f; // Level 5 - hardest

                // Calculate distance to ground
                float groundY = WORLD_HEIGHT - GROUND_HEIGHT;
                float distanceToGround = groundY - (playerY + PLAYER_SIZE);

                // Check platforms below as well
                float minDistance = distanceToGround;
                for (const auto& platform : platforms) {
                    if ((platform.isSolid || bounceLevel > 0) &&
                        playerX + PLAYER_SIZE > platform.rect.x &&
                        playerX < platform.rect.x + platform.rect.w &&
                        playerY + PLAYER_SIZE < platform.rect.y) {
                        float distToPlatform = platform.rect.y - (playerY + PLAYER_SIZE);
                        if (distToPlatform >= 0 && distToPlatform < minDistance) {
                            minDistance = distToPlatform;
                        }
                    }
                }

                // Check if we're in the timing window
                if (minDistance <= distanceThreshold && minDistance > 0) {
                    // Calculate how close we are to impact (0 = at impact, 1 = edge of window)
                    // Closer to ground = better timing
                    float proximity = minDistance / distanceThreshold;

                    // Interpolate from orange (perfect) to tan (edge of window)
                    // At perfect timing: orange (255, 100, 65)
                    // At edge: tan color (232, 151, 65)
                    r = 255;
                    g = (int)(100 + proximity * 51);  // 100 -> 151
                    b = 65; // Stay constant
                }
            }

            SDL_SetRenderDrawColor(renderer, r, g, b, 255);
            SDL_Rect head = {(int)playerX - (int)cameraX, (int)playerY - (int)cameraY, PLAYER_SIZE, PLAYER_SIZE};
            SDL_RenderFillRect(renderer, &head);

            // Draw eyes (two small squares) - shift based on movement
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_Rect leftEye = {(int)playerX + 12 + faceOffset - (int)cameraX, (int)playerY + 15 - (int)cameraY, 10, 10};
            SDL_Rect rightEye = {(int)playerX + 42 + faceOffset - (int)cameraX, (int)playerY + 15 - (int)cameraY, 10, 10};
            SDL_RenderFillRect(renderer, &leftEye);
            SDL_RenderFillRect(renderer, &rightEye);

            // Draw mouth based on bounce level - shift based on movement
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            if (bounceLevel == 0) {
                // Rectangle mouth (ground level)
                SDL_Rect mouth = {(int)playerX + 17 + faceOffset - (int)cameraX, (int)playerY + 45 - (int)cameraY, 30, 8};
                SDL_RenderFillRect(renderer, &mouth);
            }
            else if (bounceLevel == 1) {
                // Square mouth (first height level) - smaller, just slightly taller than rectangle
                SDL_Rect mouth = {(int)playerX + 22 + faceOffset - (int)cameraX, (int)playerY + 42 - (int)cameraY, 20, 12};
                SDL_RenderFillRect(renderer, &mouth);
            }
            else if (bounceLevel == 2) {
                // Triangle mouth (second height level) - shift based on movement
                SDL_Point trianglePoints[4] = {
                    {(int)playerX + 32 + faceOffset - (int)cameraX, (int)playerY + 45 - (int)cameraY},  // top center
                    {(int)playerX + 20 + faceOffset - (int)cameraX, (int)playerY + 55 - (int)cameraY},  // bottom left
                    {(int)playerX + 44 + faceOffset - (int)cameraX, (int)playerY + 55 - (int)cameraY},  // bottom right
                    {(int)playerX + 32 + faceOffset - (int)cameraX, (int)playerY + 45 - (int)cameraY}   // close triangle
                };
                SDL_RenderDrawLines(renderer, trianglePoints, 4);
                // Fill the triangle with multiple lines
                for (int i = 0; i < 10; i++) {
                    int y = (int)playerY + 45 + i - (int)cameraY;
                    int leftX = (int)playerX + 32 + faceOffset - (i * 1.2f) - (int)cameraX;
                    int rightX = (int)playerX + 32 + faceOffset + (i * 1.2f) - (int)cameraX;
                    SDL_RenderDrawLine(renderer, leftX, y, rightX, y);
                }
            }
            else if (bounceLevel == 3) {
                // Circle mouth - shift based on movement
                int centerX = (int)playerX + 32 + faceOffset - (int)cameraX;
                int centerY = (int)playerY + 48 - (int)cameraY;
                int radius = 12;
                // Draw filled circle
                for (int y = -radius; y <= radius; y++) {
                    for (int x = -radius; x <= radius; x++) {
                        if (x*x + y*y <= radius*radius) {
                            SDL_RenderDrawPoint(renderer, centerX + x, centerY + y);
                        }
                    }
                }
            }
            else if (bounceLevel == 4) {
                // Larger circle mouth - shift based on movement
                int centerX = (int)playerX + 32 + faceOffset - (int)cameraX;
                int centerY = (int)playerY + 48 - (int)cameraY;
                int radius = 15;
                // Draw filled circle
                for (int y = -radius; y <= radius; y++) {
                    for (int x = -radius; x <= radius; x++) {
                        if (x*x + y*y <= radius*radius) {
                            SDL_RenderDrawPoint(renderer, centerX + x, centerY + y);
                        }
                    }
                }
            }
            else if (bounceLevel == 5) {
                // Inverted triangle mouth (pointing up) - shift based on movement
                SDL_Point trianglePoints[4] = {
                    {(int)playerX + 32 + faceOffset - (int)cameraX, (int)playerY + 42 - (int)cameraY},  // bottom center
                    {(int)playerX + 18 + faceOffset - (int)cameraX, (int)playerY + 55 - (int)cameraY},  // top left
                    {(int)playerX + 46 + faceOffset - (int)cameraX, (int)playerY + 55 - (int)cameraY},  // top right
                    {(int)playerX + 32 + faceOffset - (int)cameraX, (int)playerY + 42 - (int)cameraY}   // close triangle
                };
                SDL_RenderDrawLines(renderer, trianglePoints, 4);
                // Fill the inverted triangle with multiple lines
                for (int i = 0; i < 13; i++) {
                    int y = (int)playerY + 42 + i - (int)cameraY;
                    int leftX = (int)playerX + 32 + faceOffset - (i * 1.08f) - (int)cameraX;
                    int rightX = (int)playerX + 32 + faceOffset + (i * 1.08f) - (int)cameraX;
                    SDL_RenderDrawLine(renderer, leftX, y, rightX, y);
                }
            }
        }

        // Update screen
        SDL_RenderPresent(renderer);

        // Add a small delay to prevent high CPU usage
        SDL_Delay(16); // ~60 FPS
    }

    // Cleanup
    TTF_CloseFont(font);
    if (groundTexture) {
        SDL_DestroyTexture(groundTexture);
    }
    TTF_Quit();
    IMG_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
