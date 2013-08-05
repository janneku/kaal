/*
 * KAAL
 *
 * Copyright 2013 Janne Kulmala <janne.t.kulmala@iki.fi>,
 * Antti Rajamäki <amikaze@gmail.com>
 *
 * Program code and resources are licensed with GNU LGPL 2.1. See
 * lgpl-2.1.txt file.
 */
#include "game.h"
#include "gfx.h"
#include "sfx.h"
#include "world.h"
#include "effects.h"
#include "video.h"
#include "ping.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Game state */
enum {
	GAME,
	BATHROOM,
	BURGER_MINIGAME,
	WAKE_MINIGAME,
	PIZZA_MINIGAME,
	AMIGAGUY,
	SHOP_ES,
	SHOP_BATTERY,
	TECH_MENU,
	SECURITYGUY,
	SAUNA_MINIGAME,
};

/* Weapons */
enum {
	FLASHLIGHT,
	ELECTRICGUN,
	RADIO,
	MEGAPHONE,
	ES,
	NUM_WEAPONS
};

/* Techs */
enum {
	FLASHLIGHT_DIST,
	FLASHLIGHT_ENERGY,
	ELECTRICGUN_SPEED,
	ELECTRICGUN_ENERGY,
	RADIO_SPEED,
	MEGAPHONE_SPEED,
	NUM_TECH
};

/* Meal items */
enum {
	BURGER_FEDORA,
	BURGER_DEBIAN,
	BURGER_WIN,
	BURGER_MAC,
	DRINK_COKE,
	DRINK_JAFFA,
	DRINK_BEER,
	DRINK_WATER,
	PIZZA_FISH,
	PIZZA_MUSHROOM,
	PIZZA_SPIDEREYE,
	PIZZA_PEPPERONI,
	FRIES,
	COFFEE,
	TRAY,
	NUM_MEAL_ITEMS
};

/* Meters */
enum {
	BLADDER,
	SLEEP,
	HYGIENE,
	SUCCESS,
	NUM_METERS
};

enum {
	ITEM_BATTERY,
	ITEM_ELECTRICGUN,
	ITEM_RADIO,
};

struct Desk {
	vec3 pos;
	int id;
	int type;
	float angle;
	Music music;
	Object computer;
	Object speaker;
	Object table;
	Object snake;
	Object sit;
	bool broken;
	Material screen;
	Material clothes;
};

struct Track {
	int a, b;
};

struct Walker {
	int id;
	double speed;
	Object obj;
	int target;
	double x;
	vec3 from, to;
	Material clothes;
};

struct Sleeper {
	World *world;
	vec3 pos;
	Object obj;
	bool waking;
};

struct Item {
	int type;
	vec3 pos;
	Object obj;
	Light light;
};

struct PickItem {
	int meal_item;
	Object obj;
};

struct Smoke {
	vec3 pos, vel;
	int id;
	Color color;
	double time;
};

namespace {

const float white[] = {1, 1, 1, 1};
const float black[] = {0, 0, 0, 1};

const double FLASHLIGHT_FOV = 0.15;
const double ELECTRIC_DIST = 1000;
const double GRAVITY = 300;
const double PLAYER_RAD = 13;
const double PLAYER_THRUST = 400;
const double KAAL_TIME = 320;
const double LEVEL_TIME = 620;
const double MINUTE = 80;
const double DEMO_TIME = 20;
const double STARS_TIME = 10;
const int ES_COST = 2;
const int BATTERY_COST = 1;
const int NUM_MEALS = 10;
const int WAKE_COUNT = 20;
const int BURGER_PAY = 5;
const int TECH_LEVELS = 3;
const int MAX_ORDER_ITEMS = 4;

const vec3 CAM_POS(0, 12, 0);
const vec3 WEAPON_POS(2, -3, -6); /* transformed by player's view */
const vec3 THIRDPERSON_POS(0, 5, 40); /* transformed by player's view */
const vec3 BIGSCREEN(-431, -30, -85);

const int cheat_seq[] = {
	SDLK_a,
	SDLK_s,
	SDLK_m,
	SDLK_i,
	SDLK_t,
};

const char *meter_names[] = {
	"Bladder",
	"Sleep",
	"Hygiene",
	"KAAL success",
};
const Color meter_colors[] = {
	Color(0.7, 1, 0),
	Color(0.2, 0.2, 1),
	Color(1, 0.2, 0.7),
	Color(0.2, 1, 0.7),
};

const char *songs[] = {
	"arenapoppt1.ogg",
	"kaalz.ogg",
	"assyrock.ogg",
};
const char *gamevideos[] = {
	"cs.png",
	"wow.png",
	"fr.png",
	"sorvi.png",
	"seko.png",
};
const char *demovideos[] = {
	"second.png",
	"phenomena.png",
};
const char *jumpsounds[] = {
	"hyppy1.ogg",
	"hyppy2.ogg",
};
const char *fill_sounds[] = {
	"vedenotto1.ogg", "vedenotto2.ogg",
};
const char *stove_sounds[] = {
	"kiuas1.ogg", "kiuas2.ogg",
};


const vec3 trackpoints[] = {
	vec3(-300, -205, -180),
	vec3(-300, -205, 20),

	vec3(-180, -205, -180),
	vec3(-180, -205, 20),

	vec3(-60, -205, -180),
	vec3(-60, -205, 20),

	vec3(60, -205, -180),
	vec3(60, -205, 20),

	/* Stairs */
	vec3(-360, -160, -240),
	vec3(-360, -160, 80),
	vec3(120, -160, -240),
	vec3(120, -160, 80),

	/* hallway, 12 -> */
	vec3(350, -151, 410),
	vec3(620, -151, -90),
	vec3(450, -151, -440),
	vec3(-110, -151, -660),
	vec3(-610, -151, -510),
	vec3(-900, -151, -130),
	vec3(-600, -151, 400),
	vec3(-130, -151, 470),

	/* second floor, 20 -> */
	vec3(360, -21, 390),
	vec3(590, -21, 80),
	vec3(590, -21, -200),
	vec3(390, -21, -500),
	vec3(-110, -21, -660),
	vec3(-600, -21, -540),
	vec3(-830, -21, -250),
	vec3(-820, -21, 10),
	vec3(-640, -21, 370),
	vec3(-120, -21, 480),

	/* 30 */
	{-700, -151, -520},
	{-840, -151, -370},
	{-60, -151, -750},
	{-140, -151, -750},
};

const Track tracks[] = {
	/* arena */
	{0, 1},
	{2, 3},
	{4, 5},
	{6, 7},
	{0, 2},
	{2, 4},
	{4, 6},
	{1, 3},
	{3, 5},
	{5, 7},
	{0, 8},
	{1, 9},
	{6, 10},
	{7, 11},

	/* hallway */
	{12, 13},
	{13, 14},
	{14, 15},
	{15, 16},
	{16, 17},
	{17, 18},
	{18, 19},
	{19, 12},

	/* second floor */
	{20, 21},
	{21, 22},
	{22, 23},
	{23, 24},
	{24, 25},
	{25, 26},
	{26, 27},
	{27, 28},
	{28, 29},
	{29, 20},

	/* extra path around entry */
	{15, 30},
	{30, 31},
	{31, 17},

	/* loop in asus */
	{15, 32},
	{32, 33},
	{33, 15},
};

/* This defines the switch points between arena and the hallway */
const Plane arena_planes[] = {
	{vec3(-0.675, 0.3, 0.675), -510},
	{vec3(-0.675, 0.3, -0.675), -400},
	{vec3(0.675, 0.3, 0.675), -680},
	{vec3(0.675, 0.3, -0.675), -570},
	{vec3(0, 0.3, 0.954), -510},
	{vec3(0, 0.3, -0.954), -360},
	{vec3(-0.954, 0.3, 0), -480},
	{vec3(0.954, 0.3, 0), -700},
};

const Plane sauna_planes[] = {
	{vec3(-1, 0, 0), 200},
	{vec3(-0.3, 0, -0.954), 390},
	{vec3(0, -1, 0), 160},
};

const Plane sauna_game_planes[] = {
	{vec3(-1, 0, 0), 490},
	{vec3(0, 0, -1), 370},
	{vec3(0, -1, 0), 160},
};

/* This defines the switch points to ASM burger game */
const Plane burger_planes[] = {
	{vec3(1, 0, 0), -790},
	{vec3(-1, 0, 0), 700},
	{vec3(0, 0, 1), -250},
	{vec3(0, 0, -1), 40},
	{vec3(0, -1, 0), 55},
	{vec3(0, 1, 0), -150},
};

/* Pizza Box minigame */
const Plane pizza_planes[] = {
	{vec3(1, 0, 0), -450},
	{vec3(-1, 0, 0), 260},
	{vec3(0, 0, 1), -560},
	{vec3(0, 0, -1), 480},
	{vec3(0, -1, 0), 55},
	{vec3(0, 1, 0), -150},
};

/* ES shop */
const Plane es_planes[] = {
	{vec3(1, 0, 0), -730},
	{vec3(-1, 0, 0), 660},
	{vec3(0, 0, 1), -400},
	{vec3(0, 0, -1), 320},
	{vec3(0, -1, 0), 55},
	{vec3(0, 1, 0), -150},
};

/* Battery shop */
const Plane battery_planes[] = {
	{vec3(1, 0, 0), 210},
	{vec3(-1, 0, 0), -260},
	{vec3(0, 0, 1), -630},
	{vec3(0, 0, -1), 590},
	{vec3(0, -1, 0), 55},
	{vec3(0, 1, 0), -150},
};

const vec3 bathrooms[] = {
	vec3(495, -140, 155),
	vec3(530, -140, 45),
};

const vec3 showers[] = {
	vec3(-590, -190, -440),
	vec3(-590, -190, -410),
	vec3(-590, -190, -380),
};

struct TechLevel {
	const char *descr;
	double value;
};

struct TechInfo {
	int weapon;
	const char *descr;
	const TechLevel levels[TECH_LEVELS];
};

const TechInfo techs[] = {
	{FLASHLIGHT, "Flashlight distance", {
		{"20 m", 300},
		{"25 m", 400},
		{"35 m", 500}
	}},

	{FLASHLIGHT, "Flashlight energy efficiency", {
		{"Supermarket", 1},
		{"Amateur", 0.9},
		{"Super Green", 0.7}
	}},

	{ELECTRICGUN, "Electric Gun charge time", {
		{"Normal", 20},
		{"Faster", 10},
		{"Instant", 5},
	}},

	{ELECTRICGUN, "Electric Gun battery life", {
		{"10 shots", 10},
		{"12 shots", 8},
		{"20 shots", 5},
	}},

	{RADIO, "KAAL Radio setup time", {
		{"Normal", 300},
		{"Faster", 200},
		{"Super fast", 100},
	}},

	{MEGAPHONE, "Megaphone cool off time", {
		{"Normal", 60},
		{"Faster", 45},
		{"Super fast", 30},
	}},
};

struct WeaponInfo {
	const char *name;
	const char *model;
	const char *pick_model;
};

const WeaponInfo weapons[] = {
	{"Flashlight", "flashlight.obj", NULL},
	{"Electric Gun", "electricgun.obj", NULL},
	{"KAAL Radio", "radio.obj", NULL},
	{"Megaphone", "megaphone.obj", NULL},
	{"ES energy drink", "es.obj", NULL},
};

struct WeaponInfo meal_items[] = {
	{"Fedora Burger", "burger1.obj", NULL},
	{"Debian Burger", "burger2.obj", NULL},
	{"Win Burger", "burger3.obj", NULL},
	{"Mac Burger", "burger4.obj", NULL},
	{"Coke", "cup.obj", NULL},
	{"Jaffa", "cup.obj", NULL},
	{"Beer", "cup.obj", NULL},
	{"Water", "cup.obj", NULL},
	{"Fish Pizza", "slice.obj", "fishpizza.obj"},
	{"Mushroom Pizza", "slice.obj", "shroompizza.obj"},
	{"Spider Eye Pizza", "slice.obj", "spidereyepizza.obj"},
	{"Pepperoni Pizza", "slice.obj", "pepperonipizza.obj"},
	{"Fries", "fries.obj", NULL},
	{"Coffee", "coffee.obj", NULL},
	{"Tray", "tray.obj", NULL},
};

struct LevelInfo {
	const char *name;
	bool kill_lights;
	double kaal_effect;
	double voluntary;
	double ignorance;
};

const LevelInfo levels[] = {
	{"GameDev",       true, 10, 50, 10000},
	{"Fast music",   false, 10, 50, 10000},
	{"Fast graphics", true, 15, 100, 5000},
	{"Oldskool demo", true, 15, 100, 5000},
	{"Music",        false, 15, 200, 2000},
	{"Graphics",      true, 20, 200, 2000},
	{"1k intro",      true, 30, 500, 1000},
	{"Real wild",     true, 30, 500, 1000},
	{"Short film",    true, 40, 1000, 500},
	{"4k intro",      true, 40, 1000, 500},
	{"Demo",          true, 50, 1000, 300},
};

const char *curses[] = {
	"curse1.ogg", "curse2.ogg",
};

const char *snake_models[] = {
	"snake.obj",
	"snake2.obj",
};

const char *computer_models[] = {
	"laptop.obj",
	"laptop2.obj",
	"pc.obj",
	"pc2.obj",
	"monitor.obj",
	"monitor2.obj",
	"chasis1.obj",
};

const char *yells[] = {
	"kaal1.ogg", "kaal2.ogg", "kaal3.ogg", "kaal4.ogg",
};

bool have_weapon[NUM_WEAPONS];
int weapon;
float player_yaw, player_pitch;
std::list<Desk> desks;
std::list<Desk> oldskools;
std::list<Desk> gamers;
std::list<Walker> walkers;
std::list<Smoke> smokes;
std::vector<vec3> bolt;
double bolt_time;
double video_time;
double meters[NUM_METERS];
double energy;
Object player;
Object weap;
Material player_clothes;
bool thirdperson;
Music kaal;
Music demo;
const Texture *weapon_pics[NUM_WEAPONS];
Light weapon_light;
std::list<Item> items;
const Model *persons[2];
const Model *personslowpoly[2];
bool paused;
bool firing;
bool zooming;
std::string message;
double message_time;
double level_time;
double megaphone_delay;
double electricgun_delay;
double megaphone_active;
int level;
Video video;
World arena;
World hallway;
World sauna;
World *world;
int money;
std::list<Sleeper> sleepers;
std::vector<Desk *> speaker_selection;
double radio_delay;
bool end;
std::list<PickItem> pick_items;
Object tray_items[4];
int order_items[MAX_ORDER_ITEMS];
int meal_item;
const TechLevel *tech_level[NUM_TECH];
int upgrades;
bool show_todo;
int meal_counter;
int wake_counter;
bool todo_burger;
bool todo_pizza;
bool todo_wake;
Object standers[2];
Object securityguy;
int state;
bool show_weapons;
double weapons_slide;
double todo_slide;
int cheat;
double motivation;
Object tux;
bool hold_pail;
double pail_water;
double heat;
Object pail;
Object stove;
double sauna_anim;

void show_message(const char *str)
{
	message = str;
	message_time = 5;
}

Matrix dir_matrix(double yaw, double pitch)
{
	vec3 forward(sin(yaw) * cos(pitch),
		    -sin(pitch),
		    -cos(yaw) * cos(pitch));
	return look_at(forward, vec3(0, 1, 0));
}

double flashlight_reach()
{
	weap.set_world(NULL);
	double reach = tech_level[FLASHLIGHT_DIST]->value;
	world->raytrace(weap.pos(), weap.matrix().forward, &reach);
	for (int i = 0; i < 8; ++i) {
		double a = i * (M_PI / 4);
		vec3 dir(cos(a) * FLASHLIGHT_FOV,
			 sin(a) * FLASHLIGHT_FOV, -1);
		double d = tech_level[FLASHLIGHT_DIST]->value;
		world->raytrace(weap.pos(), transform(dir, weap.matrix()),
				&d);
		reach = std::max(reach, d);
	}
	weap.set_world(world);

	return std::min(reach + 10, tech_level[FLASHLIGHT_DIST]->value);
}

void draw_flashlight(double dist)
{
	glLoadIdentity();
	glColor4fv(white);
	glBegin(GL_TRIANGLES);
	for (int i = 0; i < 32; ++i) {
		vec3 p = weap.pos();
		double a = i * (M_PI / 16);
		vec3 dir(cos(a) * FLASHLIGHT_FOV,
			 sin(a) * FLASHLIGHT_FOV, -1);
		vec3 p1 = weap.pos() + transform(dir, weap.matrix()) * dist;
		a = (i + 1) * (M_PI / 16);
		dir = vec3(cos(a) * FLASHLIGHT_FOV,
			   sin(a) * FLASHLIGHT_FOV, -1);
		vec3 p2 = weap.pos() + transform(dir, weap.matrix()) * dist;
		glVertex3fv(&p.x);
		glVertex3fv(&p1.x);
		glVertex3fv(&p2.x);

		p = weap.pos() + weap.matrix().forward * dist;
		glVertex3fv(&p1.x);
		glVertex3fv(&p.x);
		glVertex3fv(&p2.x);
	}
	glEnd();
}

void render_level(bool light)
{
	static const Texture *compo;
	static const Model *skybox;
	static const Texture *smoke;
	if (compo == NULL) {
		compo = get_texture("design.png");
		smoke = get_texture("smoke.png");
		skybox = get_model("skybox.obj");
		skybox->get_material("None")->brightness = 0.1;
		skybox->get_material("None_sky.png")->brightness = 0.5;
	}

	Material *mat = arena.get_material("Material");
	if (video.playing() && video.get_frame()->loaded()) {
		mat->texture = video.get_frame();
		mat->light_color = video.get_frame()->color();
	} else {
		mat->texture = compo;
		mat->light_color = compo->color();
	}

	GLState gl;
	gl.enable(GL_DEPTH_TEST);
	gl.enable(GL_CULL_FACE);

	double fov = zooming ? 30 : 45;

	Camera camera;
	camera.matrix = dir_matrix(player_yaw, player_pitch);

	camera.pos = player.pos() + CAM_POS;
	if (thirdperson) {
		vec3 dir = transform(THIRDPERSON_POS, camera.matrix);
		double dist = 1;
		world->raytrace(camera.pos, dir, &dist);
		camera.pos += dir * dist;
	}

	double fov_y = tan(fov * (M_PI / 180)) * 0.5;
	prepare_camera(&camera, fov_y * scr_width / scr_height, fov_y,
		       RENDER_DIST);

	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(fov, (double) scr_width / scr_height, 1, RENDER_DIST);
	mult_matrix_reverse(camera.matrix);
	glTranslatef(-camera.pos.x, -camera.pos.y, -camera.pos.z);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	int flags = light ? RENDER_BLOOM : 0;

	world->render(camera, flags);

	if (!video.playing()) {
		char buf[256];
		if (level_time < DEMO_TIME) {
			strcpy(buf, "The compo is over.");
		} else {
			sprintf(buf, "%s compo\nstarts in %d minutes\n\n",
				levels[level].name,
				int((level_time - DEMO_TIME) / 60));
			if (level_time < KAAL_TIME) {
				if (levels[level].kill_lights) {
					strcat(buf, "Please kill all\n"
					       "audio and lights!");
				} else {
					strcat(buf, "Please kill all audio!");
				}
			}
		}
		if (light) {
			glColor4fv(black);
		} else {
			glColor4fv(white);
		}
		glLoadIdentity();
		glTranslatef(BIGSCREEN.x, BIGSCREEN.y, BIGSCREEN.z);
		mult_matrix(Matrix(vec3(0, 0, 0), vec3(0, 0, -1), vec3(0, -1, 0)));
		glScalef(0.3, 0.3, 1);
		glTranslatef(-large_font.text_width(buf)/2, 0, 0);
		large_font.draw_text(buf);
	}

	if (world == &hallway) {
		glDepthMask(GL_FALSE);
		glLoadIdentity();
		glTranslatef(camera.pos.x, camera.pos.y, camera.pos.z);
		begin_rendering(0, flags);
		skybox->render(flags | RENDER_LIGHTS_ON);
		end_rendering();
		glDepthMask(GL_TRUE);
	}

	{
		GLState gl;
		gl.enable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		world->render(camera, flags | RENDER_GLASS);
	}

	glDepthMask(GL_FALSE);

	/*if (light) {
		glColor4f(0, 0, 0, 0.5);
	} else {
		glColor4f(1, 1, 1, 0.5);
	}
	glLoadIdentity();
	{
		GLState gl;
		gl.enable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	for (size_t i = 0; i < lengthof(sauna_planes); ++i) {
		glDisable(GL_CULL_FACE);
		const Plane *plane = &sauna_planes[i];
		glBegin(GL_TRIANGLES);
		vec3 right = normalize(cross(plane->norm, vec3(0, 1, 0))) * RENDER_DIST;
		vec3 up = cross(right, plane->norm);
		for (int j = 0; j < 32; ++j) {
			vec3 p = plane->norm * plane->pos;
			glVertex3fv(&p.x);
			double a = j * (M_PI / 16);
			p = plane->norm * plane->pos + up * sin(a) +
			    right * cos(a);
			glVertex3fv(&p.x);
			a = (j + 1) * (M_PI / 16);
			p = plane->norm * plane->pos + up * sin(a) +
			    right * cos(a);
			glVertex3fv(&p.x);
		}
		glEnd();
		glEnable(GL_CULL_FACE);
	}
	}*/

	{
		GLState gl;
		gl.enable(GL_BLEND);
		gl.enable(GL_TEXTURE_2D);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		smoke->bind();
		glLoadIdentity();

		glBegin(GL_QUADS);
		for(const Smoke &smoke : smokes) {
			if (light) {
				glColor4fv(black);
			} else {
				Color c = smoke.color;
				c.a *= smoke.time;
				glColor4fv(&c.r);
			}

			double size = 20 - smoke.time * 20;
			glTexCoord2f(0, 0);
			vec3 p = smoke.pos - camera.matrix.right * size
				 - camera.matrix.up * size;
			glVertex3fv(&p.x);
			glTexCoord2f(1, 0);
			p = smoke.pos + camera.matrix.right * size
				 - camera.matrix.up * size;
			glVertex3fv(&p.x);
			glTexCoord2f(1, 1);
			p = smoke.pos + camera.matrix.right * size
				 + camera.matrix.up * size;
			glVertex3fv(&p.x);
			glTexCoord2f(0, 1);
			p = smoke.pos - camera.matrix.right * size
				 + camera.matrix.up * size;
			glVertex3fv(&p.x);
		}
		glEnd();
	}

	if (bolt_time > 0) {
		GLState gl;
		gl.enable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glLineWidth(2);
		glLoadIdentity();
		glBegin(GL_LINE_STRIP);
		if (light) {
			glColor4f(0.2, 0.2, 0.4, bolt_time);
		} else {
			glColor4f(0.5, 0.6, 1, bolt_time);
		}
		for (const vec3 &p : bolt) {
			glVertex3fv(&p.x);
		}
		glEnd();
	}

	if (firing && weapon == FLASHLIGHT && energy > 0) {
		double dist = flashlight_reach();

		camera.pos = weap.pos();
		camera.matrix = weap.matrix();
		prepare_camera(&camera, FLASHLIGHT_FOV, FLASHLIGHT_FOV, dist);

		player.set_world(NULL);
		weap.set_world(NULL);
		gl.enable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		GLState gl;
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		gl.enable(GL_STENCIL_TEST);
		glStencilFunc(GL_ALWAYS, 0, -1);
		glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
		draw_flashlight(dist);
		world->render(camera, RENDER_SHADOW_VOL);

		glStencilOp(GL_KEEP, GL_KEEP, GL_DECR);
		glFrontFace(GL_CW);
		draw_flashlight(dist);
		glFrontFace(GL_CCW);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

		gl.enable(GL_BLEND);
		glStencilFunc(GL_NOTEQUAL, 0, -1);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glColor4f(1, 0.9, 0.7, std::min(energy, 1.0) * 0.2);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, 1, 0, 1, -1, 1);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		glBegin(GL_QUADS);
		glVertex3f(0, 0, 0);
		glVertex3f(1, 0, 0);
		glVertex3f(1, 1, 0);
		glVertex3f(0, 1, 0);
		glEnd();

		weap.set_world(world);
		player.set_world(world);
	}
	glDepthMask(GL_TRUE);
}

void draw_bar(double value)
{
	static const Texture *bar;
	if (bar == NULL) {
		bar = get_texture("bar.png");
	}

	value *= 0.01;

	const int WIDTH = 256;
	const int HEIGHT = 32;

	GLState gl;
	gl.enable(GL_BLEND);
	gl.enable(GL_TEXTURE_2D);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	bar->bind();

	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex3f(0, 0, 0);
	glTexCoord2f(value, 0);
	glVertex3f(value * WIDTH, 0, 0);
	glTexCoord2f(value, 1);
	glVertex3f(value * WIDTH, HEIGHT, 0);
	glTexCoord2f(0, 1);
	glVertex3f(0, HEIGHT, 0);

	glColor4f(0.5, 0.5, 0.5, 1);
	glTexCoord2f(value, 0);
	glVertex3f(value * WIDTH, 0, 0);
	glTexCoord2f(1, 0);
	glVertex3f(WIDTH, 0, 0);
	glTexCoord2f(1, 1);
	glVertex3f(WIDTH, HEIGHT, 0);
	glTexCoord2f(value, 1);
	glVertex3f(value * WIDTH, HEIGHT, 0);
	glEnd();
}

void quad(int x, int y, int width, int height)
{
	glBegin(GL_QUADS);
	glVertex3f(x, y, 0);
	glVertex3f(x, y + height, 0);
	glVertex3f(x + width, y + height, 0);
	glVertex3f(x + width, y, 0);
	glEnd();
}

void draw_minimap()
{
	static const Texture *maps[2];
	if (maps[0] == NULL) {
		maps[0] = get_texture("map1.png");
		maps[1] = get_texture("map2.png");
	}

	const int WIDTH = 2048;
	const int HEIGHT = 1024;

	vec2 player_pos((player.pos().x - 190) * 0.6876 + 1243,
			(player.pos().z + 330) * 0.6876 + 340);

	vec2 map_pos = player_pos;
	if (map_pos.x < 128) map_pos.x = 128;
	if (map_pos.y < 128) map_pos.y = 128;
	if (map_pos.x > WIDTH - 128) map_pos.x = WIDTH - 128;
	if (map_pos.y > HEIGHT - 128) map_pos.y = HEIGHT - 128;

	{
		GLState gl;
		gl.enable(GL_BLEND);
		gl.enable(GL_TEXTURE_2D);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		int floor = (player.pos().y > -55);
		maps[floor]->bind();

		glLoadIdentity();
		glTranslatef(144, 144, 0);
		glColor4f(1, 1, 1, 0.8);
		glBegin(GL_TRIANGLES);
		for (int i = 0; i < 32; ++i) {
			double a = i * (M_PI / 16);
			glTexCoord2f((cos(a) * 128 + map_pos.x) / WIDTH,
				     (sin(a) * 128 + map_pos.y) / HEIGHT);
			glVertex3f(cos(a) * 128, sin(a) * 128, 0);
			a = (i + 1) * (M_PI / 16);
			glTexCoord2f((cos(a) * 128 + map_pos.x) / WIDTH,
				     (sin(a) * 128 + map_pos.y) / HEIGHT);
			glVertex3f(cos(a) * 128, sin(a) * 128, 0);
			glTexCoord2f(map_pos.x / WIDTH, map_pos.y / HEIGHT);
			glVertex3f(0, 0, 0);
		}
		glEnd();
	}

	glLoadIdentity();
	glTranslatef(player_pos.x - map_pos.x + 144,
		     player_pos.y - map_pos.y + 144, 0);
	glRotatef(player_yaw * (180 / M_PI), 0, 0, 1);
	glColor4fv(black);
	glBegin(GL_TRIANGLES);
	glVertex3f(0, -10, 0);
	glVertex3f(5, 5, 0);
	glVertex3f(-5, 5, 0);
	glEnd();
}

void draw_meters()
{
	for (int i = 0; i < NUM_METERS; ++i) {
		glLoadIdentity();
		glTranslatef(16, 300 + i * 50, 0);
		glColor4fv(white);
		small_font.draw_text(meter_names[i]);
		glTranslatef(0, 5, 0);
		if (meters[i] < 10) {
			glColor4f(0.5 + sin(SDL_GetTicks() * 0.005), 0, 0, 1);
		} else {
			glColor4fv(&meter_colors[i].r);
		}
		draw_bar(meters[i]);
	}
}

void draw_todo()
{
	static const Texture *todo;

	if (todo == NULL) {
		todo = get_texture("todo.png");
	}

	if (todo_slide <= 0) return;

	glLoadIdentity();
	glTranslatef(scr_width - todo_slide * 270, 50, 0);
	glColor4fv(white);
	todo->draw();

	glTranslatef(20, 120, 0);

	glColor4fv(black);

	if (level_time > DEMO_TIME && level_time < KAAL_TIME) {
		if (levels[level].kill_lights) {
			todo_font.draw_text("Get all lights and\nspeakers turned off.");
		} else {
			todo_font.draw_text("Get all speakers turned\noff (not lights).");
		}
		glTranslatef(0, 70, 0);
	}
	if (level_time > DEMO_TIME && level_time < MINUTE) {
		todo_font.draw_text("ONE MINUTE LEFT!");
		glTranslatef(0, 40, 0);
	}
	if (todo_burger) {
		todo_font.draw_text("Work at ASM burger\nto earn some money.");
		glTranslatef(0, 70, 0);
	}
	if (todo_pizza) {
		todo_font.draw_text("Intership at Pizza\nBox for income.");
		glTranslatef(0, 70, 0);
	}
	if (todo_wake) {
		todo_font.draw_text("Help the security crew\non second floor.");
		glTranslatef(0, 70, 0);
	}
	if (upgrades > 0) {
		todo_font.draw_text("Talk to the Amiga guy\nto upgrade weapons.");
		glTranslatef(0, 70, 0);
	}
	if (meters[BLADDER] < 10) {
		todo_font.draw_text("Find a bathroom.");
		glTranslatef(0, 40, 0);
	}
	if (meters[SUCCESS] < 10) {
		todo_font.draw_text("Talk to security to\nimprove your success!");
		glTranslatef(0, 70, 0);
	}
	if (meters[SLEEP] < 10) {
		todo_font.draw_text("You feel tired.\nDrink some ES!");
		glTranslatef(0, 70, 0);
	}
	if (meters[HYGIENE] < 10) {
		todo_font.draw_text("Visit sauna to\nwash the sweat off.");
		glTranslatef(0, 70, 0);
	}
	if (level_time > DEMO_TIME && level == 0) {
		todo_font.draw_text("Press P to skip\nto the compo now.");
		glTranslatef(0, 70, 0);
	}
}

void draw_hud()
{
	static const Texture *crosshair;
	static const Texture *box;
	static const Texture *flatbox;
	static const Texture *order;
	static const Texture *pizzaorder;
	static const Texture *wakeup;
	static const Texture *star;

	if (crosshair == NULL) {
		crosshair = get_texture("crosshair.png");
		box = get_texture("box.png");
		flatbox = get_texture("flatbox.png");
		order = get_texture("order.png");
		pizzaorder = get_texture("pizzaorder.png");
		wakeup = get_texture("wakeup.png");
		star = get_texture("star.png");
	}

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, scr_width, scr_height, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	char buf[256];

	switch (state) {
	case BURGER_MINIGAME:
	case PIZZA_MINIGAME:
		glLoadIdentity();
		glTranslatef(scr_width - 256 - 16, 200, 0);
		glColor4fv(white);
		if (state == BURGER_MINIGAME) {
			order->draw();
		} else {
			pizzaorder->draw();
		}

		glTranslatef(30, 100, 0);
		glColor4fv(black);

		sprintf(buf , "Order %d/%d:", meal_counter + 1, NUM_MEALS);
		order_font.draw_text(buf);
		glTranslatef(0, 40, 0);

		for (int i = 0; i < MAX_ORDER_ITEMS; ++i) {
			if (order_items[i] < 0) break;

			order_font.draw_text(meal_items[order_items[i]].name);
			glTranslatef(0, 30, 0);
		}
		break;

	case WAKE_MINIGAME:
		glLoadIdentity();
		glTranslatef(scr_width - 256 - 16, 200, 0);
		glColor4fv(white);
		wakeup->draw();

		glTranslatef(30, 100, 0);
		glColor4fv(black);

		sprintf(buf , "Wake up %d people", wake_counter);
		todo_font.draw_text(buf);
		break;

	case SAUNA_MINIGAME:
		glLoadIdentity();
		glTranslatef(16, scr_height - 100, 0);
		glColor4fv(white);
		small_font.draw_text("Heat");
		glTranslatef(0, 5, 0);
		glColor4f(1, 0, 0, 1);
		draw_bar(heat);
		glTranslatef(0, 50, 0);

		glColor4fv(white);
		small_font.draw_text("Hygiene");
		glTranslatef(0, 5, 0);
		if (meters[HYGIENE] < 10) {
			glColor4f(1, 0, 0, 1);
		} else {
			glColor4f(1, 0.2, 0.7, 1);
		}
		draw_bar(meters[HYGIENE]);
		break;

	case GAME:
	case AMIGAGUY:
	case SHOP_ES:
	case SHOP_BATTERY:
	case SECURITYGUY:
	case BATHROOM:
		draw_minimap();

		sprintf(buf, "ByteMoney: %d", money);
		glLoadIdentity();
		glTranslatef(scr_width - small_font.text_width(buf) - 16, 25, 0);
		glColor4fv(white);
		small_font.draw_text(buf);

		draw_meters();

		glLoadIdentity();
		glTranslatef(16, scr_height - 50, 0);
		glColor4fv(white);
		small_font.draw_text("Motivation");
		glTranslatef(0, 5, 0);
		if (motivation < 26) {
			glColor4f(0.5 + sin(SDL_GetTicks() * 0.005), 0, 0, 1);
		}
		draw_bar(motivation);

		draw_todo();
		break;
	}

	if (weapons_slide > 0) {
		for (int i = 0; i < NUM_WEAPONS; ++i) {
			if (!have_weapon[i]) continue;

			GLState gl;
			glLoadIdentity();
			glColor4fv(white);
			glTranslatef(scr_width/2 - (NUM_WEAPONS - 1) * 40 + i * 80 - 64,
				     scr_height - weapons_slide * 128, 0);
			weapon_pics[i]->draw();
			glTranslatef(60, 70, 0);
			char buf[64];
			sprintf(buf, "%d", i + 1);
			glColor4fv(white);
			small_font.draw_text(buf);
		}
	}

	glLoadIdentity();
	glTranslatef(scr_width - 304, scr_height - 50, 0);
	if (state == SAUNA_MINIGAME) {
		glColor4fv(white);
		small_font.draw_text("Pail water");
		glTranslatef(0, 5, 0);
		glColor4f(0, 0, 1, 1);
		draw_bar(pail_water);

	} else if (meal_item >= 0) {
		glColor4fv(white);
		small_font.draw_text(meal_items[meal_item].name);

	} else
	switch (weapon) {
	case FLASHLIGHT:
		glColor4fv(white);
		small_font.draw_text("Flashlight energy");
		glTranslatef(0, 5, 0);
		glColor4f(0.2, 0.6, 1, 1);
		draw_bar(energy);
		break;

	case ELECTRICGUN:
		if (electricgun_delay > 0) {
			glColor4fv(white);
			small_font.draw_text("Electric Gun recharge");
			glTranslatef(0, 5, 0);
			draw_bar(100 - electricgun_delay * 100);
		} else {
			glColor4fv(white);
			sprintf(buf, "Electric Gun ammo %d / %d",
				(int) (energy / tech_level[ELECTRICGUN_ENERGY]->value),
				(int) (100 / tech_level[ELECTRICGUN_ENERGY]->value));
			small_font.draw_text(buf);
			glTranslatef(0, 5, 0);
			glColor4f(0.2, 0.6, 1, 1);
			draw_bar(energy);
		}
		break;

	case MEGAPHONE:
		glColor4fv(white);
		small_font.draw_text("Megaphone cool off time");
		glTranslatef(0, 5, 0);
		glColor4fv(white);
		draw_bar(100 - megaphone_delay * 100);
		break;

	case RADIO:
		glColor4fv(white);
		small_font.draw_text("Radio KAAL setup");
		glTranslatef(0, 5, 0);
		glColor4fv(white);
		draw_bar(100 - radio_delay * 100);
		break;
	}

	if (message_time > 0) {
		glLoadIdentity();
		glTranslatef(scr_width/2 -
			     small_font.text_width(message.c_str())/2, 25, 0);
		glColor4f(1, 1, 1, message_time);
		small_font.draw_text(message.c_str());
	}

	if (state != GAME || weapon == FLASHLIGHT || weapon == ELECTRICGUN) {
		glLoadIdentity();
		glTranslatef(scr_width/2 - 8, scr_height/2 - 8, 0);
		glColor4fv(white);
		crosshair->draw();
	}

	buf[0] = 0;
	switch (state) {
	case BATHROOM:
		strcpy(buf, "Click to use bathroom");
		break;

	case SECURITYGUY:
		strcpy(buf, "Click to talk to the security guy");
		break;

	case AMIGAGUY:
		strcpy(buf, "Click to talk to the Amiga guy");
		break;

	case SHOP_ES:
		strcpy(buf, "Click to buy ES\n(costs 2 ByteMoneys)");
		break;

	case SHOP_BATTERY:
		strcpy(buf, "Click to buy a battery\n(costs 1 ByteMoney)");
		break;
	}

	if (buf[0]) {
		glLoadIdentity();
		glColor4fv(white);
		glTranslatef(scr_width/2 - small_font.text_width(buf)/2,
			     scr_height/2, 0);
		small_font.draw_text(buf);
	}

	int mx, my;
	SDL_GetMouseState(&mx, &my);

	if (state == TECH_MENU) {
		int y = std::min((my - 200) * (scr_height - 300 - NUM_TECH * 170) /
				 (scr_height - 300), 0) + 200;
		for (int i = 0; i < NUM_TECH; ++i) {
			const TechInfo *tech = &techs[i];
			if (!have_weapon[tech->weapon]) continue;

			glLoadIdentity();
			glTranslatef(scr_width/2 - 210, y - 10, 0);
			glColor4fv(white);
			small_font.draw_text(tech->descr);

			int cur_level = tech_level[i] - &tech->levels[0];
			for (int level = 0; level < TECH_LEVELS; ++level) {
				const TechLevel *info = &tech->levels[level];
				glLoadIdentity();
				glTranslatef(scr_width/2 - 210 + level * 150, y, 0);
				if (level < 2) {
					glColor4fv(white);
					glLineWidth(2);
					glBegin(GL_LINES);
					glVertex3f(128 - 6, 64, 0);
					glVertex3f(150 + 6, 64, 0);
					glEnd();
				}

				if (cur_level >= level) {
					glColor4fv(white);
					flatbox->draw();
				} else if (level <= cur_level + upgrades) {
					glColor4fv(white);
					box->draw();
				} else {
					glColor4f(0.3, 0.3, 0.3, 1);
					flatbox->draw();
				}
				glTranslatef(0, -10, 0);
				weapon_pics[tech->weapon]->draw();
				glTranslatef(64 - small_font.text_width(info->descr)/2,
					     120, 0);
				small_font.draw_text(info->descr);
			}

			y += 170;
		}
		const char title[] = "Weapon upgrades";
		glLoadIdentity();
		glColor4fv(white);
		glTranslatef(scr_width/2 - large_font.text_width(title)/2, 64, 0);
		large_font.draw_text(title);
	}

	if (level_time < STARS_TIME) {
		int stars = motivation / 26;

		glLoadIdentity();
		glTranslatef(scr_width/2 - 128*3,
			     scr_height/2, 0);
		for (int i = 0; i < 3; ++i) {
			if (i < stars) {
				glColor4f(1, 0.8, 0.5, STARS_TIME - level_time);
			} else {
				glColor4f(0.5, 0.5, 0.5, STARS_TIME - level_time);
			}
			star->draw();
			glTranslatef(256, 0, 0);
		}
	}

	if (paused) {
		glColor4fv(white);
		const char title[] = "Paused";
		glLoadIdentity();
		glTranslatef(scr_width/2 - large_font.text_width(title)/2,
			     scr_height/4, 0);
		large_font.draw_text(title);

		const char text[] = "Are you sure you want to quit? Press Y/N";
		glLoadIdentity();
		glTranslatef(scr_width/2 - small_font.text_width(text)/2,
			     scr_height/2, 0);
		small_font.draw_text(text);
	}
}

void render()
{
	if (quality >= 1) {
		begin_bloom();
		render_level(true);
		end_bloom();
	}
	render_level(false);
	if (quality >= 1) {
		draw_bloom();
	}

	draw_hud();
}

void start_desk(Desk *desk)
{
	if (desk->speaker.model() != NULL && !desk->music.playing()) {
		desk->music.play(songs[rand() % lengthof(songs)],
				 true, 0.2);
		desk->music.seek(frand() * desk->music.length());
	}
	if (desk->computer.model()->get_material("Screen") != NULL) {
		if (!desk->broken) {
			int i = rand() % lengthof(gamevideos);
			desk->screen.texture = get_texture(gamevideos[i]);
			desk->screen.num_frames = 128;
			desk->screen.num_cols = 1;
			if (i >= 3) {
				desk->screen.num_cols = 2;
			}
			desk->computer.set_lights_on(true);
		}
	} else {
		desk->computer.set_lights_on(true);
	}
	desk->snake.set_lights_on(true);
}

double sound_pan(const vec3 &v)
{
	double a;
	if (fabs(v.z) < 1e-6) {
		a = (v.x < 0) ? -M_PI/2 : M_PI/2;
	} else {
		a = atan(v.x / fabs(v.z));
	}
	return a * (1.8 / M_PI) + 1;
}

void stop_desk(Desk *desk, bool force = false)
{
	static const Texture *off;
	if (off == NULL) {
		off = get_texture("off.png");
	}

	desk->music.stop();

	if (!levels[level].kill_lights && !force && frand() < 0.5) {
		return;
	}

	if (desk->computer.lights_on()) {
		Matrix matrix = dir_matrix(player_yaw, 0);
		vec3 reverse_pos = reverse_transform(desk->pos - player.pos(), matrix);
		double volume = 50 / length(desk->pos - player.pos());
		play_sound(get_sound("screen.ogg"), volume, sound_pan(reverse_pos));

		/* Random cursing */
		if (frand() < 0.5 && desk->sit.model() != NULL && force) {
			play_sound(get_sound(curses[rand() % lengthof(curses)]),
				   volume * (2 + frand()),
				   sound_pan(reverse_pos));
		}
	}

	desk->snake.set_lights_on(false);
	desk->computer.set_lights_on(false);

	if (desk->computer.model()->get_material("Screen") != NULL) {
		desk->screen.texture = off;
		desk->screen.frame = 0;
		desk->screen.num_frames = 0;
	}
}

void kill_desk(Desk *desk)
{
	const size_t NUM_SCREENS = 2;
	static const Texture *screens[NUM_SCREENS];
	if (screens[0] == NULL) {
		screens[0] = get_texture("bsod.png");
		screens[1] = get_texture("broken.png");
	}

	if (desk->computer.model()->get_material("Screen") != NULL &&
	    (frand() < 0.5) && !desk->broken) {
		/* The computer crashed - Turn lights on even if
		 * they were off.
		 */
		desk->broken = true;
		desk->screen.texture = screens[rand() % NUM_SCREENS];
		desk->screen.frame = 0;
		desk->screen.num_frames = 0;
		desk->computer.set_lights_on(true);
		desk->music.stop();
		for (int i = 0; i < 10; ++i) {
			Smoke smoke;
			smoke.pos = desk->pos;
			smoke.vel = vec3(frand() - 0.5, frand(), frand() - 0.5) * 10;
			smoke.time = 1;
			double c = frand() * 0.5;
			smoke.color = Color(c, c, c * 2, 0.2);
			smokes.push_back(smoke);
		}
	} else {
		stop_desk(desk, true);
	}
}

void build_table(vec3 pos, double angle, int num, double turn = 0,
		 bool reverse = true)
{
	const double computer_height[] = {
		-1, -1, 11, 8, -1, 7, -1,
	};
	static const Model *computers[lengthof(computer_models)];
	static const Model *snakes[lengthof(snake_models)];
	static const Model *sit[2];
	static const Model *sitlowpoly[2];
	if (computers[0] == NULL) {
		for (size_t i = 0; i < lengthof(computers); ++i) {
			computers[i] = get_model(computer_models[i]);
		}
		computers[6]->get_material("Material.001")->color.a = 0.6;
		computers[6]->get_material("Light.002")->brightness = 0.2;
		for (size_t i = 0; i < lengthof(snakes); ++i) {
			snakes[i] = get_model(snake_models[i]);
			snakes[i]->get_material("Light")->brightness = 0.3;
		}
		sit[0] = get_model("personsit");
		sitlowpoly[0] = get_model("personsitlowpoly.obj");
		sit[1] = get_model("personsit2");
		sitlowpoly[1] = get_model("personsit2lowpoly.obj");
	}

	for (int i = 0; i < num; ++i) {
		desks.push_back(Desk());
		Desk *desk = &desks.back();

		desk->id = rand();
		desk->pos = pos;
		desk->broken = false;
		desk->angle = angle;
		if (reverse && (frand() < 0.5)) {
			desk->angle += M_PI;
		}
		Matrix matrix = dir_matrix(desk->angle, 0);
		desk->computer.set_model(computers[desk->id % lengthof(computers)]);
		desk->computer.move(pos);
		desk->computer.set_matrix(dir_matrix(desk->angle + (frand() * 0.2 - 0.1), 0));

		if (desk->computer.model()->get_material("Screen") != NULL) {
			desk->screen.texture = NULL;
			desk->screen.frame = 0;
			desk->screen.num_frames = 0;
			desk->screen.brightness = 0.1;
			desk->screen.color = Color(1, 1, 1);
			desk->screen.light_color = Color(0.5, 0.6, 0.7);
			desk->computer.replace_material("Screen",
					&desk->screen);
		}
		desk->computer.set_world(&arena);

		if (frand() < 0.5 && reverse) {
			int sex = frand() < 0.8;
			desk->sit.set_model(sit[sex],
					    sitlowpoly[sex]);
			desk->sit.move(pos + matrix.forward * -10 + vec3(0, -11, 0));
			desk->sit.set_matrix(matrix);
			desk->sit.set_world(&arena);

			desk->clothes.texture = NULL;
			desk->clothes.num_frames = 0;
			desk->clothes.brightness = 0;
			desk->clothes.color = Color(frand(), frand(), frand());
			desk->sit.replace_material("Material", &desk->clothes);
		}

		if (frand() < 0.1 && computer_height[desk->id % lengthof(computers)] > 0) {
			desk->snake.set_model(snakes[rand() % lengthof(snakes)]);
			desk->snake.move(pos + vec3(0, computer_height[desk->id % lengthof(computers)], 0));
			desk->snake.set_matrix(matrix);
			desk->snake.set_world(&arena);
		} else if (reverse) {
			speaker_selection.push_back(desk);
		}

		pos += vec3(cos(angle), 0, sin(angle)) * 10;
		angle += turn / num;
	}
}

void build_gamers(vec3 pos, double angle, int num, double turn = 0)
{
	static const Model *computers[lengthof(computer_models)];
	static const Model *sit[2];
	static const Model *sitlowpoly[2];
	if (computers[0] == NULL) {
		for (size_t i = 0; i < lengthof(computers); ++i) {
			computers[i] = get_model(computer_models[i]);
		}
		computers[6]->get_material("Material.001")->color.a = 0.8;
		computers[6]->get_material("Light.002")->brightness = 0.2;
		sit[0] = get_model("personsit");
		sitlowpoly[0] = get_model("personsitlowpoly.obj");
		sit[1] = get_model("personsit2");
		sitlowpoly[1] = get_model("personsit2lowpoly.obj");
	}

	for (int i = 0; i < num; ++i) {
		gamers.push_back(Desk());
		Desk *desk = &gamers.back();

		desk->id = rand();
		desk->pos = pos;
		desk->broken = false;
		desk->angle = angle;
		if (frand() < 0.5) {
			desk->angle += M_PI;
		}
		Matrix matrix = dir_matrix(desk->angle, 0);
		desk->computer.set_model(computers[desk->id % lengthof(computers)]);
		desk->computer.move(pos);
		desk->computer.set_matrix(dir_matrix(desk->angle + (frand() * 0.2 - 0.1), 0));

		if (desk->computer.model()->get_material("Screen") != NULL) {
			desk->screen.texture = NULL;
			desk->screen.frame = 0;
			desk->screen.brightness = 0.1;
			desk->screen.color = Color(1, 1, 1);
			desk->screen.light_color = Color(0.5, 0.6, 0.7);
			desk->computer.replace_material("Screen",
					&desk->screen);

			int i = rand() % lengthof(gamevideos);
			desk->screen.texture = get_texture(gamevideos[i]);
			desk->screen.num_frames = 128;
			desk->screen.num_cols = 1;
			if (i >= 3) {
				desk->screen.num_cols = 2;
			}
		}
		desk->computer.set_world(&hallway);

		if (frand() < 0.5) {
			int sex = frand() < 0.8;
			desk->sit.set_model(sit[sex], sitlowpoly[sex]);
			desk->sit.move(pos + matrix.forward * -10 + vec3(0, -11, 0));
			desk->sit.set_matrix(matrix);
			desk->sit.set_world(&hallway);

			desk->clothes.texture = NULL;
			desk->clothes.num_frames = 0;
			desk->clothes.brightness = 0;
			desk->clothes.color = Color(frand(), frand(), frand());
			desk->sit.replace_material("Material", &desk->clothes);
		}

		pos += vec3(cos(angle), 0, sin(angle)) * 15;
		angle += turn / num;
	}
}

void build_oldskool(vec3 pos, double angle, int num)
{
	const size_t NUM_COMPUTERS = 3;
	const Model *computers[NUM_COMPUTERS];
	computers[0] = get_model("oldskool1.obj");
	computers[1] = get_model("oldskool2.obj");
	computers[2] = get_model("oldskool3.obj");
	computers[2]->get_material("Light")->brightness = 0.1;
	const Model *sit[2];
	const Model *sitlowpoly[2];
	sit[0] = get_model("personsit");
	sitlowpoly[0] = get_model("personsitlowpoly.obj");
	sit[1] = get_model("personsit2");
	sitlowpoly[1] = get_model("personsit2lowpoly.obj");

	for (int i = 0; i < num; ++i) {
		oldskools.push_back(Desk());
		Desk *desk = &oldskools.back();
		desk->id = rand();
		desk->pos = pos;
		desk->angle = angle;
		if (frand() < 0.5) {
			desk->angle += M_PI;
		}

		Matrix matrix = dir_matrix(desk->angle, 0);
		desk->computer.set_model(computers[rand() % lengthof(computers)]);
		desk->computer.move(pos);
		desk->computer.set_matrix(dir_matrix(desk->angle + (frand() * 0.2 - 0.1), 0));

		if (desk->computer.model()->get_material("Screen") != NULL) {
			desk->screen.texture =
				get_texture(demovideos[rand() % lengthof(demovideos)]);
			desk->screen.frame = 0;
			desk->screen.num_frames = 128;
			desk->screen.num_cols = 1;
			desk->screen.brightness = 0.05;
			desk->screen.color = Color(1, 1, 1);
			desk->screen.light_color = Color(0.5, 0.6, 0.7);
			desk->computer.replace_material("Screen",
					&desk->screen);
		}
		desk->computer.set_world(&hallway);

		if (frand() < 0.5) {
			int i = rand() % 2;
			desk->sit.set_model(sit[i],
					    sitlowpoly[i]);
			desk->sit.move(pos + matrix.forward * -10 + vec3(0, -11, 0));
			desk->sit.set_matrix(matrix);
			desk->sit.set_world(&hallway);

			desk->clothes.texture = NULL;
			desk->clothes.num_frames = 0;
			desk->clothes.brightness = 0;
			desk->clothes.color = Color(frand(), frand(), frand());
			desk->sit.replace_material("Material", &desk->clothes);
		}

		pos += vec3(cos(angle), 0, sin(angle)) * 13;
	}
}

void add_sleeping_area(World *world, vec3 pos, double angle, int num,
			double turn = 0)
{
	static const Model *sleeper_model;
	static const Model *sleeperlowpoly_model;
	if (sleeper_model == NULL) {
		sleeper_model = get_model("sleeper");
		sleeper_model->get_material("cloth")->brightness = 0.05;
		sleeperlowpoly_model = get_model("nukkujalowpoly.obj");
		sleeperlowpoly_model->get_material("cloth")->brightness = 0.05;
	}

	for (int i = 0; i < num; ++i) {
		sleepers.push_back(Sleeper());
		Sleeper *sleeper = &sleepers.back();
		sleeper->world = world;
		sleeper->waking = false;
		sleeper->pos = pos;
		sleeper->obj.move(pos);
		sleeper->obj.set_matrix(dir_matrix(angle + M_PI/2, 0));
		sleeper->obj.set_model(sleeper_model, sleeperlowpoly_model);

		pos += vec3(cos(angle), 0, sin(angle)) * 40;
		angle += turn / num;
	}
}

void hit_walker(Walker *walker)
{
	Matrix matrix = dir_matrix(player_yaw, 0);
	vec3 reverse_pos = reverse_transform(walker->obj.pos() - player.pos(), matrix);
	double volume = 20 / length(walker->obj.pos() - player.pos());
	play_sound(get_sound("hey.ogg"), volume * (2 + frand()),
		   sound_pan(reverse_pos));
}

void wake_up(Sleeper *sleeper)
{
	Matrix matrix = dir_matrix(player_yaw, 0);
	vec3 reverse_pos = reverse_transform(sleeper->obj.pos() - player.pos(), matrix);
	double volume = 20 / length(sleeper->obj.pos() - player.pos());
	play_sound(get_sound("heratys.ogg"),
		   volume * (2 + frand()),
		   sound_pan(reverse_pos));

	sleeper->waking = true;

	wake_counter--;
	if (wake_counter == 0) {
		show_message("Good job!");
		if (!have_weapon[RADIO]) {
			show_message("Good job! Here is radio which you use to request the KAAL!");
			have_weapon[RADIO] = true;
			show_weapons = true;
		}

		state = GAME;
		meters[SUCCESS] =
			std::min(meters[SUCCESS] + 10, 100.0);
		wake_counter = WAKE_COUNT;
	}
}

void flashlight(double probability)
{
	energy -= probability * tech_level[FLASHLIGHT_ENERGY]->value;
	if (energy < 0) return;

	weapon_light.set_color(Color(0.4, 0.4, 0.4));
	weapon_light.move(weap.pos());
	weapon_light.set_brightness(std::min(energy, 1.0));
	weapon_light.set_world(world);

	double dist = flashlight_reach();

	Camera camera;
	camera.pos = weap.pos();
	camera.matrix = weap.matrix();
	prepare_camera(&camera, FLASHLIGHT_FOV, FLASHLIGHT_FOV, dist);

	weap.set_world(NULL);

	std::unordered_set<Object *> hits;
	world->sweep(camera, &hits);

	Object *hit = NULL;
	world->raytrace(weap.pos(), weap.matrix().forward, &dist, NULL, &hit);
	if (hit != NULL) {
		hits.insert(hit);
	}
	weap.set_world(world);

	if (hits.empty()) return;

	if (state == WAKE_MINIGAME) {
		for (Sleeper &sleeper : sleepers) {
			if (hits.count(&sleeper.obj) > 0) {
				if (frand() < probability &&
				    !sleeper.waking) {
					wake_up(&sleeper);
				}
			}
		}
	}

	for (Desk &desk : desks) {
		if (hits.count(&desk.computer) > 0 ||
		    hits.count(&desk.speaker) > 0 ||
		    hits.count(&desk.sit) > 0) {
			if (frand() < probability) {
				stop_desk(&desk, true);
			}
		}
	}

	for (Walker &walker : walkers) {
		if (hits.count(&walker.obj) > 0) {
			if (frand() < probability) {
				hit_walker(&walker);
			}
		}
	}
}

void order_complete()
{
	play_sound(get_sound("cashregister.ogg"));
	meal_counter++;
	if (meal_counter >= NUM_MEALS) {
		show_message("You earned 5 ByteMoneys");

		if (state == BURGER_MINIGAME) {
			if (!have_weapon[MEGAPHONE]) {
				show_message("Here is Megaphone for your hard work");
				have_weapon[MEGAPHONE] = true;
				show_weapons = true;
			}
		} else {
			if (!have_weapon[ELECTRICGUN]) {
				show_message("Since you worked so hard, here is something extra: Electric Gun");
				have_weapon[ELECTRICGUN] = true;
				show_weapons = true;
			}
		}

		money += BURGER_PAY;
		meal_counter = 0;
	}

	for (int i = 0; i < MAX_ORDER_ITEMS; ++i) {
		tray_items[i].set_world(NULL);
	}

	if (state == BURGER_MINIGAME) {
		todo_burger = false;
		order_items[1] = BURGER_FEDORA + rand() % 4;
		order_items[2] = DRINK_COKE + rand() % 4;
		order_items[3] = (frand() < 0.5) ? FRIES : -1;

	} else {
		todo_pizza = false;
		order_items[1] = PIZZA_FISH + rand() % 4;
		order_items[2] = DRINK_COKE + rand() % 4;
		order_items[3] = (frand() < 0.5) ? COFFEE : -1;
	}
}

void drop_meal()
{
	Matrix matrix = dir_matrix(player_yaw, player_pitch);

	weap.set_world(NULL);
	Object *hit = NULL;
	double dist = 40;
	world->raytrace(player.pos() + CAM_POS, matrix.forward,
			&dist, NULL, &hit);
	weap.set_world(world);

	const vec3 order_pos[] = {
		vec3(0, 0, 0),
		vec3(0, 0, 0),
		vec3(2, 2, -3),
		vec3(-2, 1, 3),
	};

	Object *tray = &tray_items[0];

	if (hit == tray) {
		for (int i = 0; i < MAX_ORDER_ITEMS; ++i) {
			Object *item = &tray_items[i];
			if (order_items[i] == meal_item &&
			    item->world() == NULL) {
				item->set_model(get_model(meal_items[meal_item].model));
				item->move(tray->pos() + order_pos[i]);
				item->set_matrix(look_at(vec3(1, 0, 0), vec3(0, 1, 0)));
				item->set_world(world);
				meal_item = -1;
				weap.set_model(get_model(weapons[weapon].model));
				break;
			}
		}
		for (int i = 0; i < MAX_ORDER_ITEMS; ++i) {
			if (order_items[i] < 0) break;

			if (tray_items[i].world() == NULL) {
				return;
			}
		}
		order_complete();

	} else if (hit == NULL && meal_item == TRAY && dist < 30) {
		for (int i = 0; i < MAX_ORDER_ITEMS; ++i) {
			tray_items[i].set_world(NULL);
		}
		tray->set_model(get_model("tray.obj"));
		tray->move(player.pos() + CAM_POS + matrix.forward * dist + vec3(0, 1, 0));
		tray->set_matrix(look_at(vec3(1, 0, 0), vec3(0, 1, 0)));
		tray->set_world(world);
		meal_item = -1;
		weap.set_model(get_model(weapons[weapon].model));
	}
}

void drop_pail()
{
	Matrix matrix = dir_matrix(player_yaw, player_pitch);

	weap.set_world(NULL);
	Object *hit = NULL;
	double dist = 40;
	world->raytrace(player.pos() + CAM_POS, matrix.forward,
			&dist, NULL, &hit);
	weap.set_world(world);

	for (size_t i = 0; i < lengthof(showers); ++i) {
		vec3 d = showers[i] - player.pos();
		if (d < PLAYER_RAD && hold_pail) {
			pail_water = 100;
			play_sound(get_sound(fill_sounds[rand() % lengthof(fill_sounds)]));
			return;
		}
	}
	pail.move(player.pos() + CAM_POS + matrix.forward * (dist - 5));
	pail.set_world(&sauna);
	hold_pail = false;
	weap.set_model(get_model("kauha.obj"));
}

void fire()
{
	if (hold_pail) {
		return;
	}

	if (meal_item >= 0) {
		drop_meal();
		return;
	}

	switch (weapon) {
	case FLASHLIGHT:
		firing = true;
		play_sound(get_sound("flashlighton.ogg"));
		flashlight(0.1);
		break;

	case ELECTRICGUN: {
		if (electricgun_delay > 0 ||
		    energy < tech_level[ELECTRICGUN_ENERGY]->value) {
			play_sound(get_sound("egempty.ogg"));
			break;
		}

		play_sound(get_sound("electricgun.ogg"));
		energy -= tech_level[ELECTRICGUN_ENERGY]->value;

		weap.set_world(NULL);
		Object *hit = NULL;
		double dist = ELECTRIC_DIST;
		world->raytrace(weap.pos(), weap.matrix().forward, &dist, NULL,
				&hit);
		weap.set_world(world);

		electricgun_delay = 1;
		bolt_time = 1;
		weapon_light.set_brightness(1);
		weapon_light.set_color(Color(0.5, 0.6, 1));
		weapon_light.move(weap.pos());
		weapon_light.set_world(world);

		bolt.resize(dist);
		for (int i = 0; i < (int) dist; ++i) {
			vec3 v(frand() - 0.5, frand() - 0.5, -i);
			bolt[i] = weap.pos() + transform(v, weap.matrix());
		}

		if (hit == NULL) break;

		for (Desk &desk : desks) {
			if (&desk.computer == hit ||
			    &desk.sit == hit ||
			    &desk.speaker == hit) {
				kill_desk(&desk);
				break;
			}
		}
		if (state == WAKE_MINIGAME) {
			for (Sleeper &sleeper : sleepers) {
				if (&sleeper.obj == hit &&
				    !sleeper.waking) {
					wake_up(&sleeper);
					break;
				}
			}
		}
		for (Walker &walker : walkers) {
			if (&walker.obj == hit) {
				hit_walker(&walker);
				break;
			}
		}
		break;
	}

	case RADIO:
		if (level_time > DEMO_TIME && level_time < KAAL_TIME &&
		    radio_delay <= 0) {
			play_sound(get_sound("kaalon.ogg"));
			video_time = 1.0 / 24;
			kaal.play("kaal.ogg", false);
			video.play("kaal.ogv");
			radio_delay = 1;
		} else {
			play_sound(get_sound("kaaldeny.ogg"));
		}
		break;

	case MEGAPHONE:
		if (megaphone_delay <= 0) {
			const Sound *yell =
				get_sound(yells[rand() % lengthof(yells)]);
			play_sound(yell, 2 + frand());
			megaphone_delay = 1;
			megaphone_active = 5;
		}
		break;

	case ES:
		play_sound(get_sound("esparinaa.ogg"));
		meters[SLEEP] = 100;
		meters[BLADDER] = std::max(meters[BLADDER] - 10, 0.0);
		have_weapon[ES] = false;
		weapon = FLASHLIGHT;
		weap.set_model(get_model(weapons[weapon].model));
		break;
	}
}

void build_tables()
{
	const Model *cylinder = get_model("cylinder.obj");
	cylinder->get_material("Light.001")->brightness = 0.1;
	cylinder->get_material("Light.002")->brightness = 0.1;
	const Model *table = get_model("table.obj");

	const size_t NUM_SPEAKERS = 3;
	const Model *speakers[NUM_SPEAKERS];
	speakers[0] = get_model("speaker1.obj");
	speakers[1] = get_model("speaker2.obj");
	speakers[2] = get_model("speaker3.obj");

	desks.clear();

	{
		desks.push_back(Desk());
		Desk *desk = &desks.back();
		desk->pos = vec3(30, -194, -265);
		desk->computer.set_model(cylinder);
		desk->computer.move(desk->pos);
		desk->computer.set_matrix(look_at(vec3(0, 0, -1), vec3(0, 1, 0)));
		desk->computer.set_world(&arena);
	}

	/* ground */
	for (int x = 0; x < 3; ++x) {
		for (int y = 0; y < 3; ++y) {
			build_table(vec3(x * 120 - 280, -194,
					 y * 25 - 265), 0, 8);
		}
		for (int y = 0; y < 7; ++y) {
			build_table(vec3(x * 120 - 280, -194,
					 y * 27 - 160), 0, 8);
		}
		for (int y = 0; y < 3; ++y) {
			build_table(vec3(x * 120 - 280, -194,
					 y * 25 + 60), 0, 8);
		}
	}
	/* fourth row */
	for (int y = 0; y < 7; ++y) {
		build_table(vec3(105, -194,
				 y * 27 - 160), 0, 6);
	}

	/* corners */
	for (int x = 0; x < 4; ++x) {
		/* left from screen */
		build_table(vec3(-380 - x*0.7*22, -143 + x * 6,
				 130 + x*0.7*22), M_PI*0.25, 10+x, -M_PI*0.25,
				 false);

		/* right from screen */
		build_table(vec3(-310, -142 + x * 6,
				 -325 - x*22), M_PI, 10+x, -M_PI*0.3,
				 false);

		/* left bottom */
		build_table(vec3(40, -142 + x * 6,
				 165 + x*22), 0, 10+x, -M_PI*0.25,
				 false);

		/* right bottom */
		build_table(vec3(130 + x*0.7*25, -141 + x * 6,
				 -280 - x*0.7*25), M_PI*1.25, 10+x, -M_PI*0.25,
				 false);
	}

	/* stands */
	for (int x = 0; x < 3; ++x) {
		for (int y = 0; y < 4; ++y) {
			build_table(vec3(x * 105 - 260, -143 + y * 6,
					 y * 23 + 165), 0, 7, 0, false);
		}
		for (int y = 0; y < 4; ++y) {
			build_table(vec3(x * 105 - 210, -143 + y * 6,
					 y * -23 - 325), M_PI, 7, 0, false);
		}
	}

	/* back stand */
	for (int y = 0; y < 4; ++y) {
		build_table(vec3(225 + y * 23, -139 + y * 6,
				 -20), -M_PI*0.5, 10, 0, false);
	}
	printf("%zd desks\n", desks.size());

	for (int i = 0; i < 10; ++i) {
		Desk *desk =
			speaker_selection[rand() % speaker_selection.size()];
		desk->speaker.set_model(speakers[rand() % NUM_SPEAKERS]);
		desk->speaker.move(desk->pos + vec3(0, 13, 0));
		desk->speaker.set_matrix(desk->computer.matrix());
		desk->speaker.set_world(&arena);

		desk->table.set_model(table);
		desk->table.move(desk->pos);
		desk->table.set_matrix(desk->computer.matrix());
		desk->table.set_world(&arena);
	}
	speaker_selection.clear();

	for (Desk &desk : desks) {
		start_desk(&desk);
	}

	for (int x = 0; x < 4; ++x) {
		for (int y = 0; y < 3; ++y) {
			if (y == 2 && x < 2) continue;
			build_oldskool(vec3(x * 120 - 430, -10,
					 y * 43 + 505), 0, 4);
		}
	}

	Desk *amiga = &oldskools.front();
	Matrix matrix = dir_matrix(amiga->angle, 0);
	amiga->sit.replace_material("", NULL);
	amiga->sit.set_model(get_model("amigaguy"));
	amiga->sit.move(amiga->pos + matrix.forward * -10 + vec3(0, -11, 0));
	amiga->sit.set_matrix(matrix);
	amiga->sit.set_world(&hallway);

	for (int x = 0; x < 3; ++x) {
		for (int y = 0; y < 3; ++y) {
			build_gamers(vec3(y * 35 - 300 + x * 120, -141, 490),
				     M_PI/2, 8);
		}
	}

	for (int i = 0; i < 4; ++i) {
		for (int x = 0; x < 3; ++x) {
			pick_items.push_back(PickItem());
			PickItem *item = &pick_items.back();
			item->meal_item = BURGER_FEDORA + i;
			item->obj.set_model(get_model(meal_items[BURGER_FEDORA + i].model));
			item->obj.move(vec3(i - 739 + x * 5, -124 + x * 2, i * -8 - 176));
			item->obj.set_matrix(look_at(vec3(2, 1, 0), vec3(0, 1, 0)));
			item->obj.set_world(&hallway);
		}
	}
	pick_items.push_back(PickItem());
	PickItem *item = &pick_items.back();
	item->meal_item = TRAY;
	item->obj.set_model(get_model("tray.obj"));
	item->obj.move(vec3(-740, -129, -135));
	item->obj.set_matrix(look_at(vec3(1, 0, 0), vec3(0, 1, 0)));
	item->obj.set_world(&hallway);

	pick_items.push_back(PickItem());
	item = &pick_items.back();
	item->meal_item = FRIES;
	item->obj.set_model(get_model("friespile.obj"));
	item->obj.move(vec3(-760, -132, -250));
	item->obj.set_matrix(look_at(vec3(1, 0, 0), vec3(0, 1, 0)));
	item->obj.set_world(&hallway);

	const Model *tap = get_model("tap.obj");
	for (int i = 0; i < 4; ++i) {
		pick_items.push_back(PickItem());
		PickItem *item = &pick_items.back();
		item->meal_item = DRINK_COKE + i;
		item->obj.set_model(tap);
		item->obj.move(vec3(-738, -124, -165 + i*3));
		item->obj.set_matrix(look_at(vec3(1, 0, 0), vec3(0, 1, 0)));
		item->obj.set_world(&hallway);
	}

	/* Pizza Box starts here */
	for (int i = 0; i < 2; ++i) {
		pick_items.push_back(PickItem());
		PickItem *item = &pick_items.back();
		item->meal_item = PIZZA_FISH + i;
		item->obj.set_model(get_model(meal_items[PIZZA_FISH + i].pick_model));
		item->obj.move(vec3(i * -17 - 313, -127, i * 3 - 517));
		item->obj.set_matrix(look_at(vec3(0, 1, 5), vec3(0, 1, 0)));
		item->obj.set_world(&hallway);
	}

	for (int i = 0; i < 2; ++i) {
		pick_items.push_back(PickItem());
		PickItem *item = &pick_items.back();
		item->meal_item = PIZZA_SPIDEREYE + i;
		item->obj.set_model(get_model(meal_items[PIZZA_SPIDEREYE + i].pick_model));
		item->obj.move(vec3(i * -17 - 367, -127, i * 3 - 508));
		item->obj.set_matrix(look_at(vec3(0, 1, 5), vec3(0, 1, 0)));
		item->obj.set_world(&hallway);
	}

	pick_items.push_back(PickItem());
	item = &pick_items.back();
	item->meal_item = TRAY;
	item->obj.set_model(get_model("tray.obj"));
	item->obj.move(vec3(-410, -133, -500));
	item->obj.set_matrix(look_at(vec3(1, 0, 0), vec3(0, 1, 0)));
	item->obj.set_world(&hallway);

	for (int i = 0; i < 4; ++i) {
		pick_items.push_back(PickItem());
		PickItem *item = &pick_items.back();
		item->meal_item = DRINK_COKE + i;
		item->obj.set_model(tap);
		item->obj.move(vec3(-345 - i*3, -127, -512));
		item->obj.set_matrix(look_at(vec3(1, 0, 0), vec3(0, 1, 0)));
		item->obj.set_world(&hallway);
	}

	pick_items.push_back(PickItem());
	item = &pick_items.back();
	item->meal_item = COFFEE;
	item->obj.set_model(get_model("kahvi.obj"));
	item->obj.move(vec3(-292, -131, -572));
	item->obj.set_matrix(look_at(vec3(1, 0, 0), vec3(0, 1, 0)));
	item->obj.set_world(&hallway);
}

void add_sleeping_areas()
{
	for (int y = 0; y < 6; ++y) {
		add_sleeping_area(&arena,
				vec3(-300, -10 + y * 25 - (y > 3 ? y * 3 : 0),
				       y * 32 + 266), 0, 10);
	}
	for (int y = 0; y < 6; ++y) {
		add_sleeping_area(&arena,
				vec3(-300, -10 + y * 25 - (y > 3 ? y * 3 : 0),
				       y * -32 - 422), 0, 10);
	}
}

void move_walkers(double dt)
{
	for (Walker &walker : walkers) {
		double dist = length(walker.to - walker.from);
		walker.x += dt * walker.speed;
		if (walker.x > dist) {
			std::vector<int> choices;
			for (size_t i = 0; i < lengthof(tracks); ++i) {
				const Track *track = &tracks[i];
				if (track->a == walker.target) {
					choices.push_back(track->b);
				} else if (track->b == walker.target) {
					choices.push_back(track->a);
				}
			}
			assert(!choices.empty());
			walker.target = choices[rand() % choices.size()];
			walker.x = 0;
			walker.from = walker.to;
			walker.to = trackpoints[walker.target] +
				    vec3(frand() - 0.5, 0, frand() - 0.5) * 20;
			vec3 d = walker.to - walker.from;
			dist = length(d);
			d.y = 0;
			walker.obj.set_matrix(look_at(d, vec3(0, 1, 0)));
		}

		walker.obj.move(walker.from + (walker.to - walker.from)
				* (walker.x / dist));
		walker.obj.set_anim(walker.x * 0.2);
	}
}

void calc_score()
{
	meters[SUCCESS] = std::min(meters[SUCCESS] + 20, 100.0);
	for (const Desk &desk : desks) {
		if (desk.music.playing()) {
			meters[SUCCESS] -= 5;
		}
		if (levels[level].kill_lights && desk.computer.lights_on()) {
			meters[SUCCESS] -= 0.1;
		}
		if (levels[level].kill_lights && desk.snake.lights_on()) {
			meters[SUCCESS] -= 0.5;
		}
	}
	meters[SUCCESS] = std::max(meters[SUCCESS], 0.0);
}

void finish_level()
{
	int stars = motivation / 26;
	if (stars == 0 || level + 1 == lengthof(levels)) {
		end = true;
		return;
	}
	level++;
	upgrades++;
	level_time = LEVEL_TIME;
	char buf[128];
	sprintf(buf, "Level %d/%d: %s compo starts in 10 minutes",
		level + 1, (int) lengthof(levels), levels[level].name);
	show_message(buf);
}

bool in_area(const Plane *planes, size_t count)
{
	for (size_t i = 0; i < count; ++i) {
		if (dot(player.pos(), planes[i].norm) < planes[i].pos) {
			return false;
		}
	}
	return true;
}

void move_desks(double dt)
{
	Matrix matrix = dir_matrix(player_yaw, 0);

	for (Desk &desk : desks) {
		if (kaal.playing() &&
		    frand() < dt / levels[level].kaal_effect) {
			stop_desk(&desk);
		}
		if (megaphone_active > 0 && frand() < dt * 0.2) {
			stop_desk(&desk);
		}
		if (level_time > DEMO_TIME && level_time < KAAL_TIME &&
		    frand() < dt / levels[level].voluntary) {
			stop_desk(&desk);
		}
		if (level_time > KAAL_TIME && frand() < dt * 0.1) {
			start_desk(&desk);
		}
		if (level_time > DEMO_TIME && level_time < KAAL_TIME &&
		    frand() < dt / levels[level].ignorance) {
			start_desk(&desk);
		}
		if (desk.screen.num_frames > 0) {
			desk.screen.frame = ((SDL_GetTicks() + desk.id) / 100) %
					    desk.screen.num_frames;
		}
		if (desk.music.playing()) {
			vec3 reverse_pos =
				reverse_transform(desk.pos - player.pos(),
						  matrix);
			desk.music.set_pan(sound_pan(reverse_pos));
			desk.music.set_volume((world == &arena ? 50 : 10) /
					      length(desk.pos - player.pos()));

			double h = 13 + fabs(sin((SDL_GetTicks() + desk.id) / 100.0)) * 2;
			desk.speaker.move(desk.pos + vec3(0, h, 0));
		}
		desk.sit.set_anim(SDL_GetTicks() / 500.0);
	}
	if (world == &hallway) {
		for (Desk &desk : oldskools) {
			if (desk.screen.num_frames > 0) {
				desk.screen.frame = ((SDL_GetTicks() + desk.id) / 100) %
						    desk.screen.num_frames;
			}
			desk.sit.set_anim(SDL_GetTicks() / 500.0);
		}
		for (Desk &desk : gamers) {
			if (desk.screen.num_frames > 0) {
				desk.screen.frame = ((SDL_GetTicks() + desk.id) / 100) %
						    desk.screen.num_frames;
			}
			desk.sit.set_anim(SDL_GetTicks() / 500.0);
		}
	}
}

void move_smokes(double dt)
{
	for (auto iter = smokes.begin(); iter != smokes.end();) {
		auto this_iter = iter;
		Smoke *smoke = &*iter;
		iter++;

		smoke->time -= dt * 0.5;
		smoke->pos += smoke->vel * dt;
		smoke->vel.y += dt * 5;

		if (smoke->time < 0) {
			smokes.erase(this_iter);
		}
	}
}

void move_sleepers(double dt)
{
	for (Sleeper &sleeper : sleepers) {
		if (level_time > KAAL_TIME && sleeper.obj.world() == NULL &&
		    frand() < dt / 1000) {
			sleeper.waking = false;
			sleeper.obj.set_anim(0);
			sleeper.obj.set_world(sleeper.world);
		}
		if (frand() < dt / 5000) {
			sleeper.waking = true;
		}
		if (sleeper.waking && sleeper.obj.world() != NULL) {
			sleeper.obj.set_anim(sleeper.obj.anim() + dt * 0.5);
			if (sleeper.obj.anim() > 1) {
				sleeper.obj.set_world(NULL);
			}
		}
	}
}

void move_meters(double dt)
{
	meters[BLADDER] = std::max(meters[BLADDER] - dt * 0.1, 0.0);
	meters[SLEEP] = std::max(meters[SLEEP] - dt * 0.05, 0.0);
	meters[HYGIENE] = std::max(meters[HYGIENE] - dt * 0.03, 0.0);
	megaphone_delay = std::max(megaphone_delay -
				  dt / tech_level[MEGAPHONE_SPEED]->value, 0.0);
	electricgun_delay = std::max(electricgun_delay -
			       dt / tech_level[ELECTRICGUN_SPEED]->value, 0.0);
	radio_delay = std::max(radio_delay -
			       dt / tech_level[RADIO_SPEED]->value, 0.0);
	message_time = std::max(message_time - dt, 0.0);
	megaphone_active = std::max(megaphone_active - dt, 0.0);
	heat -= heat * 0.1 * dt;
}

void move_game(double dt)
{
	const char *demo_music[] = {
		"demo.ogg", "kade.ogg",
	};
	const char *demo_video[] = {
		"demo.ogv", "kade.ogv",
	};

	if (firing && weapon == FLASHLIGHT) {
		flashlight(dt * 0.5);
	}

	double prev_level_time = level_time;

	level_time -= dt;
	if (level_time < 0) {
		finish_level();
	}
	if (prev_level_time >= KAAL_TIME && level_time < KAAL_TIME) {
		if (levels[level].kill_lights) {
			show_message("Compo starts in 5 minutes. Time to kill all audio and lights!");
		} else {
			show_message("Compo starts in 5 minutes. Time to kill all audio (not lights).");
		}

	} else if (prev_level_time >= MINUTE && level_time < MINUTE) {
		show_message("1 minute left!");
		play_sound(get_sound("ringring.ogg"));

	} else if (prev_level_time >= DEMO_TIME && level_time < DEMO_TIME) {
		video_time = 1.0 / 24;
		kaal.stop();
		int i = rand() % 2;
		demo.play(demo_music[i], false);
		video.play(demo_video[i]);
		calc_score();
		printf("calc score\n");

	} else if (prev_level_time >= STARS_TIME && level_time < STARS_TIME) {
		int stars = motivation / 26;
		if (stars > 0) {
			play_sound(get_sound("lapi.ogg"));
		} else {
			play_sound(get_sound("fail.ogg"));
		}
	}

	move_meters(dt);

	if (heat > 80) {
		meters[HYGIENE] = std::min(meters[HYGIENE] +
					   dt * (heat - 80), 100.0);
	}

	if (show_todo) {
		todo_slide = std::min(todo_slide + dt * 2, 1.0);
	} else {
		todo_slide = std::max(todo_slide - dt * 2, 0.0);
	}

	if (show_weapons) {
		weapons_slide += dt;
		if (weapons_slide > 1) {
			show_weapons = false;
		}
	} else {
		weapons_slide = std::max(weapons_slide - dt * 0.3, 0.0);
	}

	sauna_anim = std::max(sauna_anim - dt, 0.0);

	if (bolt_time > 0) {
		bolt_time -= dt;
		if (bolt_time > 0) {
			weapon_light.set_brightness(bolt_time);
		} else {
			bolt.clear();
			weapon_light.set_world(NULL);
		}
	}
	if (video.playing()) {
		video_time -= dt;
		while (video.playing() && video_time < 0) {
			video.advance();
			video_time += 1.0 / 24;
		}
	}

	Uint8 *keys = SDL_GetKeyState(NULL);

	vec3 accel(0, 0, 0);
	Matrix matrix = dir_matrix(player_yaw, 0);
	if (player.grounded()) {
		if (keys[SDLK_w]) {
			accel += matrix.forward * PLAYER_THRUST;
		} else if (keys[SDLK_s]) {
			accel -= matrix.forward * PLAYER_THRUST;
		} else {
			accel -= matrix.forward * (dot(player.vel(), matrix.forward) * 10);
		}
		if (keys[SDLK_a]) {
			accel -= matrix.right * PLAYER_THRUST;
		} if (keys[SDLK_d]) {
			accel += matrix.right * PLAYER_THRUST;
		} else {
			accel -= matrix.right * (dot(player.vel(), matrix.right) * 10);
		}
	}
	accel.y -= GRAVITY;

	player.set_matrix(matrix);
	player.trust(accel * dt);

	if (video.playing()) {
		vec3 reverse_pos = reverse_transform(BIGSCREEN - player.pos(), matrix);
		kaal.set_pan(sound_pan(reverse_pos));
		demo.set_pan(sound_pan(reverse_pos));
	}

	move_desks(dt);

	if (world == &sauna && frand() < dt * 10) {
		Smoke smoke;
		smoke.pos = vec3(-530, -190, -430);
		smoke.vel = vec3(frand() - 0.5, 0, frand() - 0.5) * 10;
		smoke.time = 1;
		double c = frand() * 0.3 + 0.4;
		smoke.color = Color(c, c, c, 0.1);
		smokes.push_back(smoke);
	}

	if (world == &hallway) {
		if (frand() < dt * 10) {
			Smoke smoke;
			smoke.pos = vec3(-760, -132, -250);
			smoke.vel = vec3(frand() - 0.5, frand(), frand() - 0.5) * 10;
			smoke.time = 1;
			smoke.color = Color(0, 0, 0, 0.1);
			smokes.push_back(smoke);
		}

		securityguy.set_anim(SDL_GetTicks() / 500.0);
		standers[0].set_anim(SDL_GetTicks() / 500.0);
		standers[1].set_anim(SDL_GetTicks() / 500.0);
		tux.set_anim(SDL_GetTicks() / 500.0);
	}

	move_sleepers(dt);
	move_walkers(dt);
	move_smokes(dt);

	player.advance(dt, PLAYER_RAD);
	if (player.grounded()) {
		player.set_anim(player.anim() +
				dot(matrix.forward, player.vel()) * (dt * 0.2));
	}

	if (state == SAUNA_MINIGAME && sauna_anim > 0) {
		weap.set_matrix(dir_matrix(player_yaw, player_pitch + sauna_anim));
	} else {
		weap.set_matrix(dir_matrix(player_yaw, player_pitch));
	}
	weap.move(player.pos() + CAM_POS +
		    transform(WEAPON_POS, weap.matrix()));

	motivation = 0;
	for (int i = 0; i < NUM_METERS; ++i) {
		motivation += meters[i] / NUM_METERS;
	}

	world = &hallway;
	if (in_area(arena_planes, lengthof(arena_planes))) {
		world = &arena;
	}
	if (in_area(sauna_planes, lengthof(sauna_planes))) {
		world = &sauna;
	}
	player.set_world(world);
	weap.set_world(world);

	if (state == TECH_MENU) return;

	for (size_t i = 0; i < lengthof(bathrooms); ++i) {
		vec3 d = bathrooms[i] - player.pos();
		if (d < PLAYER_RAD) {
			state = BATHROOM;
			return;
		}
	}

	Desk *amiga = &oldskools.front();

	if (in_area(burger_planes, lengthof(burger_planes))) {
		if (state == GAME) {
			meal_counter = 0;
			show_message("Welcome to ASM burger!");
			order_items[0] = TRAY;
			order_items[1] = BURGER_FEDORA + rand() % 4;
			order_items[2] = DRINK_COKE + rand() % 4;
			order_items[3] = (frand() < 0.5) ? FRIES : -1;
		}
		state = BURGER_MINIGAME;

	} else if (in_area(pizza_planes, lengthof(pizza_planes))) {
		if (state == GAME) {
			meal_counter = 0;
			show_message("Welcome to Pizza Box");
			order_items[0] = TRAY;
			order_items[1] = PIZZA_FISH + rand() % 4;
			order_items[2] = DRINK_COKE + rand() % 4;
			order_items[3] = (frand() < 0.5) ? COFFEE : -1;
		}
		state = PIZZA_MINIGAME;

	} else if (in_area(es_planes, lengthof(es_planes))) {
		state = SHOP_ES;

	} else if (in_area(battery_planes, lengthof(battery_planes))) {
		state = SHOP_BATTERY;

	} else if (in_area(sauna_game_planes, lengthof(sauna_game_planes))) {
		if (state == GAME && !hold_pail) {
			weap.set_model(get_model("kauha.obj"));
		}
		state = SAUNA_MINIGAME;

	} else if (player.pos() - securityguy.pos() < 30) {
		if (state != WAKE_MINIGAME) {
			state = SECURITYGUY;
		}

	} else if (player.pos() - amiga->sit.pos() < 30) {
		state = AMIGAGUY;

	} else if (state != WAKE_MINIGAME) {
		if (state == SAUNA_MINIGAME) {
			if (hold_pail) {
				drop_pail();
			}
			if (!hold_pail) {
				weap.set_model(get_model(weapons[weapon].model));
			}
		}
		state = GAME;
	}
}

void upgrade(int tech, int level)
{
	assert(level > 0 && level < TECH_LEVELS);
	const TechInfo *info = &techs[tech];

	tech_level[tech] = &info->levels[level];
	play_sound(get_sound("upgrade.ogg"));
	char buf[128];
	sprintf(buf, "%s was upgraded to %s", weapons[info->weapon].name,
		info->levels[level].descr);
	show_message(buf);
}

void tech_menu_click()
{
	int mx, my;
	SDL_GetMouseState(&mx, &my);

	int y = std::min((my - 200) * (scr_height - 300 - NUM_TECH * 170) /
			 (scr_height - 300), 0) + 200;
	for (int i = 0; i < NUM_TECH; ++i) {
		const TechInfo *info = &techs[i];
		if (!have_weapon[info->weapon]) continue;

		if (my >= y && my < y + 128 &&
		    mx >= scr_width/2 - 210) {
			int level = (mx - (scr_width/2 - 210)) / 150;
			int cur_level = tech_level[i] - &info->levels[0];

			if (level < TECH_LEVELS && level > cur_level &&
			    level <= cur_level + upgrades) {
				upgrades -= level - cur_level;
				upgrade(i, level);
			}
			break;
		}
		y += 170;
	}
}

void burger_click()
{
	Matrix matrix = dir_matrix(player_yaw, player_pitch);

	weap.set_world(NULL);
	Object *hit = NULL;
	double dist = 40;
	world->raytrace(player.pos() + CAM_POS, matrix.forward,
			&dist, NULL, &hit);
	weap.set_world(world);

	for (PickItem &item : pick_items) {
		if (hit == &item.obj) {
			hold_pail = false;
			meal_item = item.meal_item;
			weap.set_model(get_model(meal_items[meal_item].model));
			return;
		}
	}

	if (meal_item >= 0) {
		drop_meal();
	}
}

void heat_stove(double increase)
{
	for (int i = 0; i < (int) increase; ++i) {
		Smoke smoke;
		smoke.pos = vec3(-530, -190, -430);
		smoke.vel = vec3(frand() - 0.5, 0, frand() - 0.5) * 10;
		smoke.time = 1;
		double c = frand() * 0.3 + 0.4;
		smoke.color = Color(c, c, c, 0.1);
		smokes.push_back(smoke);
	}
	heat = std::min(heat + increase, 100.0);
}

void sauna_click()
{
	Matrix matrix = dir_matrix(player_yaw, player_pitch);

	weap.set_world(NULL);
	Object *hit = NULL;
	double dist = 40;
	world->raytrace(player.pos() + CAM_POS, matrix.forward,
			&dist, NULL, &hit);
	weap.set_world(world);

	if (hit == &pail) {
		hold_pail = true;
		pail.set_world(NULL);
		weap.set_model(get_model("kiulu.obj"));

	} else if (hit == &stove) {
		if (hold_pail) {
			if (pail_water > 0) {
				heat_stove(pail_water * 0.5);
				pail_water = 0;
				play_sound(get_sound(stove_sounds[rand() % lengthof(stove_sounds)]));
				sauna_anim = 0.3;
			}

		} else if (player.pos() - pail.pos() > 30) {
			show_message("You have to be next to the pail to reach it!");

		} else if (pail_water >= 10) {
			pail_water = std::max(pail_water - 10, 0.0);
			heat_stove(10);
			play_sound(get_sound(stove_sounds[rand() % lengthof(stove_sounds)]));
			sauna_anim = 0.3;
		}

	} else if (hold_pail) {
		drop_pail();
	}
}

void init_walkers()
{
	walkers.resize(100);
	for (Walker &walker : walkers) {
		walker.id = rand();
		int i = rand() % lengthof(tracks);
		const Track *track = &tracks[i];
		int point;
		if (frand() < 0.5) {
			point = track->a;
			walker.target = track->b;
		} else {
			point = track->b;
			walker.target = track->a;
		}
		walker.speed = 5 + frand() * 10;
		walker.from = trackpoints[point] +
			      vec3(frand() - 0.5, 0, frand() - 0.5) * 20;
		walker.to = trackpoints[walker.target] +
			    vec3(frand() - 0.5, 0, frand() - 0.5) * 20;
		vec3 d = walker.to - walker.from;
		double dist = length(d);
		walker.x = frand() * dist;
		walker.obj.move(walker.from + d * (walker.x / dist));
		d.y = 0;
		int sex = frand() < 0.8;
		walker.obj.set_model(persons[sex],
				     personslowpoly[sex]);
		walker.obj.set_matrix(look_at(d, vec3(0, 1, 0)));
		if (i < 14) {
			walker.obj.set_world(&arena);
		} else {
			walker.obj.set_world(&hallway);
		}

		walker.clothes.texture = NULL;
		walker.clothes.num_frames = 0;
		walker.clothes.brightness = 0;
		walker.clothes.color = Color(frand(), frand(), frand());
		walker.obj.replace_material("Material", &walker.clothes);
	}
}

void click()
{
	switch (state) {
	case GAME:
	case WAKE_MINIGAME:
		fire();
		break;

	case TECH_MENU:
		tech_menu_click();
		break;

	case BURGER_MINIGAME:
		burger_click();
		break;

	case PIZZA_MINIGAME:
		burger_click();
		break;

	case SAUNA_MINIGAME:
		sauna_click();
		break;

	case BATHROOM:
		meters[BLADDER] = 100;
		play_sound(get_sound("wc.ogg"));
		break;

	case SECURITYGUY:
		play_sound(get_sound("gibberish.ogg"));
		todo_wake = false;
		state = WAKE_MINIGAME;
		show_message("Could you help us to wake up people sleeping in unsafe places?");
		break;

	case AMIGAGUY:
		play_sound(get_sound("gibberish.ogg"));
		state = TECH_MENU;
		SDL_ShowCursor(1);
		break;

	case SHOP_ES:
		if (!have_weapon[ES] && money >= ES_COST) {
			play_sound(get_sound("cashregister.ogg"));
			have_weapon[ES] = true;
			show_weapons = 1;
			money -= ES_COST;
		}
		break;

	case SHOP_BATTERY:
		if (energy < 100 && money >= BATTERY_COST) {
			play_sound(get_sound("cashregister.ogg"));
			energy = std::min(energy + 20, 100.0);
			money -= BATTERY_COST;
		}
		break;

	default:
		assert(0);
	}
}

void handle_event(const SDL_Event &e)
{
	switch (e.type) {
		case SDL_MOUSEBUTTONDOWN:
			if (paused) break;

			if (e.button.button == 3) {
				zooming = true;
				break;
			} else if (e.button.button == 1) {
				click();
			}
			break;

		case SDL_MOUSEBUTTONUP:
			if (e.button.button == 1) {
				if (firing && weapon == FLASHLIGHT) {
					play_sound(get_sound("flashlightoff.ogg"));
					flashlight(0.2);
					weapon_light.set_world(NULL);
				}
				firing = false;
			} else if (e.button.button == 3) {
				zooming = false;
			}
			break;

		case SDL_KEYDOWN:
			if (cheat < (int) lengthof(cheat_seq) &&
			    e.key.keysym.sym == cheat_seq[cheat]) {
				cheat++;
				if (cheat == lengthof(cheat_seq)) {
					cheat = 0;
					meters[BLADDER] = 100;
					meters[SUCCESS] = 100;
					meters[SLEEP] = 100;
					meters[HYGIENE] = 100;
					energy = 100;
					money += 10;
					memset(have_weapon, -1,
						sizeof(have_weapon));
					show_message("You cheater!");
				}
			}

			switch (e.key.keysym.sym) {
			case SDLK_y:
				if (paused) {
					end = true;
				}
				break;
			case SDLK_p:
				if (paused) break;
				if (level_time > KAAL_TIME) {
					move_meters(level_time - KAAL_TIME);
					level_time = KAAL_TIME;
				} else if (level_time > MINUTE) {
					move_meters(level_time - MINUTE);
					level_time = MINUTE;
				} else if (level_time > DEMO_TIME) {
					move_meters(level_time - DEMO_TIME);
					level_time = DEMO_TIME;
				}
				break;
			case SDLK_n:
				if (paused) {
					paused = false;
					SDL_PauseAudio(0);
					SDL_ShowCursor(0);
				}
				break;
			case SDLK_1:
			case SDLK_2:
			case SDLK_3:
			case SDLK_4:
			case SDLK_5:
				if (paused) break;
				if (have_weapon[e.key.keysym.sym - SDLK_1] && !hold_pail) {
					weapon = e.key.keysym.sym - SDLK_1;
					meal_item = -1;
					weap.set_model(get_model(weapons[weapon].model));
				}
				show_weapons = 1;
				break;
			case SDLK_ESCAPE:
				if (paused) {
					paused = false;
				} else if (state == WAKE_MINIGAME || state == TECH_MENU) {
					state = GAME;
				} else {
					paused = true;
				}
				SDL_PauseAudio(paused);
				SDL_ShowCursor(paused || state == TECH_MENU);
				break;
			case SDLK_SPACE:
				if (!paused && player.grounded()) {
					play_sound(get_sound(
						jumpsounds[rand() % lengthof(jumpsounds)]));
					player.trust(vec3(0, 100, 0));
				}
				break;
			case SDLK_l:
				if (!paused) {
					show_todo = !show_todo;
				}
				break;
			case SDLK_t:
				thirdperson = !thirdperson;
				if (thirdperson) {
					player.set_model(persons[0], NULL,
							vec3(0, -PLAYER_RAD, 0));
				} else {
					player.set_model(NULL);
				}
				break;
			default:
				break;
			}
			break;
		}
}

}

void game()
{
	bool mouse_ready = false;

	if (persons[0] == NULL) {
		arena.load("areena.obj");
		hallway.load("areenaulko.obj");
		sauna.load("sauna.obj");
		hallway.set_ambient(Color(0.5, 0.5, 0.5));
		sauna.set_ambient(Color(0.3, 0.3, 0.3));
		arena.get_material("Light")->brightness = 0.2;
		arena.get_material("Light.002")->brightness = 0.2;
		arena.get_material("Light.003")->brightness = 0.2;
		arena.get_material("Light.004")->brightness = 0.2;
		arena.get_material("Light.005")->brightness = 0.2;
		arena.get_material("Glass")->color.a = 0.3;
		arena.get_material("Material")->brightness = 0.3;
		arena.get_material("Material")->color = Color(1, 1, 1);
		hallway.get_material("Glass")->color.a = 0.3;
		hallway.get_material("Window")->color.a = 0.3;
		hallway.get_material("Window")->brightness = 0.6;
		hallway.get_material("Material.003")->brightness = 0.4;
		hallway.get_material("Material")->brightness = 0.3;
		hallway.get_material("asmburgersign")->brightness = 0.3;
		hallway.get_material("jills")->brightness = 0.3;
		hallway.get_material("tv")->brightness = 0.3;
		hallway.get_material("screen")->brightness = 0.3;
		hallway.get_material("Light")->brightness = 0.4;
		hallway.get_material("Light.001")->brightness = 0.2;
		hallway.get_material("Light.002")->brightness = 0.2;
		sauna.get_material("Light")->brightness = 0.3;
		arena.register_lights();
		hallway.register_lights();
		sauna.register_lights();
		get_model("radio.obj")->get_material("SScreen")->brightness = 0.1;
		get_model("kauha.obj")->get_material("water")->color.a = 0.3;
		get_model("kiulu.obj")->get_material("water")->color.a = 0.3;
		weapon_pics[FLASHLIGHT] = get_texture("flashlight.png");
		weapon_pics[ELECTRICGUN] = get_texture("electricgun.png");
		weapon_pics[RADIO] = get_texture("radio.png");
		weapon_pics[MEGAPHONE] = get_texture("megaphone.png");
		weapon_pics[ES] = get_texture("esicon.png");
		persons[0] = get_model("person");
		persons[1] = get_model("person2");
		personslowpoly[0] = get_model("personlowpoly");
		personslowpoly[1] = get_model("person2lowpoly");
		check_gl_errors();
	}

	world = &hallway;
	memset(have_weapon, 0, sizeof(have_weapon));
	for (int i = 0; i < NUM_TECH; ++i) {
		tech_level[i] = &techs[i].levels[0];
	}
	player_yaw = M_PI/2;
	player_pitch = 0;
	level = 0;
	weapon = FLASHLIGHT;
	have_weapon[FLASHLIGHT] = true;
	energy = 100;
	meters[BLADDER] = 100;
	meters[SLEEP] = 100;
	meters[SUCCESS] = 100;
	meters[HYGIENE] = 100;
	bolt_time = 0;
	video_time = 0;
	message_time = 0;
	paused = false;
	firing = false;
	zooming = false;
	level_time = LEVEL_TIME;
	megaphone_delay = 0;
	megaphone_active = 0;
	radio_delay = 0;
	electricgun_delay = 0;
	money = 5;
	thirdperson = false;
	meal_item = -1;
	state = GAME;
	upgrades = 1;
	show_todo = true;
	todo_slide = 0;
	meal_counter = 0;
	wake_counter = WAKE_COUNT;
	todo_burger = true;
	todo_pizza = true;
	todo_wake = true;
	show_weapons = true;
	hold_pail = false;
	weapons_slide = 0;
	pail_water = 50;
	heat = 0;
	sauna_anim = 0;

	SDL_ShowCursor(0);

	char buf[128];
	sprintf(buf, "Level %d/%d: %s compo starts in 10 minutes",
		level + 1, (int) lengthof(levels), levels[level].name);
	show_message(buf);

	build_tables();
	add_sleeping_areas();
	init_walkers();

	static const Sound *flashlightoff;
	static const Model *battery;
	if (flashlightoff == NULL) {
		flashlightoff = get_sound("flashlightoff.ogg");
		battery = get_model("battery.obj");
		battery->get_material("red")->brightness = 0.1;
	}

	securityguy.move(vec3(-100, -21, -730));
	securityguy.set_model(get_model("securityguy"));
	securityguy.set_world(&hallway);
	securityguy.set_matrix(look_at(vec3(1, 0, 0), vec3(0, 1, 0)));

	standers[0].move(vec3(-670, -151, -350));
	standers[0].set_model(get_model("personstand"));
	standers[0].set_world(&hallway);
	standers[0].set_matrix(look_at(vec3(-1, 0, 0), vec3(0, 1, 0)));

	standers[1].move(vec3(230, -151, -630));
	standers[1].set_model(get_model("personstand"));
	standers[1].set_world(&hallway);
	standers[1].set_matrix(look_at(vec3(0, 0, 1), vec3(0, 1, 0)));

	tux.move(vec3(35, -19, 580));
	tux.set_model(get_model("tux"));
	tux.set_world(&hallway);
	tux.set_matrix(look_at(vec3(0, 0, 1), vec3(0, 1, 0)));

	pail.move(vec3(-515, -192, -390));
	pail.set_model(get_model("kiulu.obj"));
	pail.set_world(&sauna);
	pail.set_matrix(look_at(vec3(0, 0, 1), vec3(0, 1, 0)));

	stove.move(vec3(-530, -200, -430));
	stove.set_model(get_model("kiuas.obj"));
	stove.set_world(&sauna);
	stove.set_matrix(look_at(vec3(0, 0, 1), vec3(0, 1, 0)));

	player.move(vec3(-1070, -137, -230));
	player.set_model(NULL);
	player.set_world(world);
	player_clothes.texture = NULL;
	player_clothes.num_frames = 0;
	player_clothes.brightness = 0;
	player_clothes.color = Color(frand(), frand(), frand());
	player.replace_material("Material", &player_clothes);

	weap.set_model(get_model(weapons[weapon].model));
	weap.set_world(world);

	Music crowd;
	crowd.play("crowd.ogg", true);

	check_gl_errors();
	Uint32 tics = SDL_GetTicks();
	Uint32 begin_tics = tics;

	end = false;
	while (!end) {
		SDL_Event e;
		while (get_event(&e)) {
			handle_event(e);
		}

		int x, y;
		SDL_GetMouseState(&x, &y);

		if (!paused && state != TECH_MENU && (SDL_GetAppState() & SDL_APPINPUTFOCUS)) {
			if (mouse_ready) {
				player_yaw += (x - scr_width / 2) * 0.005;
				player_pitch += (y - scr_height / 2) * 0.005;
				if (player_pitch < -M_PI/3) player_pitch = -M_PI/3;
				if (player_pitch > M_PI/3) player_pitch = M_PI/3;
			}
			mouse_ready = true;
			SDL_WarpMouse(scr_width / 2, scr_height / 2);
		} else {
			mouse_ready = false;
		}

		double dt = (SDL_GetTicks() - tics) / 1000.0;
		dt = std::max(std::min(dt, 0.1), 0.0);
		tics = SDL_GetTicks();

		if (!paused) {
			move_game(dt);
		}

		render();
		finish_draw();
	}
	video.stop();
	kill_sounds();
	arena.remove_all_objects();
	hallway.remove_all_objects();
	sauna.remove_all_objects();
	desks.clear();
	walkers.clear();
	gamers.clear();
	sleepers.clear();
	items.clear();
	pick_items.clear();
	oldskools.clear();
	bolt.clear();
	smokes.clear();
	SDL_PauseAudio(0);
	ping((SDL_GetTicks() - begin_tics) / 1000, level);
}

