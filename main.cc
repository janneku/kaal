/*
 * KAAL
 *
 * Copyright 2013 Janne Kulmala <janne.t.kulmala@iki.fi>,
 * Antti Rajamäki <amikaze@gmail.com>
 *
 * Program code and resources are licensed with GNU LGPL 2.1. See
 * lgpl-2.1.txt file.
 */
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif
#include <fenv.h>
#include <time.h>
#include "gfx.h"
#include "effects.h"
#include "sfx.h"
#include "game.h"
#include "system.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern "C" {
extern const char *version;
}

namespace {

/* Menu items */
enum {
	PLAY,
	HELP,
	SETTINGS,
	CREDITS,
	QUIT,
	MENU_ITEMS
};

/* Settings */
enum {
	QUALITY,
	ANTIALIASING,
	INVERT_MOUSE,
	SETTINGS_CLOSE,
	SETTINGS_MENU_ITEMS
};

/* Menu state */
enum {
	MENU,
	SHOW_HELP,
	SHOW_SETTINGS,
	SHOW_CREDITS,
};

const char *menu_items[] = {
	"Play",
	"Help",
	"Settings",
	"Credits",
	"Quit",
};

struct TextureInfo {
	const char *fname;
	bool mipmap;
};

const TextureInfo textures[] = {
	{"asmburgeropposite.png", true},
	{"asmburger.png", true},
	{"blank.png", true},
	{"broken.png", true},
	{"bsod.png", true},
	{"ChilledNewbie.png", true},
	{"crosshair.png", true},
	{"cppposter.png", true},
	{"cs.png", false},
	{"demokyky.png", true},
	{"design.png", true},
	{"Eelis.png", true},
	{"eroa.png", true},
	{"fr.png", false},
	{"grass.png", true},
	{"hypercube.png", true},
	{"Ironchains.png", true},
	{"jills.png", true},
	{"laatta.png", true},
	{"logo.png", false},
	{"masuiso.png", true},
	{"NKfi.PNG", true},
	{"noise.png", true},
	{"numerot.png", true},
	{"Nuotio.png", true},
	{"off.png", false},
	{"paatti.png", true},
	{"Partyilija.png", true},
	{"PinQ.png", true},
	{"seko.png", false},
	{"SISKO.png", true},
	{"sky.png", true},
	{"snue.png", true},
	{"sorvi.png", false},
	{"wow.png", false},
	{"ylatekstuuri.png", true},
	{"alatekstuuri.png", true},
	{"ym.png", true},
	{"flashlight.png", false},
	{"radio.png", false},
	{"megaphone.png", false},
	{"electricgun.png", false},
	{"map1.png", false},
	{"map2.png", false},
	{"bar.png", false},
	{"box.png", false},
	{"flatbox.png", false},
	{"juomat.png", true},
	{"order.png", false},
	{"todo.png", false},
	{"infodesk.png", true},
	{"roof.png", true},
	{"burgers.png", false},
	{"es.png", true},
	{"wc.png", true},
	{"pizzabox.png", true},
	{"grid.png", true},
	{"batterytex.png", true},
	{"esicon.png", false},
	{"second.png", true},
	{"phenomena.png", true},
	{"security.png", true},
	{"wakeup.png", false},
	{"doorposter.png", true},
	{"amiga.png", true},
	{"steak.png", true},
	{"pizza.png", true},
	{"Brown_Mushroom.png", true},
	{"Spider_Eye.png", true},
	{"pizzaorder.png", false},
	{"pizzat.png", false},
	{"star.png", false},
	{"lux.png", true},
	{"presentation.png", true},
	{"wc2.png", true},
	{"boards.png", true},
	{"kiuas.png", true},
};

const char *sounds[] = {
	"crowd.ogg",
	"curse1.ogg",
	"curse2.ogg",
	"electricgun.ogg",
	"flashlightoff.ogg",
	"flashlighton.ogg",
	"kaal1.ogg",
	"kaal2.ogg",
	"kaal3.ogg",
	"kaal4.ogg",
	"kaaldeny.ogg",
	"kaalon.ogg",
	"screen.ogg",
	"ringring.ogg",
	"wc.ogg",
	"cashregister.ogg",
	"upgrade.ogg",
	"esparinaa.ogg",
	"heratys.ogg",
	"hyppy1.ogg",
	"hyppy2.ogg",
	"egempty.ogg",
	"lapi.ogg",
	"fail.ogg",
	"gibberish.ogg",
	"hey.ogg",
	"vedenotto1.ogg",
	"vedenotto2.ogg",
	"kiuas1.ogg",
	"kiuas2.ogg",
};

struct ModelInfo {
	const char *fname;
	double scale;
	int first_frame;
	int num_frames;
	bool noise;
};

struct ModelInfo models[] = {
	{"battery.obj", 0.2, 1, 1, true},
	{"chasis1.obj", 1.0, 1, 1, true},
	{"cylinder.obj", 1.0, 1, 1, true},
	{"electricgun.obj", 0.7, 1, 1, false},
	{"flashlight.obj", 0.3, 1, 1, false},
	{"laptop.obj", 1.0, 1, 1, false},
	{"laptop2.obj", 1.0, 1, 1, false},
	{"logo.obj", 5.0, 1, 1, true},
	{"megaphone.obj", 0.2, 1, 1, false},
	{"monitor.obj", 1.0, 1, 1, false},
	{"monitor2.obj", 1.0, 1, 1, false},
	{"pc.obj", 1.0, 1, 1, false},
	{"pc2.obj", 1.0, 1, 1, false},
	{"person", 0.3, 2, 7, false},
	{"person2", 0.3, 2, 7, false},
	{"personlowpoly", 0.3, 2, 7, false},
	{"person2lowpoly", 0.3, 2, 7, false},
	{"personsit", 0.3, 1, 2, true},
	{"personsit2", 0.3, 1, 2, true},
	{"personsitlowpoly.obj", 0.3, 1, 1, false},
	{"personsit2lowpoly.obj", 0.3, 1, 1, false},
	{"radio.obj", 0.2, 1, 1, false},
	{"skybox.obj", RENDER_DIST * 0.9, 1, 1, true},
	{"sleeper", 0.4, 1, 2, false},
	{"nukkujalowpoly.obj", 0.4, 1, 1, false},
	{"snake.obj", 1.0, 1, 1, false},
	{"snake2.obj", 1.0, 1, 1, false},
	{"speaker1.obj", 1.0, 1, 1, true},
	{"speaker2.obj", 1.0, 1, 1, true},
	{"speaker3.obj", 1.0, 1, 1, true},
	{"table.obj", 1.0, 1, 1, true},
	{"oldskool1.obj", 1.0, 1, 1, false},
	{"oldskool2.obj", 1.0, 1, 1, false},
	{"oldskool3.obj", 1.0, 1, 1, false},
	{"burger1.obj", 1.5, 1, 1, true},
	{"burger2.obj", 1.5, 1, 1, true},
	{"burger3.obj", 1.5, 1, 1, true},
	{"burger4.obj", 1.5, 1, 1, true},
	{"tray.obj", 1.0, 1, 1, false},
	{"cup.obj", 1.0, 1, 1, false},
	{"fries.obj", 0.7, 1, 1, true},
	{"friespile.obj", 1, 1, 1, true},
	{"tap.obj", 1, 1, 1, false},
	{"es.obj", 1, 1, 1, false},
	{"securityguy", 0.3, 2, 7, false},
	{"personstand", 0.3, 2, 6, false},
	{"amigaguy", 0.3, 1, 6, true},
	{"fishpizza.obj", 1, 1, 1, true},
	{"shroompizza.obj", 1, 1, 1, true},
	{"spidereyepizza.obj", 1, 1, 1, true},
	{"pepperonipizza.obj", 1, 1, 1, true},
	{"slice.obj", 1, 1, 1, true},
	{"coffee.obj", 1, 1, 1, false},
	{"kahvi.obj", 1, 1, 1, true},
	{"tux", 15, 1, 5, false},
	{"kauha.obj", 1, 1, 1, false},
	{"kiulu.obj", 1, 1, 1, false},
	{"kiuas.obj", 1, 1, 1, false},
};

double menu_time;
int active_item;
double slide[MENU_ITEMS];
int state;

void render_menu_scene(bool light)
{
	static const Model *logo;
	static const Model *snakes[2];

	if (logo == NULL) {
		logo = get_model("logo.obj");
		snakes[0] = get_model("snake.obj");
		snakes[1] = get_model("snake2.obj");
		snakes[0]->get_material("Light")->brightness = 0.3;
		snakes[1]->get_material("Light")->brightness = 0.3;
	}

	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45, (double) scr_width / scr_height, 1, 1500);
	gluLookAt(50, 0, 60, 10, 10, 0, 0, 1, 0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	GLState gl;
	gl.enable(GL_CULL_FACE);
	gl.enable(GL_DEPTH_TEST);

	int flags = light ? RENDER_BLOOM : 0;

	bool lighton[2] = {false, false};
	lighton[0] = (sin(menu_time * 10) > -0.8);
	lighton[1] = (sin(menu_time * 3) > -0.8);

	const vec3 lightpos[] = {
		vec3(20, 0, 20),
		vec3(-30, 20, 10),
	};

	if (light) {
		begin_rendering(0, flags);
	} else {
		size_t n = 0;
		for (int i = 0; i < 2; ++i) {
			if (lighton[i]) {
				n += snakes[i]->get_lights().size();
			}
		}
		begin_rendering(n, flags);
		n = 0;
		for (int i = 0; i < 2; ++i) {
			if (lighton[i]) {
				for (const Model::Light &l : snakes[i]->get_lights()) {
					set_light(n, l.pos + lightpos[i],
						  l.mat->color,
						  l.mat->brightness);
					n++;
				}
			}
		}
	}
	for (int i = 0; i < 2; ++i) {
		glLoadIdentity();
		glTranslatef(lightpos[i].x, lightpos[i].y, lightpos[i].z);
		snakes[i]->render(flags | (lighton[i] ? RENDER_LIGHTS_ON : 0));
	}
	glLoadIdentity();
	logo->render(flags);
	end_rendering();
}

bool preload_step(int n)
{
	static Uint32 last_tics = 0;

	if (SDL_GetTicks() - last_tics < 10) {
		return true;
	}
	last_tics = SDL_GetTicks();

	SDL_Event e;
	while (get_event(&e)) {
		switch (e.type) {
		case SDL_KEYDOWN:
			switch (e.key.keysym.sym) {
			case SDLK_ESCAPE:
				return false;
			default:
				break;
			}
			break;
		}
	}

	glClear(GL_COLOR_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, scr_width, scr_height, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	char buf[64];
	sprintf(buf, "Loading: %d %%", n * 100 / (int) (
			lengthof(textures) + lengthof(sounds) +
			lengthof(models)));
	glColor3f(1, 1, 1);
	glTranslatef(scr_width/2 - small_font.text_width(buf)/2,
	     scr_height/2, 0);
	small_font.draw_text(buf);

	finish_draw();
	return true;
}

bool preload()
{
	int n = 0;
	for (size_t i = 0; i < lengthof(textures); ++i) {
		if (!preload_step(n)) {
			return false;
		}
		load_texture(textures[i].fname, textures[i].mipmap, false);
		n++;
	}
	for (size_t i = 0; i < lengthof(sounds); ++i) {
		if (!preload_step(n)) {
			return false;
		}
		load_sound(sounds[i]);
		n++;
	}
	for (size_t i = 0; i < lengthof(models); ++i) {
		if (!preload_step(n)) {
			return false;
		}
		load_model(models[i].fname, models[i].scale,
			   models[i].first_frame, models[i].num_frames,
			   models[i].noise);
		n++;
	}

	load_texture("smoke.png", true, true);
	return true;
}

bool menu()
{
	Music music;
	music.play("lobby.ogg", true);

	state = MENU;
	menu_time = 0;
	Uint32 tics = SDL_GetTicks();

	SDL_ShowCursor(1);
	memset(slide, 0, sizeof(slide));

	while (1) {
		SDL_Event e;
		while (get_event(&e)) {
			switch (e.type) {
			case SDL_MOUSEBUTTONDOWN:
				if (menu_time < 4) {
					menu_time = 4;

				} else if (state == MENU) {
					switch (active_item) {
					case PLAY:
						kill_sounds();
						return true;

					case HELP:
						state = SHOW_HELP;
						break;

					case SETTINGS:
						memset(slide, 0, sizeof(slide));
						active_item = -1;
						state = SHOW_SETTINGS;
						break;

					case CREDITS:
						state = SHOW_CREDITS;
						break;

					case QUIT:
						kill_sounds();
						return false;
					}

				} else if (state == SHOW_SETTINGS) {
					switch (active_item) {
					case QUALITY:
						quality = (quality + 1) % 3;
						break;

					case ANTIALIASING:
						antialiasing = !antialiasing;
						break;

					case INVERT_MOUSE:
						invert_mouse = !invert_mouse;
						break;

					case SETTINGS_CLOSE:
						memset(slide, 0, sizeof(slide));
						active_item = -1;
						write_settings();
						state = MENU;
						break;
					}

				} else {
					state = MENU;
				}
				break;

			case SDL_KEYDOWN:
				switch (e.key.keysym.sym) {
				case SDLK_ESCAPE:
					if (state != MENU) {
						state = MENU;
					} else if (menu_time < 4) {
						menu_time = 4;
					} else {
						kill_sounds();
						return false;
					}
					break;
				default:
					break;
				}
				break;
			}
		}

		double dt = (SDL_GetTicks() - tics) / 1000.0;
		dt = std::max(std::min(dt, 0.1), 0.0);
		tics = SDL_GetTicks();

		menu_time += dt;

		int mx, my;
		SDL_GetMouseState(&mx, &my);

		if (menu_time > 4) {
			if (quality >= 1) {
				begin_bloom();
				render_menu_scene(true);
				end_bloom();
			}
			render_menu_scene(false);
			if (quality >= 1) {
				draw_bloom();
			}
		} else {
			glClear(GL_COLOR_BUFFER_BIT);
		}

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, scr_width, scr_height, 0, -1, 1);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		if (menu_time < 4) {
			const char text[] = "Pizzalaatikko presents";
			glColor4f(1, 1, 1, sin(menu_time * M_PI / 4));
			glTranslatef(scr_width/2 - large_font.text_width(text)/2,
				     scr_height/2, 0);
			large_font.draw_text(text);

		} else if (state == MENU) {
			active_item = -1;
			for (int i = 0; i < MENU_ITEMS; ++i) {
				int x = scr_width - 300;
				int y = scr_height - 500 + i * 100;
				if (mx >= x && my >= y && my < y + 50) {
					slide[i] = std::min(slide[i] + dt * 3, 1.0);
					active_item = i;
				} else {
					slide[i] = std::max(slide[i] - dt * 3, 0.0);
				}
			}

			for (int i = 0; i < MENU_ITEMS; ++i) {
				glLoadIdentity();
				glTranslatef(scr_width - 250 - slide[i] * 20,
					     scr_height - 500 + i * 100 + 40,
					     0);
				if (active_item == i) {
					glColor3f(1, 0.8, 0.5);
				} else {
					glColor3f(1, 1, 1);
				}
				large_font.draw_text(menu_items[i]);
				glTranslatef(0, 100, 0);
			}

		} else if (state == SHOW_HELP) {
			const char text[] =
				"You are visiting Assembly 2013 summer party\n"
				"in Hartwall Arena, Helsinki.\n"
				"There is a big problem: Visitors have brought\n"
				"very bright lights and large speakers and are\n"
				"reluctant to turn those off.\n\n"
				"Your mission is to help the organizers to get\n"
				"all lights and audio switched off before a compo\n"
				"starts so that all visitors can enjoy the presented\n"
				"demos without distractions.\n\n"
				"To improve your success, Pizza Box and ASM Burger\n"
				"have open positions where you can work to get\n"
				"better weapons and money to buy ES. You can\n"
				"also help the security crew to to wake up\n"
				"visitors sleeping in forbidden areas.\n\n"
				"Keys:\n"
				"ESC     End game or the current minigame\n"
				"L          Show or hide the todo list\n"
				"T          Third person view\n"
				"P          Fast forward to the next compo\n"
				"WASD Move around\n"
				"1 - 5    Change weapon\n"
				"Click   Fire the weapon or perform an action\n";
			glColor4f(1, 1, 1, 1);
			glTranslatef(scr_width/2 - small_font.text_width(text)/2,
				     scr_height/2 - 250, 0);
			small_font.draw_text(text);

		} else if (state == SHOW_SETTINGS) {
			active_item = -1;
			for (int i = 0; i < SETTINGS_MENU_ITEMS; ++i) {
				int x = scr_width - 400;
				int y = scr_height - 300 + i * 50;
				if (mx >= x && my >= y && my < y + 50) {
					slide[i] = std::min(slide[i] + dt * 3, 1.0);
					active_item = i;
				} else {
					slide[i] = std::max(slide[i] - dt * 3, 0.0);
				}
			}

			char text[256];
			for (int i = 0; i < SETTINGS_MENU_ITEMS; ++i) {
				glLoadIdentity();
				glTranslatef(scr_width - 350 - slide[i] * 20,
					     scr_height - 300 + i * 50 + 20,
					     0);
				if (active_item == i) {
					glColor3f(1, 0.8, 0.5);
				} else {
					glColor3f(1, 1, 1);
				}
				if (i == ANTIALIASING) {
					sprintf(text, "Antialiasing (needs restart): %s",
						antialiasing ? "yes" : "no");
				} else if (i == INVERT_MOUSE) {
					sprintf(text, "Invert mouse: %s",
						invert_mouse ? "yes" : "no");
				} else if (i == QUALITY) {
					sprintf(text, "Quality level: %d",
						quality + 1);
				} else if (i == SETTINGS_CLOSE) {
					strcpy(text, "Back to main menu");
				}
				small_font.draw_text(text);
				glTranslatef(0, 100, 0);
			}

		} else if (state == SHOW_CREDITS) {
			char text[256];
			sprintf(text, "KAAL version %s\nby Pizzalaatikko\n\n"
				"For Assembly summer 2013\n\n"
				"Credits:\n"
				"Janne \"Japeq\" Kulmala - Code, graphics\n"
				"Antti \"Amikaze\" Rajamaki - Models, graphics, sounds",
				version);
			glColor4f(1, 1, 1, 1);
			glTranslatef(scr_width/2 - small_font.text_width(text)/2,
				     scr_height/2, 0);
			small_font.draw_text(text);
		}

		finish_draw();
		SDL_Delay(10);
	}
}

}

int main(int argc, char **argv)
try {
	printf("KAAL version %s\n", version);
	if (SDL_Init(SDL_INIT_VIDEO)) {
		throw std::runtime_error(std::string("Can not init SDL: ") +
					 SDL_GetError());
	}

	open_pack("kaal.dat");

	srand(time(NULL));

	char buf[64];
	sprintf(buf, "KAAL (%s)", version);
	SDL_WM_SetCaption(buf, buf);

	bool windowed = false;
	if (argc > 1 && std::string(argv[1]) == "-window") {
#ifndef _WIN32
		feenableexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW);
#endif
		windowed = true;
	}
	load_settings();
	init_system(windowed);

	glewInit();

	if (!GLEW_VERSION_1_5) {
		throw std::runtime_error("Time to update your graphics card - OpenGL 1.5 required!");
	}
	if (quality < 0) {
		quality = 0;
		if (GLEW_VERSION_2_0) {
			quality = 1;
		}
		if (GLEW_VERSION_3_1) {
			quality = 2;
		}
	}
	printf("Quality level: %d\n", quality);

	init_sound();

	glDepthFunc(GL_LESS);
	check_gl_errors();

	if (preload()) {
		while (menu()) {
			game();
		}
	}
	SDL_Quit();
	return 0;

} catch (const std::runtime_error &e) {
	SDL_Quit();
#ifdef _WIN32
	MessageBox(NULL, e.what(), "Error", MB_OK|MB_ICONERROR);
#else
	fprintf(stderr, "ERROR: %s\n", e.what());
#endif
	return 1;
}
