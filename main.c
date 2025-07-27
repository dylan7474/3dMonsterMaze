/*
 * main.c - A 3D Maze game using raycasting with SDL2.
 *
 * This application is designed to be cross-compiled on a Linux system
 * to generate a standalone executable for Windows.
 *
 * Controls:
 * Arrow Keys or W/A/S/D: Move and turn.
 * Spacebar: Shoot
 */

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_mixer.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>


// --- Game Constants ---
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define MAP_WIDTH 16
#define MAP_HEIGHT 16
#define MOVE_SPEED 0.05f
#define TURN_SPEED 0.03f
#define BULLET_SPEED 0.1f
#define MAX_BULLETS 5
#define SAMPLE_RATE 44100
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Structs ---
typedef struct {
    float x, y;
    int alive;
    int health;
    SDL_Texture* texture;
} Sprite;

typedef struct {
    float x, y;
    float dx, dy;
    int active;
    int lifetime;
    int anim_frame;
} Bullet;


// --- Map Layout ---
// 1 = Wall, 0 = Empty Space
int g_map[MAP_HEIGHT][MAP_WIDTH] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,1,1,1,0,1,0,1,1,1,0,1,1,0,1},
    {1,0,0,0,1,0,1,0,0,0,1,0,1,0,0,1},
    {1,0,1,0,1,0,1,1,1,0,1,0,1,0,1,1},
    {1,0,1,0,0,0,0,0,0,0,0,0,1,0,0,1},
    {1,0,1,0,1,1,1,0,1,1,1,0,1,1,0,1},
    {1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,0,1,1,1,1,1,0,1,1,0,1},
    {1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1},
    {1,0,1,1,1,0,1,1,1,0,1,0,1,1,0,1},
    {1,0,0,0,1,0,0,0,1,0,1,0,1,0,0,1},
    {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,1},
    {1,0,1,0,0,0,1,0,0,0,0,0,1,0,0,1},
    {1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

// --- Global Variables ---
SDL_Window* g_window = NULL;
SDL_Renderer* g_renderer = NULL;
Mix_Chunk* g_heartbeat_sound = NULL;
Mix_Chunk* g_shoot_sound = NULL;
Mix_Chunk* g_monster_hit_sound = NULL;
Mix_Chunk* g_monster_death_sound = NULL;
Uint32 g_last_heartbeat_time = 0;

float g_player_x = 1.5f;
float g_player_y = 1.5f;
float g_dir_x = -1.0f, g_dir_y = 0.0f;
float g_plane_x = 0.0f, g_plane_y = 0.66f;

Sprite g_monster;
Bullet g_bullets[MAX_BULLETS];
SDL_Texture* g_bullet_textures[2];

// --- Function Prototypes ---
int initialize();
void create_sprites();
void create_sounds();
void fire_bullet();
void handle_input(int* is_running);
void update_game();
void render_scene();
void cleanup();
void draw_circle(SDL_Renderer* rend, int cx, int cy, int radius);

// --- Main ---
int main(int argc, char* argv[]) {
    if (!initialize()) {
        cleanup();
        return 1;
    }
    int is_running = 1;
    while (is_running) {
        handle_input(&is_running);
        update_game();
        render_scene();
        SDL_Delay(16);
    }
    cleanup();
    return 0;
}

// --- Implementations ---
int initialize() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) return 0;
    if (Mix_OpenAudio(SAMPLE_RATE, MIX_DEFAULT_FORMAT, 2, 2048) < 0) return 0;
    g_window = SDL_CreateWindow("3D Monster Maze", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!g_window) return 0;
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
    if (!g_renderer) return 0;
    srand(time(0));
    create_sprites();
    create_sounds();
    g_monster.x = 14.5f;
    g_monster.y = 13.5f;
    g_monster.alive = 1;
    g_monster.health = 3;
    for(int i=0; i<MAX_BULLETS; i++) g_bullets[i].active = 0;
    return 1;
}

void create_sounds() {
    // Heartbeat
    static Sint16 heartbeat_data[SAMPLE_RATE/8]; int len = sizeof(heartbeat_data);
    for(int i=0; i<len/2; ++i){ double t=(double)i/SAMPLE_RATE; double f=100.0-(t*100.0); heartbeat_data[i]=(Sint16)(9000*sin(2.0*M_PI*f*t)*(1.0-(t*8)));}
    g_heartbeat_sound = Mix_QuickLoad_RAW((Uint8*)heartbeat_data, len);

    // Shoot
    static Sint16 shoot_data[SAMPLE_RATE/20]; len = sizeof(shoot_data);
    for(int i=0; i<len/2; ++i){ double t=(double)i/SAMPLE_RATE; double f=880.0-(t*1500.0); shoot_data[i]=(Sint16)(5000*sin(2.0*M_PI*f*t)*(1.0-(t*20)));}
    g_shoot_sound = Mix_QuickLoad_RAW((Uint8*)shoot_data, len);
    
    // Monster Hit
    static Sint16 hit_data[SAMPLE_RATE/15]; len = sizeof(hit_data);
    for(int i=0; i<len/2; ++i){ double t=(double)i/SAMPLE_RATE; double f=660.0+(t*800.0); hit_data[i]=(Sint16)(7000*sin(2.0*M_PI*f*t)*(1.0-(t*15)));}
    g_monster_hit_sound = Mix_QuickLoad_RAW((Uint8*)hit_data, len);

    // Monster Death
    static Sint16 death_data[SAMPLE_RATE/2]; len = sizeof(death_data);
    for(int i=0; i<len/2; ++i){ double t=(double)i/SAMPLE_RATE; double f=220.0-(t*200.0); death_data[i]=(Sint16)(8000*sin(2.0*M_PI*f*t)*(1.0-(t*2)) + 4000*((rand()%256)/255.0-0.5));}
    g_monster_death_sound = Mix_QuickLoad_RAW((Uint8*)death_data, len);
}

void create_sprites() {
    // Monster Sprite
    g_monster.texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 64, 64);
    SDL_SetTextureBlendMode(g_monster.texture, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(g_renderer, g_monster.texture);
    SDL_SetRenderDrawColor(g_renderer, 0,0,0,0); SDL_RenderClear(g_renderer);
    SDL_SetRenderDrawColor(g_renderer, 255,0,0,255);
    SDL_RenderFillRect(g_renderer, &(SDL_Rect){20, 8, 24, 40});
    SDL_RenderFillRect(g_renderer, &(SDL_Rect){12, 48, 40, 8});
    SDL_SetRenderDrawColor(g_renderer, 255,255,0,255);
    SDL_RenderFillRect(g_renderer, &(SDL_Rect){24, 16, 16, 16});
    
    // Bullet Sprite (2 frames for animation)
    for(int i=0; i<2; i++) {
        g_bullet_textures[i] = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 16, 16);
        SDL_SetTextureBlendMode(g_bullet_textures[i], SDL_BLENDMODE_BLEND);
        SDL_SetRenderTarget(g_renderer, g_bullet_textures[i]);
        SDL_SetRenderDrawColor(g_renderer, 0,0,0,0); SDL_RenderClear(g_renderer);
        SDL_SetRenderDrawColor(g_renderer, 255, 255, 0, 255);
        draw_circle(g_renderer, 8, 8, 4 + i*2);
    }
    
    SDL_SetRenderTarget(g_renderer, NULL);
}

void fire_bullet() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!g_bullets[i].active) {
            g_bullets[i].active = 1;
            g_bullets[i].x = g_player_x;
            g_bullets[i].y = g_player_y;
            g_bullets[i].dx = g_dir_x * BULLET_SPEED;
            g_bullets[i].dy = g_dir_y * BULLET_SPEED;
            g_bullets[i].lifetime = 100;
            g_bullets[i].anim_frame = 0;
            if(g_shoot_sound) Mix_PlayChannel(-1, g_shoot_sound, 0);
            return;
        }
    }
}

void handle_input(int* is_running) {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) *is_running = 0;
        if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_SPACE && e.key.repeat == 0) {
                fire_bullet();
            }
        }
    }
    const Uint8* state = SDL_GetKeyboardState(NULL);
    if (state[SDL_SCANCODE_LEFT] || state[SDL_SCANCODE_A]) {
        float old_dir_x=g_dir_x; g_dir_x=g_dir_x*cosf(TURN_SPEED)-g_dir_y*sinf(TURN_SPEED); g_dir_y=old_dir_x*sinf(TURN_SPEED)+g_dir_y*cosf(TURN_SPEED);
        float old_plane_x=g_plane_x; g_plane_x=g_plane_x*cosf(TURN_SPEED)-g_plane_y*sinf(TURN_SPEED); g_plane_y=old_plane_x*sinf(TURN_SPEED)+g_plane_y*cosf(TURN_SPEED);
    }
    if (state[SDL_SCANCODE_RIGHT] || state[SDL_SCANCODE_D]) {
        float old_dir_x=g_dir_x; g_dir_x=g_dir_x*cosf(-TURN_SPEED)-g_dir_y*sinf(-TURN_SPEED); g_dir_y=old_dir_x*sinf(-TURN_SPEED)+g_dir_y*cosf(-TURN_SPEED);
        float old_plane_x=g_plane_x; g_plane_x=g_plane_x*cosf(-TURN_SPEED)-g_plane_y*sinf(-TURN_SPEED); g_plane_y=old_plane_x*sinf(-TURN_SPEED)+g_plane_y*cosf(-TURN_SPEED);
    }
    if (state[SDL_SCANCODE_UP] || state[SDL_SCANCODE_W]) {
        if(g_map[(int)(g_player_y)][(int)(g_player_x+g_dir_x*MOVE_SPEED)]==0) g_player_x+=g_dir_x*MOVE_SPEED;
        if(g_map[(int)(g_player_y+g_dir_y*MOVE_SPEED)][(int)(g_player_x)]==0) g_player_y+=g_dir_y*MOVE_SPEED;
    }
    if (state[SDL_SCANCODE_DOWN] || state[SDL_SCANCODE_S]) {
        if(g_map[(int)(g_player_y)][(int)(g_player_x-g_dir_x*MOVE_SPEED)]==0) g_player_x-=g_dir_x*MOVE_SPEED;
        if(g_map[(int)(g_player_y-g_dir_y*MOVE_SPEED)][(int)(g_player_x)]==0) g_player_y-=g_dir_y*MOVE_SPEED;
    }
}

void update_game() {
    if(g_monster.alive) {
        float dist = hypotf(g_player_x - g_monster.x, g_player_y - g_monster.y);
        
        if (dist < 8.0f) {
            float angle_to_player = atan2f(g_player_y - g_monster.y, g_player_x - g_monster.x);
            float monster_speed = MOVE_SPEED * 0.5f;
            float next_x = g_monster.x + cosf(angle_to_player) * monster_speed;
            float next_y = g_monster.y + sinf(angle_to_player) * monster_speed;
            if (g_map[(int)g_monster.y][(int)next_x] == 0) g_monster.x = next_x;
            if (g_map[(int)next_y][(int)g_monster.x] == 0) g_monster.y = next_y;
        }
        
        if(g_heartbeat_sound) {
            int delay = (int)(dist * 100);
            if (delay < 50) delay = 50; 
            if (SDL_GetTicks() > g_last_heartbeat_time + delay) {
                Mix_PlayChannel(-1, g_heartbeat_sound, 0);
                g_last_heartbeat_time = SDL_GetTicks();
            }
        }

        if (dist < 0.5f) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Game Over", "You have been caught by the monster!", NULL);
            exit(0);
        }
    }

    // Update bullets
    static int anim_counter = 0;
    if(++anim_counter > 5) { anim_counter = 0; }
    
    for(int i=0; i < MAX_BULLETS; i++) {
        if (g_bullets[i].active) {
            g_bullets[i].x += g_bullets[i].dx;
            g_bullets[i].y += g_bullets[i].dy;
            if(anim_counter == 0) g_bullets[i].anim_frame = 1 - g_bullets[i].anim_frame;

            if (g_map[(int)g_bullets[i].y][(int)g_bullets[i].x] == 1) {
                g_bullets[i].active = 0;
            }
            if (--g_bullets[i].lifetime <= 0) g_bullets[i].active = 0;

            if(g_monster.alive && hypotf(g_bullets[i].x - g_monster.x, g_bullets[i].y - g_monster.y) < 0.5f) {
                g_bullets[i].active = 0;
                g_monster.health--;
                if(g_monster_hit_sound) Mix_PlayChannel(-1, g_monster_hit_sound, 0);
                if(g_monster.health <= 0) {
                    g_monster.alive = 0;
                    if(g_monster_death_sound) Mix_PlayChannel(-1, g_monster_death_sound, 0);
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "You Win!", "You have defeated the monster!", NULL);
                    exit(0);
                }
            }
        }
    }
}

void render_scene() {
    SDL_SetRenderDrawColor(g_renderer, 30,30,30,255);
    SDL_RenderFillRect(g_renderer, &(SDL_Rect){0,0,SCREEN_WIDTH,SCREEN_HEIGHT/2});
    SDL_SetRenderDrawColor(g_renderer, 60,60,60,255);
    SDL_RenderFillRect(g_renderer, &(SDL_Rect){0,SCREEN_HEIGHT/2,SCREEN_WIDTH,SCREEN_HEIGHT/2});

    float z_buffer[SCREEN_WIDTH];

    for(int x = 0; x < SCREEN_WIDTH; x++) {
        float camera_x = 2*x/(float)SCREEN_WIDTH-1;
        float ray_dir_x = g_dir_x+g_plane_x*camera_x;
        float ray_dir_y = g_dir_y+g_plane_y*camera_x;
        int map_x=(int)g_player_x; int map_y=(int)g_player_y;
        float delta_dist_x=(ray_dir_x==0)?1e30:fabsf(1/ray_dir_x);
        float delta_dist_y=(ray_dir_y==0)?1e30:fabsf(1/ray_dir_y);
        float perp_wall_dist;
        int step_x, step_y;
        float side_dist_x, side_dist_y;
        int hit=0, side;

        if(ray_dir_x<0){step_x=-1; side_dist_x=(g_player_x-map_x)*delta_dist_x;} else {step_x=1; side_dist_x=(map_x+1.0-g_player_x)*delta_dist_x;}
        if(ray_dir_y<0){step_y=-1; side_dist_y=(g_player_y-map_y)*delta_dist_y;} else {step_y=1; side_dist_y=(map_y+1.0-g_player_y)*delta_dist_y;}
        
        while(hit==0){
            if(side_dist_x<side_dist_y){side_dist_x+=delta_dist_x;map_x+=step_x;side=0;}
            else{side_dist_y+=delta_dist_y;map_y+=step_y;side=1;}
            if(g_map[map_y][map_x]>0)hit=1;
        }
        
        if(side==0) perp_wall_dist = (side_dist_x-delta_dist_x); else perp_wall_dist=(side_dist_y-delta_dist_y);
        z_buffer[x] = perp_wall_dist;

        int line_height=(int)(SCREEN_HEIGHT/perp_wall_dist);
        int draw_start=-line_height/2+SCREEN_HEIGHT/2; if(draw_start<0)draw_start=0;
        int draw_end=line_height/2+SCREEN_HEIGHT/2; if(draw_end>=SCREEN_HEIGHT)draw_end=SCREEN_HEIGHT-1;
        
        SDL_Color color = (side==1) ? (SDL_Color){100,100,100,255} : (SDL_Color){150,150,150,255};
        SDL_SetRenderDrawColor(g_renderer, color.r, color.g, color.b, 255);
        SDL_RenderDrawLine(g_renderer, x, draw_start, x, draw_end);
    }
    
    // --- Sprite Casting ---
    Sprite* sprites[] = {&g_monster};
    Bullet* bullet_sprites[MAX_BULLETS];
    int sprite_count = g_monster.alive ? 1 : 0;
    int bullet_count = 0;
    for(int i=0; i<MAX_BULLETS; i++) { if(g_bullets[i].active) bullet_sprites[bullet_count++] = &g_bullets[i]; }

    // Combine and sort all visible sprites by distance
    // For now, we will just render the monster then the bullets. A full implementation
    // would combine them into one list and sort by distance.
    
    // Render monster
    if(g_monster.alive) {
        float sprite_x = g_monster.x - g_player_x;
        float sprite_y = g_monster.y - g_player_y;
        float inv_det = 1.0f/(g_plane_x*g_dir_y-g_dir_x*g_plane_y);
        float transform_x = inv_det*(g_dir_y*sprite_x-g_dir_x*sprite_y);
        float transform_y = inv_det*(-g_plane_y*sprite_x+g_plane_x*sprite_y);
        
        if(transform_y > 0) {
            int sprite_screen_x=(int)((SCREEN_WIDTH/2)*(1+transform_x/transform_y));
            int sprite_height=abs((int)(SCREEN_HEIGHT/transform_y));
            int draw_start_y=-sprite_height/2+SCREEN_HEIGHT/2; if(draw_start_y<0)draw_start_y=0;
            int draw_end_y=sprite_height/2+SCREEN_HEIGHT/2; if(draw_end_y>=SCREEN_HEIGHT)draw_end_y=SCREEN_HEIGHT-1;
            int sprite_width=abs((int)(SCREEN_HEIGHT/transform_y));
            int draw_start_x=-sprite_width/2+sprite_screen_x; if(draw_start_x<0)draw_start_x=0;
            int draw_end_x=sprite_width/2+sprite_screen_x; if(draw_end_x>=SCREEN_WIDTH)draw_end_x=SCREEN_WIDTH-1;
            
            for(int stripe=draw_start_x; stripe<draw_end_x; stripe++){
                if(transform_y>0 && stripe>=0 && stripe<SCREEN_WIDTH && transform_y<z_buffer[stripe]){
                    int tex_x=(int)(256*(stripe-(-sprite_width/2+sprite_screen_x))*64/sprite_width)/256;
                    SDL_Rect src={tex_x,0,1,64}; SDL_Rect dest={stripe,draw_start_y,1,sprite_height};
                    SDL_RenderCopy(g_renderer,g_monster.texture,&src,&dest);
                }
            }
        }
    }

    // Render bullets
    for(int i=0; i<bullet_count; ++i) {
        float sprite_x = bullet_sprites[i]->x - g_player_x;
        float sprite_y = bullet_sprites[i]->y - g_player_y;
        float inv_det = 1.0f/(g_plane_x*g_dir_y-g_dir_x*g_plane_y);
        float transform_x = inv_det*(g_dir_y*sprite_x-g_dir_x*sprite_y);
        float transform_y = inv_det*(-g_plane_y*sprite_x+g_plane_x*sprite_y);

        if(transform_y > 0) {
            int sprite_screen_x=(int)((SCREEN_WIDTH/2)*(1+transform_x/transform_y));
            int sprite_size = abs((int)(SCREEN_HEIGHT/transform_y)) / 4;
            int draw_start_y=-sprite_size/2+SCREEN_HEIGHT/2;
            int draw_end_y=sprite_size/2+SCREEN_HEIGHT/2;
            int draw_start_x=-sprite_size/2+sprite_screen_x;
            int draw_end_x=sprite_size/2+sprite_screen_x;
            
            for(int stripe=draw_start_x; stripe<draw_end_x; ++stripe) {
                if(transform_y > 0 && stripe >= 0 && stripe < SCREEN_WIDTH && transform_y < z_buffer[stripe]) {
                     SDL_Rect dest = {draw_start_x, draw_start_y, sprite_size, sprite_size};
                     SDL_RenderCopy(g_renderer, g_bullet_textures[bullet_sprites[i]->anim_frame], NULL, &dest);
                     break; 
                }
            }
        }
    }
    
    SDL_RenderPresent(g_renderer);
}

void cleanup() {
    if(g_heartbeat_sound) Mix_FreeChunk(g_heartbeat_sound);
    if(g_shoot_sound) Mix_FreeChunk(g_shoot_sound);
    if(g_monster_death_sound) Mix_FreeChunk(g_monster_death_sound);
    Mix_CloseAudio();
    if(g_monster.texture) SDL_DestroyTexture(g_monster.texture);
    if(g_bullet_textures[0]) SDL_DestroyTexture(g_bullet_textures[0]);
    if(g_bullet_textures[1]) SDL_DestroyTexture(g_bullet_textures[1]);
    if(g_renderer) SDL_DestroyRenderer(g_renderer);
    if(g_window) SDL_DestroyWindow(g_window);
    SDL_Quit();
}

void draw_circle(SDL_Renderer* rend, int cx, int cy, int radius) {
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x*x + y*y <= radius*radius) {
                SDL_RenderDrawPoint(rend, cx + x, cy + y);
            }
        }
    }
}
