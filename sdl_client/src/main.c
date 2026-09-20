#include <SDL3/SDL.h>

#include <getopt.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "render.h"
#include "serial.h"
#include "terminal.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define BELL_RATE 44100
#define BELL_MS 80
#define BELL_HZ 880.0f
#define BELL_SAMPLES ((BELL_RATE * BELL_MS) / 1000)

static SDL_AudioStream *bell_stream;
static float bell_buf[BELL_SAMPLES];

static void bell_init(void) {
	if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
		return;
	SDL_AudioSpec spec = {SDL_AUDIO_F32, 1, BELL_RATE};
	bell_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
											&spec, NULL, NULL);
	if (!bell_stream)
		return;
	for (int i = 0; i < BELL_SAMPLES; i++)
		bell_buf[i] = 0.25f * (float)sin(2.0 * M_PI * BELL_HZ * i / BELL_RATE) *
					  (1.0f - (float)i / BELL_SAMPLES);
	SDL_ResumeAudioStreamDevice(bell_stream);
}

static void bell_play(void) {
	if (bell_stream)
		SDL_PutAudioStreamData(bell_stream, bell_buf, (int)sizeof(bell_buf));
}

static void bell_quit(void) {
	if (bell_stream)
		SDL_DestroyAudioStream(bell_stream);
}

static const uint32_t cp437_tab[128] = {
	0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 0x00EA,
	0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5, 0x00C9, 0x00E6,
	0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, 0x00FF, 0x00D6, 0x00DC,
	0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192, 0x00E1, 0x00ED, 0x00F3, 0x00FA,
	0x00F1, 0x00D1, 0x00AA, 0x00BA, 0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC,
	0x00A1, 0x00AB, 0x00BB, 0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561,
	0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B,
	0x2510, 0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F,
	0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567, 0x2568,
	0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 0x256A, 0x2518,
	0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580, 0x03B1, 0x00DF, 0x0393,
	0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4, 0x03A6, 0x0398, 0x03A9, 0x03B4,
	0x221E, 0x03C6, 0x03B5, 0x2229, 0x2261, 0x00B1, 0x2265, 0x2264, 0x2320,
	0x2321, 0x00F7, 0x2248, 0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2,
	0x25A0, 0x00A0,
};

static int encode_cp(uint32_t cp) {
	if (cp < 0x80)
		return (int)cp;
	for (int i = 0; i < 128; i++)
		if (cp437_tab[i] == cp)
			return 0x80 + i;
	return '?';
}

static void cursor_seq(Terminal *t, char c) {
	if (terminal_vt52(t)) {
		char seq[2] = {27, c};
		terminal_send(t, seq, 2);
	} else {
		char seq[3] = {27, '[', c};
		terminal_send(t, seq, 3);
	}
}

static void csi_tilde(Terminal *t, int n) {
	char buf[12];
	int len = snprintf(buf, sizeof(buf), "\033[%d~", n);
	terminal_send(t, buf, (size_t)len);
}

static void paste_clipboard(Terminal *t) {
	char *text = SDL_GetClipboardText();
	if (!text)
		return;
	for (const char *p = text; *p; p++) {
		uint8_t b = (uint8_t)*p;
		if (b == '\r')
			continue;
		if (b == '\n')
			b = '\r';
		terminal_send(t, &b, 1);
	}
	SDL_free(text);
}

static void paste_primary(Terminal *t) {
	char *text = SDL_GetPrimarySelectionText();
	if (!text)
		return;
	for (const char *p = text; *p; p++) {
		uint8_t b = (uint8_t)*p;
		if (b == '\r')
			continue;
		if (b == '\n')
			b = '\r';
		terminal_send(t, &b, 1);
	}
	SDL_free(text);
}

static void handle_key(Terminal *t, const SDL_KeyboardEvent *ev) {
	SDL_Keycode key = ev->key;
	SDL_Keymod mod = ev->mod;

	if ((mod & SDL_KMOD_CTRL) && (mod & SDL_KMOD_SHIFT) && key == SDLK_V) {
		paste_clipboard(t);
		return;
	}
	if ((mod & SDL_KMOD_SHIFT) && key == SDLK_INSERT) {
		paste_clipboard(t);
		return;
	}

	if (mod & SDL_KMOD_CTRL) {
		if (key >= SDLK_A && key <= SDLK_Z) {
			uint8_t c = (uint8_t)(key - SDLK_A + 1);
			terminal_send(t, &c, 1);
			return;
		}
		uint8_t c = 0;
		bool handled = true;
		switch (key) {
		case SDLK_SPACE:
			c = 0;
			break;
		case SDLK_2:
			c = 0;
			break;
		case SDLK_3:
			c = 27;
			break;
		case SDLK_4:
			c = 28;
			break;
		case SDLK_5:
			c = 29;
			break;
		case SDLK_6:
			c = 30;
			break;
		case SDLK_7:
			c = 31;
			break;
		case SDLK_8:
			c = 127;
			break;
		case SDLK_LEFTBRACKET:
			c = 27;
			break;
		case SDLK_BACKSLASH:
			c = 28;
			break;
		case SDLK_RIGHTBRACKET:
			c = 29;
			break;
		case SDLK_MINUS:
			c = 31;
			break;
		case SDLK_SLASH:
			c = 31;
			break;
		default:
			handled = false;
			break;
		}
		if (handled) {
			terminal_send(t, &c, 1);
			return;
		}
	}

	switch (key) {
	case SDLK_UP:
		cursor_seq(t, 'A');
		break;
	case SDLK_DOWN:
		cursor_seq(t, 'B');
		break;
	case SDLK_RIGHT:
		cursor_seq(t, 'C');
		break;
	case SDLK_LEFT:
		cursor_seq(t, 'D');
		break;
	case SDLK_F1:
	case SDLK_F2:
	case SDLK_F3:
	case SDLK_F4: {
		char buf[3] = {
			27, terminal_vt52(t) ? (char)('P' + (key - SDLK_F1)) : 'O', 0};
		if (terminal_vt52(t)) {
			terminal_send(t, buf, 2);
		} else {
			buf[2] = (char)('P' + (key - SDLK_F1));
			terminal_send(t, buf, 3);
		}
		break;
	}
	case SDLK_F5:
		csi_tilde(t, 15);
		break;
	case SDLK_F6:
		csi_tilde(t, 17);
		break;
	case SDLK_F7:
		csi_tilde(t, 18);
		break;
	case SDLK_F8:
		csi_tilde(t, 19);
		break;
	case SDLK_F9:
		csi_tilde(t, 20);
		break;
	case SDLK_F10:
		csi_tilde(t, 21);
		break;
	case SDLK_F11:
		csi_tilde(t, 23);
		break;
	case SDLK_F12:
		csi_tilde(t, 24);
		break;
	case SDLK_HOME:
		cursor_seq(t, 'H');
		break;
	case SDLK_END:
		csi_tilde(t, 4);
		break;
	case SDLK_INSERT:
		csi_tilde(t, 2);
		break;
	case SDLK_DELETE:
		csi_tilde(t, 3);
		break;
	case SDLK_PAGEUP:
		csi_tilde(t, 5);
		break;
	case SDLK_PAGEDOWN:
		csi_tilde(t, 6);
		break;
	case SDLK_RETURN:
	case SDLK_KP_ENTER: {
		uint8_t c = 0x0d;
		terminal_send(t, &c, 1);
		break;
	}
	case SDLK_BACKSPACE: {
		uint8_t c = 0x08;
		terminal_send(t, &c, 1);
		break;
	}
	case SDLK_TAB: {
		uint8_t c = 0x09;
		terminal_send(t, &c, 1);
		break;
	}
	case SDLK_ESCAPE: {
		uint8_t c = 0x1b;
		terminal_send(t, &c, 1);
		break;
	}
	default:
		break;
	}
}

static size_t utf8_decode(const char *s, uint32_t *cp) {
	unsigned char c = (unsigned char)s[0];
	if (c < 0x80) {
		*cp = c;
		return 1;
	}
	if ((c & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
		*cp = ((c & 0x1F) << 6) | (s[1] & 0x3F);
		return 2;
	}
	if ((c & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
		*cp = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
		return 3;
	}
	if ((c & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 &&
		(s[3] & 0xC0) == 0x80) {
		*cp = ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) |
			  ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
		return 4;
	}
	*cp = '?';
	return 1;
}

static void usage(const char *prog) {
	fprintf(
		stderr,
		"usage: %s [options]\n"
		"  -d, --device <path>  serial device (default /dev/ttyACM0)\n"
		"  -b, --baud <rate>    baud rate (default 115200)\n"
		"  -s, --scale <n>      initial window scale (default 2)\n"
		"  -c, --cols <n>       terminal columns (default %d)\n"
		"  -r, --rows <n>       terminal rows (default %d)\n"
		"      --vt52           start in VT52 mode\n"
		"      --no-bell        mute the audible bell (visual flash remains)\n"
		"  -h, --help           show this help\n",
		prog, TERM_DEFAULT_COLS, TERM_DEFAULT_ROWS);
}

int main(int argc, char **argv) {
	const char *device = "/dev/ttyACM0";
	int baud = 115200;
	int scale = 2;
	int cols = TERM_DEFAULT_COLS;
	int rows = TERM_DEFAULT_ROWS;
	bool start_vt52 = false;
	bool no_bell = false;

	static const struct option long_opts[] = {
		{"device", required_argument, NULL, 'd'},
		{"baud", required_argument, NULL, 'b'},
		{"scale", required_argument, NULL, 's'},
		{"cols", required_argument, NULL, 'c'},
		{"rows", required_argument, NULL, 'r'},
		{"vt52", no_argument, NULL, 1000},
		{"no-bell", no_argument, NULL, 1001},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0},
	};

	int opt;
	while ((opt = getopt_long(argc, argv, "d:b:s:c:r:h", long_opts, NULL)) !=
		   -1) {
		switch (opt) {
		case 'd':
			device = optarg;
			break;
		case 'b':
			baud = atoi(optarg);
			break;
		case 's':
			scale = atoi(optarg);
			break;
		case 'c':
			cols = atoi(optarg);
			break;
		case 'r':
			rows = atoi(optarg);
			break;
		case 1000:
			start_vt52 = true;
			break;
		case 1001:
			no_bell = true;
			break;
		case 'h':
			usage(argv[0]);
			return 0;
		default:
			usage(argv[0]);
			return 1;
		}
	}

	if (scale < 1)
		scale = 1;
	if (cols < 20)
		cols = TERM_DEFAULT_COLS;
	if (rows < 4)
		rows = TERM_DEFAULT_ROWS;

	Terminal *term = terminal_create(cols, rows);
	if (!term) {
		fprintf(stderr, "out of memory\n");
		return 1;
	}
	if (start_vt52)
		terminal_feed(term, (const uint8_t *)"\033[?2l", 5);

	int serial_fd = serial_open(device, baud);
	if (serial_fd < 0)
		fprintf(stderr, "waiting for %s...\n", device);

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}
	if (!no_bell)
		bell_init();

	int win_w = cols * RENDER_CELL_W * scale;
	int win_h = rows * RENDER_CELL_H * scale;
	SDL_Window *window = SDL_CreateWindow("z80neo terminal", win_w, win_h, 0);
	if (!window) {
		fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		return 1;
	}
	SDL_SetWindowMinimumSize(window, cols * RENDER_CELL_W,
							 rows * RENDER_CELL_H);
	SDL_StartTextInput(window);

	SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
	if (!renderer) {
		fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
		return 1;
	}
	SDL_SetRenderLogicalPresentation(renderer, cols * RENDER_CELL_W,
									 rows * RENDER_CELL_H,
									 SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);

	int tex_w = cols * RENDER_CELL_W;
	int tex_h = rows * RENDER_CELL_H;
	SDL_Texture *texture =
		SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
						  SDL_TEXTUREACCESS_STREAMING, tex_w, tex_h);
	if (!texture) {
		fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
		return 1;
	}
	SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
	SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);

	uint32_t *pixels = malloc((size_t)tex_w * tex_h * sizeof(uint32_t));
	if (!pixels) {
		fprintf(stderr, "out of memory\n");
		return 1;
	}

	bool running = true;
	bool dirty = true;
	bool blink_on = true;
	bool flash_active = false;
	Uint64 last_blink = SDL_GetTicks();
	Uint64 last_reconnect = SDL_GetTicks();
	Uint64 last_render = 0;
	Uint64 flash_until = 0;
	unsigned bell_seen = 0;

	while (running) {
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			switch (ev.type) {
			case SDL_EVENT_QUIT:
				running = false;
				break;
			case SDL_EVENT_KEY_DOWN:
				handle_key(term, &ev.key);
				break;
			case SDL_EVENT_TEXT_INPUT: {
				if (SDL_GetModState() & SDL_KMOD_CTRL)
					break;
				const char *p = ev.text.text;
				while (*p) {
					uint32_t cp;
					size_t n = utf8_decode(p, &cp);
					int b = encode_cp(cp);
					uint8_t byte = (uint8_t)b;
					terminal_send(term, &byte, 1);
					p += n;
				}
				break;
			}
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
				if (ev.button.button == SDL_BUTTON_MIDDLE)
					paste_primary(term);
				break;
			default:
				break;
			}
		}

		if (serial_fd >= 0) {
			uint8_t buf[1024];
			int n = serial_read(serial_fd, buf, sizeof(buf));
			if (n < 0) {
				serial_close(serial_fd);
				serial_fd = -1;
				fprintf(stderr, "serial disconnected\n");
			} else if (n > 0) {
				terminal_feed(term, buf, (size_t)n);
				dirty = true;
			}
		} else if (SDL_GetTicks() - last_reconnect > 1000) {
			serial_fd = serial_open(device, baud);
			last_reconnect = SDL_GetTicks();
		}

		size_t out_len;
		const uint8_t *out = terminal_output(term, &out_len);
		if (out_len > 0) {
			if (serial_fd >= 0)
				serial_write(serial_fd, out, out_len);
			terminal_output_clear(term);
		}

		Uint64 now = SDL_GetTicks();

		unsigned bells = terminal_bell_count(term);
		if (bells != bell_seen) {
			bell_seen = bells;
			if (!no_bell)
				bell_play();
			flash_until = now + 120;
		}

		if (now - last_blink >= 500) {
			blink_on = !blink_on;
			last_blink = now;
			dirty = true;
		}

		bool flash = now < flash_until;
		if (flash != flash_active) {
			flash_active = flash;
			dirty = true;
		}

		if (dirty && now - last_render >= 16) {
			render_frame(term, pixels, blink_on, flash);
			SDL_UpdateTexture(texture, NULL, pixels,
							  tex_w * (int)sizeof(uint32_t));
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			SDL_RenderClear(renderer);
			SDL_RenderTexture(renderer, texture, NULL, NULL);
			SDL_RenderPresent(renderer);
			last_render = now;
			dirty = false;
		}

		SDL_Delay(2);
	}

	free(pixels);
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	bell_quit();
	SDL_Quit();
	serial_close(serial_fd);
	terminal_destroy(term);
	return 0;
}
