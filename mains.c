#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "math.h"
#include "ctype.h"

#include "SDL2/SDL.h"
#include "SDL2/SDL_image.h"
#include "SDL2/SDL_mixer.h"

#define SCREEN_WIDTH   1280
#define SCREEN_HEIGHT  720
#define PLAYER_SPEED          4
#define PLAYER_BULLET_SPEED   20

#define MAX_KEYBOARD_KEYS 350
#define MAX_SND_CHANNELS 6
#define MAX_LINE_LENGTH 1024
#define GLYPH_HEIGHT 28
#define GLYPH_WIDTH  18

#define SIDE_PLAYER 0
#define SIDE_ALIEN  1
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

enum
{
	CH_ANY = -1,
	CH_PLAYER,
};

enum
{
	SND_PLAYER_FIRE,
	SND_PLAYER_DIE,
	SND_ALIEN_DIE,
	SND_MAX
};

typedef struct Entity Entity;

typedef struct {
	void (*logic)(void);
	void (*draw)(void);
} Delegate;

typedef struct {
	SDL_Renderer *renderer;
	SDL_Window *window;
	Delegate delegate;
	int keyboard[MAX_KEYBOARD_KEYS];
} App;

struct Entity {
	float x;
	float y;
	int w;
	int h;
	float dx;
	float dy;
	int health;
	int reload;
	int side;
	SDL_Texture *texture;
	Entity *next;
};

typedef struct {
	Entity fighterHead, *fighterTail;
	Entity bulletHead, *bulletTail;
	int score;
} Stage;


SDL_Texture *loadTexture(char *filename);
static Entity *player;

App app;
Stage stage;

static SDL_Texture *bulletTexture;
static SDL_Texture *enemyTexture;
static SDL_Texture *background;
static SDL_Texture *fontTexture;

int collision();
static int enemySpawnTimer;
static int backgroundX;
//static int highscore;
static int bulletHitFighter();
static int hitPlayer();

void cleanup(void);
void initSDL(void);
void initStage(void);
void prepareScene(void);
void presentScene(void);
void doInput(void);
void blit();
void blitRect();
void initSounds(void);
void loadMusic();
void playMusic(int loop);
void playSound();
void initFonts(void);
//void drawText();


static char drawTextBuffer[MAX_LINE_LENGTH];

static void logic(void);
static void draw(void);
static void initPlayer(void);
static void fireBullet(void);
static void doPlayer(void);
static void doFighters(void);
static void doBullets(void);
static void drawFighters(void);
static void drawBullets(void);
static void spawnEnemies(void);
static void doBackground(void);
static void drawBackground(void);
static void loadSounds(void);
static void drawHud(void);

static Mix_Chunk *sounds[SND_MAX];
static Mix_Music *music;

static void capFrameRate(long *then, float *remainder);

int main(int argc, char *argv[])
{
	long then;
	float remainder;
	
	memset(&app, 0, sizeof(App));
	
	initSDL();
	
	atexit(cleanup);
	
	initSounds();
	
	initStage();
	
	initFonts();
	
	then = SDL_GetTicks();
	
	remainder = 0;
	
	initSounds();

	
	while (1)
	{
		prepareScene();
		
		doInput();
		
		app.delegate.logic();
		
		app.delegate.draw();
		
		presentScene();
		
		capFrameRate(&then, &remainder);
	}

	return 0;
}

static void capFrameRate(long *then, float *remainder)
{
	long wait, frameTime;
	
	wait = 16 + *remainder;
	
	*remainder -= (int)*remainder;
	
	frameTime = SDL_GetTicks() - *then;
	
	wait -= frameTime;
	
	if (wait < 1)
	{
		wait = 1;
	}
		
	SDL_Delay(wait);
	
	*remainder += 0.667;
	
	*then = SDL_GetTicks();
}

void prepareScene(void)
{
	SDL_SetRenderDrawColor(app.renderer, 32, 32, 32, 255);
	SDL_RenderClear(app.renderer);
}

void presentScene(void)
{
	SDL_RenderPresent(app.renderer);
}

SDL_Texture *loadTexture(char *filename)
{
	SDL_Texture *texture;

	SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, "Loading %s", filename);

	texture = IMG_LoadTexture(app.renderer, filename);

	return texture;
}

void blit(SDL_Texture *texture, int x, int y)
{
	SDL_Rect dest;
	
	dest.x = x;
	dest.y = y;
	SDL_QueryTexture(texture, NULL, NULL, &dest.w, &dest.h);
	
	SDL_RenderCopy(app.renderer, texture, NULL, &dest);
}

void blitRect(SDL_Texture *texture, SDL_Rect *src, int x, int y)
{
	SDL_Rect dest;
	
	dest.x = x;
	dest.y = y;
	dest.w = src->w;
	dest.h = src->h;
	
	SDL_RenderCopy(app.renderer, texture, src, &dest);
}

void initSDL(void)
{
	int rendererFlags, windowFlags;

	rendererFlags = SDL_RENDERER_ACCELERATED;
	
	windowFlags = 0;

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		printf("Couldn't initialize SDL: %s\n", SDL_GetError());
		exit(1);
	}
	
	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) == -1)
	{
		printf("Couldn't initialize SDL Mixer\n");
		exit(1);
	}
	
	Mix_AllocateChannels(MAX_SND_CHANNELS);

	app.window = SDL_CreateWindow("Shooter 06", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, windowFlags);

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

	app.renderer = SDL_CreateRenderer(app.window, -1, rendererFlags);
	
	IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
	
}

void cleanup(void)
{
	SDL_DestroyRenderer(app.renderer);
	
	SDL_DestroyWindow(app.window);
	
	SDL_Quit();
}

void doKeyUp(SDL_KeyboardEvent *event)
{
	if (event->repeat == 0 && event->keysym.scancode < MAX_KEYBOARD_KEYS)
	{
		app.keyboard[event->keysym.scancode] = 0;
	}
}

void doKeyDown(SDL_KeyboardEvent *event)
{
	if (event->repeat == 0 && event->keysym.scancode < MAX_KEYBOARD_KEYS)
	{
		app.keyboard[event->keysym.scancode] = 1;
	}
}

void doInput(void)
{
	SDL_Event event;
	
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_QUIT:
				exit(0);
				break;
				
			case SDL_KEYDOWN:
				doKeyDown(&event.key);
				break;
				
			case SDL_KEYUP:
				doKeyUp(&event.key);
				break;

			default:
				break;
		}
	}
}

void initStage(void)
{
	app.delegate.logic = logic;
	app.delegate.draw = draw;
	
	memset(&stage, 0, sizeof(Stage));
	stage.fighterTail = &stage.fighterHead;
	stage.bulletTail = &stage.bulletHead;
	
	initPlayer();
	
	background = loadTexture("gfx/ww.jpg");
	bulletTexture = loadTexture("gfx/doritos.png");
	enemyTexture = loadTexture("gfx/enemy.png");
	
	enemySpawnTimer = 0;
	
	loadMusic("music/Surreal-Chase_Looping.mp3");
	
	playMusic(1);
}

static void initPlayer() 
{
	player = malloc(sizeof(Entity));
	memset(player, 0, sizeof(Entity));
	stage.fighterTail->next = player;
	stage.fighterTail = player;
	
	player->x = 100;
	player->y = 100;
	player->texture = loadTexture("gfx/7rbZ.png");
	SDL_QueryTexture(player->texture, NULL, NULL, &player->w, &player->h);
	player->side = SIDE_PLAYER;
}

static void logic(void)
{
	doBackground();
	
	doPlayer();
	
	doFighters();
	
	doBullets();
	
	spawnEnemies();
}

static void doPlayer(void)
{
	player->dx = player->dy = 0;
	
	if (player->reload > 0)
	{
		player->reload--;
	}
	
	if (app.keyboard[SDL_SCANCODE_UP])
	{
		player->dy = -PLAYER_SPEED;
	}
	
	if (app.keyboard[SDL_SCANCODE_DOWN])
	{
		player->dy = PLAYER_SPEED;
	}
	
	if (app.keyboard[SDL_SCANCODE_LEFT])
	{
		player->dx = -PLAYER_SPEED;
	}
	
	if (app.keyboard[SDL_SCANCODE_RIGHT])
	{
		player->dx = PLAYER_SPEED;
	}
	
	if (app.keyboard[SDL_SCANCODE_SPACE] && player->reload == 0)
	{
		playSound(SND_PLAYER_FIRE, CH_PLAYER);
		fireBullet();
	}
	
	player->x += player->dx;
	
	if(player->x <= 0){player->x = 0;}
	else if(player->x >= SCREEN_WIDTH-40) {player->x = SCREEN_WIDTH-40;}
	
 	player->y += player->dy;
	
	if (player->y <= 0){player->y = 0;}
	else if (player->y >= SCREEN_HEIGHT-40) {player->y = SCREEN_HEIGHT-40;}
}

static void doFighters(void)
{
	Entity *e, *prev;
	prev = &stage.fighterHead;
	
	for (e = stage.fighterHead.next ; e != NULL ; e = e->next)
	{
		e->x += e->dx;
		e->y += e->dy;
		
		if(e != player && hitPlayer(e)){
		stage.score++;
		//highscore = MAX(stage.score, highscore);
		}
		
		if (e != player && (e->x < -e->w || e->health == 0 || hitPlayer(e)))
		{
			if (e == stage.fighterTail)
			{
				stage.fighterTail = prev;
				
			}
			playSound(SND_ALIEN_DIE, CH_ANY);
			
			prev->next = e->next;
			free(e);
			e = prev;
		}
		
		prev = e;
	}
}
	
static void doBullets(void)
{
	Entity *b, *prev;
	
	prev = &stage.bulletHead;
	
	for (b = stage.bulletHead.next ; b != NULL ; b = b->next)
	{
		b->x += b->dx;
		b->y += b->dy;
		
		if (bulletHitFighter(b) || b->x > SCREEN_WIDTH )
		{
			if (b == stage.bulletTail)
			{
				stage.bulletTail = prev;
			}
	
			prev->next = b->next;
			free(b);
			b = prev;
		}
		
		prev = b;
	}
}

static void fireBullet(void)
{
	Entity *bullet;
	
	bullet = malloc(sizeof(Entity));
	memset(bullet, 0, sizeof(Entity));
	stage.bulletTail->next = bullet;
	stage.bulletTail = bullet;
	
	bullet->x = player->x;
	bullet->y = player->y;
	bullet->dx = PLAYER_BULLET_SPEED;
	bullet->health = 1;
	bullet->texture = bulletTexture;
	SDL_QueryTexture(bullet->texture, NULL, NULL, &bullet->w, &bullet->h);
	
	bullet->y += (player->h / 2) - (bullet->h / 2);
	
	bullet->side = SIDE_PLAYER;
	
	player->reload = 8;
	
}

static void spawnEnemies(void)
{
	Entity *enemy;
	
	if (--enemySpawnTimer <= 0)
	{
		enemy = malloc(sizeof(Entity));
		memset(enemy, 0, sizeof(Entity));
		stage.fighterTail->next = enemy;
		stage.fighterTail = enemy;
		
		enemy->x = SCREEN_WIDTH;
		enemy->y = rand() % SCREEN_HEIGHT;
		enemy->texture = enemyTexture;
		SDL_QueryTexture(enemy->texture, NULL, NULL, &enemy->w, &enemy->h);
		
		enemy->dx = -(4 + (rand() % 4));
		
		enemy->side = SIDE_ALIEN;
		enemy->health = 1;
		
		enemySpawnTimer = 10 + (rand() % 60);
		
	}
}

static void draw(void)
{
	drawBackground();
	
	drawFighters();
	
	drawBullets();
	
	drawHud();
}

static void drawFighters(void)
{
	Entity *e;
	
	for (e = stage.fighterHead.next ; e != NULL ; e = e->next)
	{
		blit(e->texture, e->x, e->y);
	}
}

static void drawBullets(void)
{
	Entity *b;
	
	for (b = stage.bulletHead.next ; b != NULL ; b = b->next)
	{
		blit(b->texture, b->x, b->y);
	}
}

int collision(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2)
{
	return (MAX(x1, x2) < MIN(x1 + w1, x2 + w2)) && (MAX(y1, y2) < MIN(y1 + h1, y2 + h2));
}

static int bulletHitFighter(Entity *b)
{
	Entity *e;
	
	for (e = stage.fighterHead.next ; e != NULL ; e = e->next)
	{
		if (e->side != b->side && collision(b->x, b->y, b->w, b->h, e->x, e->y, e->w, e->h))
		{
			b->health = 0;
			e->health = 0;
			
			stage.score++;
	
			//highscore = MAX(stage.score, highscore);
			
			return 1;
		}
	}
	
	return 0;
}

static int hitPlayer(Entity *e)
{
	return collision(player->x, player->y, player->w, player->h, e->x, e->y, e->w, e->h);
}

static void doBackground(void)
{
	if (--backgroundX < -SCREEN_WIDTH)
	{
		backgroundX = 0;
	}
}

static void drawBackground(void)
{
	SDL_Rect dest;
	int x;
	
	for (x = backgroundX ; x < SCREEN_WIDTH ; x += SCREEN_WIDTH)
	{
		dest.x = x;
		dest.y = 0;
		dest.w = SCREEN_WIDTH;
		dest.h = SCREEN_HEIGHT;
		
		SDL_RenderCopy(app.renderer, background, NULL, &dest);
	}
}

void initSounds(void)
{
	memset(sounds, 0, sizeof(Mix_Chunk*) * SND_MAX);
	
	music = NULL;
	
	loadSounds();
}

static void loadSounds(void)
{
	sounds[SND_PLAYER_FIRE] = Mix_LoadWAV("sound/whizz.mp3");
	//sounds[SND_PLAYER_DIE] = Mix_LoadWAV("sound/245372__quaker540__hq-explosion.ogg");
	sounds[SND_ALIEN_DIE] = Mix_LoadWAV("sound/10 Guage Shotgun-SoundBible.com-74120584.ogg");
}

void loadMusic(char *filename)
{
	if (music != NULL)
	{
		Mix_HaltMusic();
		Mix_FreeMusic(music);
		music = NULL;
	}

	music = Mix_LoadMUS(filename);
}

void playMusic(int loop)
{
	Mix_PlayMusic(music, (loop) ? -1 : 0);
}

void playSound(int id, int channel)
{
	Mix_PlayChannel(channel, sounds[id], 0);
}

void initFonts(void)
{
	fontTexture = loadTexture("gfx/font.png");
}

void drawText(int x, int y, int r, int g, int b, char *format, ...)
{
	int i, len, c;
	SDL_Rect rect;
	va_list args;
	
	memset(&drawTextBuffer, '\0', sizeof(drawTextBuffer));

	va_start(args, format);
	vsprintf(drawTextBuffer, format, args);
	va_end(args);
	
	len = strlen(drawTextBuffer);
	
	rect.w = GLYPH_WIDTH;
	rect.h = GLYPH_HEIGHT;
	rect.y = 0;
	
	SDL_SetTextureColorMod(fontTexture, r, g, b);
	
	for (i = 0 ; i < len ; i++)
	{
		c = drawTextBuffer[i];
		
		if (c >= ' ' && c <= 'Z')
		{
			rect.x = (c - ' ') * GLYPH_WIDTH;
			
			blitRect(fontTexture, &rect, x, y);
			
			x += GLYPH_WIDTH;
		}
	}
}

static void drawHud(void)
{
	drawText(10, 10, 255, 255, 255, "SCORE: %03d", stage.score);
}
