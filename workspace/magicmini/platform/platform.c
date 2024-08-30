// magicmini
#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include <msettings.h>

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"

#include "scaler.h"

///////////////////////////////

#define RAW_UP		544
#define RAW_DOWN	545
#define RAW_LEFT	546
#define RAW_RIGHT	547

#define RAW_A		308
#define RAW_B		305
#define RAW_X		307
#define RAW_Y		304

#define RAW_START	315
#define RAW_SELECT	314
#define RAW_MENU	704

#define RAW_L1		310
#define RAW_L2		313
#define RAW_L3		317
#define RAW_R1		311
#define RAW_R2		312
#define RAW_R3		318

#define RAW_PLUS	115
#define RAW_MINUS	114
#define RAW_POWER	116

#define RAW_LSY		1
#define RAW_LSX		0
#define RAW_RSY		2
#define RAW_RSX		5

#define RAW_MENU1	RAW_L3
#define RAW_MENU2	RAW_R3

#define INPUT_COUNT 3
static int inputs[INPUT_COUNT];

void PLAT_initInput(void) {
	inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // power
	inputs[1] = open("/dev/input/event2", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // gamepad
	inputs[2] = open("/dev/input/event3", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // volume
}
void PLAT_quitInput(void) {
	for (int i=0; i<INPUT_COUNT; i++) {
		close(inputs[i]);
	}
}

// from <linux/input.h> which has BTN_ constants that conflict with platform.h
struct input_event {
	struct timeval time;
	__u16 type;
	__u16 code;
	__s32 value;
};
#define EV_KEY			0x01
#define EV_ABS			0x03

static void updateButton(int btn, int id, int pressed, uint32_t tick) {
	if (btn==BTN_NONE) return;

	if (!pressed) {
		pad.is_pressed		&= ~btn; // unset
		pad.just_repeated	&= ~btn; // unset
		pad.just_released	|= btn; // set
	}
	else if ((pad.is_pressed & btn)==BTN_NONE) {
		pad.just_pressed	|= btn; // set
		pad.just_repeated	|= btn; // set
		pad.is_pressed		|= btn; // set
		pad.repeat_at[id]	= tick + PAD_REPEAT_DELAY;
	}
}

void PLAT_pollInput(void) {
	// reset transient state
	pad.just_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_repeated = BTN_NONE;

	uint32_t tick = SDL_GetTicks();
	for (int i=0; i<BTN_ID_COUNT; i++) {
		int btn = 1 << i;
		if ((pad.is_pressed & btn) && (tick>=pad.repeat_at[i])) {
			pad.just_repeated |= btn; // set
			pad.repeat_at[i] += PAD_REPEAT_INTERVAL;
		}
	}
	
	// gross hack for out of order hat events
	// doesn't work because we can catch it's
	// bad state update but it's own internal
	// state is broken so it never sends the
	// real release event
	// static time_t x_sec = 0;
	// static time_t y_sec = 0;
	// static suseconds_t x_usec = 0;
	// static suseconds_t y_usec = 0;
	
	// the actual poll
	int input;
	static struct input_event event;
	for (int i=0; i<INPUT_COUNT; i++) {
		input = inputs[i];
		while (read(input, &event, sizeof(event))==sizeof(event)) {
			if (event.type!=EV_KEY && event.type!=EV_ABS) continue;

			int btn = BTN_NONE;
			int pressed = 0; // 0=up,1=down
			int id = -1;
			int type = event.type;
			int code = event.code;
			int value = event.value;
			
			// TODO: tmp, hardcoded, missing some buttons
			if (type==EV_KEY) {
				if (value>1) continue; // ignore repeats
			
				pressed = value;
				// LOG_info("key event: %i (%i)\n", code,pressed);
					 if (code==RAW_UP) 		{ btn = BTN_DPAD_UP; 	id = BTN_ID_DPAD_UP; }
	 			else if (code==RAW_DOWN)	{ btn = BTN_DPAD_DOWN; 	id = BTN_ID_DPAD_DOWN; }
				else if (code==RAW_LEFT)	{ btn = BTN_DPAD_LEFT; 	id = BTN_ID_DPAD_LEFT; }
				else if (code==RAW_RIGHT)	{ btn = BTN_DPAD_RIGHT; id = BTN_ID_DPAD_RIGHT; }
				else if (code==RAW_A)		{ btn = BTN_A; 			id = BTN_ID_A; }
				else if (code==RAW_B)		{ btn = BTN_B; 			id = BTN_ID_B; }
				else if (code==RAW_X)		{ btn = BTN_X; 			id = BTN_ID_X; }
				else if (code==RAW_Y)		{ btn = BTN_Y; 			id = BTN_ID_Y; }
				else if (code==RAW_START)	{ btn = BTN_START; 		id = BTN_ID_START; }
				else if (code==RAW_SELECT)	{ btn = BTN_SELECT; 	id = BTN_ID_SELECT; }
				else if (code==RAW_MENU)	{ btn = BTN_MENU; 		id = BTN_ID_MENU; }
				else if (code==RAW_L1)		{ btn = BTN_L1; 		id = BTN_ID_L1; }
				else if (code==RAW_L2)		{ btn = BTN_L2; 		id = BTN_ID_L2; }
				else if (code==RAW_L3)		{ btn = BTN_L3; 		id = BTN_ID_L3; }
				else if (code==RAW_R1)		{ btn = BTN_R1; 		id = BTN_ID_R1; }
				else if (code==RAW_R2)		{ btn = BTN_R2; 		id = BTN_ID_R2; }
				else if (code==RAW_R3)		{ btn = BTN_R3; 		id = BTN_ID_R3; }
				else if (code==RAW_PLUS)	{ btn = BTN_PLUS; 		id = BTN_ID_PLUS; }
				else if (code==RAW_MINUS)	{ btn = BTN_MINUS; 		id = BTN_ID_MINUS; }
				else if (code==RAW_POWER)	{ btn = BTN_POWER; 		id = BTN_ID_POWER; }
			}
			else if (type==EV_ABS) {
				LOG_info("abs event: %i (%i)\n", code,value);
					 if (code==RAW_LSX) { pad.laxis.x = -value; PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, pad.laxis.x, tick+PAD_REPEAT_DELAY); }
				else if (code==RAW_LSY) { pad.laxis.y = -value; PAD_setAnalog(BTN_ID_ANALOG_UP,   BTN_ID_ANALOG_DOWN,  pad.laxis.y, tick+PAD_REPEAT_DELAY); }
				else if (code==RAW_RSX) pad.raxis.x = value;
				else if (code==RAW_RSY) pad.raxis.y = value;
			}
			
			if (btn==BTN_NONE) continue;
		
			updateButton(btn, id, pressed, tick);
			if (btn==BTN_L3||btn==BTN_R3) updateButton(BTN_MENU, BTN_ID_MENU, pressed, tick);
		}
	}
}

int PLAT_shouldWake(void) {
	
	int input;
	static struct input_event event;
	for (int i=0; i<INPUT_COUNT; i++) {
		input = inputs[i];
		while (read(input, &event, sizeof(event))==sizeof(event)) {
			if (event.type==EV_KEY && event.code==RAW_POWER && event.value==0) {
				return 1;
			}
		}
	}
	return 0;
}

///////////////////////////////

// based on rg35xxplus

static struct VID_Context {
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* texture;
	SDL_Texture* target;
	SDL_Texture* effect;

	SDL_Surface* buffer;
	SDL_Surface* screen;
	
	GFX_Renderer* blit; // yeesh
	
	int width;
	int height;
	int pitch;
	int sharpness;
} vid;


static int device_width;
static int device_height;
static int device_pitch;
static int rotate = 0;
SDL_Surface* PLAT_initVideo(void) {
	// LOG_info("PLAT_initVideo\n");
	
	SDL_InitSubSystem(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	
	// SDL_version compiled;
	// SDL_version linked;
	// SDL_VERSION(&compiled);
	// SDL_GetVersion(&linked);
	// LOG_info("Compiled SDL version %d.%d.%d ...\n", compiled.major, compiled.minor, compiled.patch);
	// LOG_info("Linked SDL version %d.%d.%d.\n", linked.major, linked.minor, linked.patch);
	//
	// int num_displays = SDL_GetNumVideoDisplays();
	// LOG_info("SDL_GetNumVideoDisplays(): %i\n", num_displays);
	//
	// LOG_info("Available video drivers:\n");
	// for (int i=0; i<SDL_GetNumVideoDrivers(); i++) {
	// 	LOG_info("- %s\n", SDL_GetVideoDriver(i));
	// }
	// LOG_info("Current video driver: %s\n", SDL_GetCurrentVideoDriver());
	//
	// LOG_info("Available render drivers:\n");
	// for (int i=0; i<SDL_GetNumRenderDrivers(); i++) {
	// 	SDL_RendererInfo info;
	// 	SDL_GetRenderDriverInfo(i,&info);
	// 	LOG_info("- %s\n", info.name);
	// }
	//
	// LOG_info("Available display modes:\n");
	// SDL_DisplayMode mode;
	// for (int i=0; i<SDL_GetNumDisplayModes(0); i++) {
	// 	SDL_GetDisplayMode(0, i, &mode);
	// 	LOG_info("- %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));
	// }
	// SDL_GetCurrentDisplayMode(0, &mode);
	// LOG_info("Current display mode: %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));

	// LOG_info("Available audio drivers:\n");
	// for (int i=0; i<SDL_GetNumAudioDrivers(); i++) {
	// 	LOG_info("- %s\n", SDL_GetAudioDriver(i));
	// }
	// LOG_info("Current audio driver: %s\n", SDL_GetCurrentAudioDriver()); // NOTE: hadn't been selected yet so will always be NULL!

	// SDL_SetHint(SDL_HINT_RENDER_VSYNC,"0"); // ignored?

	int w = FIXED_WIDTH;
	int h = FIXED_HEIGHT;
	int p = FIXED_PITCH;
	vid.window   = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w,h, SDL_WINDOW_SHOWN);
	LOG_info("window size: %ix%i\n", w,h);
	
	SDL_DisplayMode mode;
	SDL_GetCurrentDisplayMode(0, &mode);
	LOG_info("Current display mode: %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));
	if (mode.h>mode.w) rotate = 3;
	vid.renderer = SDL_CreateRenderer(vid.window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
	// SDL_RenderSetLogicalSize(vid.renderer, w,h); // TODO: wrong, but without and with the below it's even wrong-er
	
	// int renderer_width,renderer_height;
	// SDL_GetRendererOutputSize(vid.renderer, &renderer_width, &renderer_height);
	// LOG_info("output size: %ix%i\n", renderer_width, renderer_height);
	// if (renderer_width!=w) { // I think this can only be hdmi
	// 	float x_scale = (float)renderer_width / w;
	// 	float y_scale = (float)renderer_height / h;
	// 	SDL_SetWindowSize(vid.window, w / x_scale, h / y_scale);
	//
	// 	SDL_GetRendererOutputSize(vid.renderer, &renderer_width, &renderer_height);
	// 	LOG_info("adjusted size: %ix%i\n", renderer_width, renderer_height);
	// 	x_scale = (float)renderer_width / w;
	// 	y_scale = (float)renderer_height / h;
	// 	SDL_RenderSetScale(vid.renderer, x_scale,y_scale);
	//
	// 	// for some reason we need to clear and present
	// 	// after setting the window size or we'll miss
	// 	// the first frame
	// 	SDL_RenderClear(vid.renderer);
	// 	SDL_RenderPresent(vid.renderer);
	// }
	
	// SDL_RendererInfo info;
	// SDL_GetRendererInfo(vid.renderer, &info);
	// LOG_info("Current render driver: %s\n", info.name);
	
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"1"); // linear
	vid.texture = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w,h);
	vid.target	= NULL; // only needed for non-native sizes
	
	// TODO: doesn't work here
	// SDL_SetTextureScaleMode(vid.texture, SDL_ScaleModeLinear); // we always start at device size so use linear for better upscaling over hdmi
	
	// SDL_ScaleMode scale_mode;
	// SDL_GetTextureScaleMode(vid.texture, &scale_mode);
	// LOG_info("texture scale mode: %i\n", scale_mode);
	
	// int format;
	// int access_;
	// SDL_QueryTexture(vid.texture, &format, &access_, NULL,NULL);
	// LOG_info("texture format: %s (streaming: %i)\n", SDL_GetPixelFormatName(format), access_==SDL_TEXTUREACCESS_STREAMING);
	
	vid.buffer	= SDL_CreateRGBSurfaceFrom(NULL, w,h, FIXED_DEPTH, p, RGBA_MASK_565);
	vid.screen	= SDL_CreateRGBSurface(SDL_SWSURFACE, w,h, FIXED_DEPTH, RGBA_MASK_565);
	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;
	
	device_width	= w;
	device_height	= h;
	device_pitch	= p;
	
	vid.sharpness = SHARPNESS_SOFT;
	
	return vid.screen;
}

static void clearVideo(void) {
	SDL_FillRect(vid.screen, NULL, 0);
	for (int i=0; i<3; i++) {
		SDL_RenderClear(vid.renderer);
		SDL_RenderPresent(vid.renderer);
	}
}

void PLAT_quitVideo(void) {
	// clearVideo();

	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.buffer);
	if (vid.target) SDL_DestroyTexture(vid.target);
	if (vid.effect) SDL_DestroyTexture(vid.effect);
	SDL_DestroyTexture(vid.texture);
	SDL_DestroyRenderer(vid.renderer);
	SDL_DestroyWindow(vid.window);
	

	// system("cat /dev/zero > /dev/fb0");
	SDL_Quit();
}

void PLAT_clearVideo(SDL_Surface* screen) {
	SDL_FillRect(screen, NULL, 0); // TODO: revisit
}
void PLAT_clearAll(void) {
	PLAT_clearVideo(vid.screen); // TODO: revist
	SDL_RenderClear(vid.renderer);
}

void PLAT_setVsync(int vsync) {
	// buh
}

static int hard_scale = 4; // TODO: base src size, eg. 160x144 can be 4

static void resizeVideo(int w, int h, int p) {
	if (w==vid.width && h==vid.height && p==vid.pitch) return;
	
	// TODO: minarch disables crisp (and nn upscale before linear downscale) when native
	
	if (w>=device_width && h>=device_height) hard_scale = 1;
	else if (h>=160) hard_scale = 2; // limits gba and up to 2x (seems sufficient)
	else hard_scale = 4;

	LOG_info("resizeVideo(%i,%i,%i) hard_scale: %i crisp: %i\n",w,h,p, hard_scale,vid.sharpness==SHARPNESS_CRISP);

	SDL_FreeSurface(vid.buffer);
	SDL_DestroyTexture(vid.texture);
	if (vid.target) SDL_DestroyTexture(vid.target);
	
	SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, vid.sharpness==SHARPNESS_SOFT?"1":"0", SDL_HINT_OVERRIDE);
	vid.texture = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w,h);
	
	if (vid.sharpness==SHARPNESS_CRISP) {
		SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "1", SDL_HINT_OVERRIDE);
		vid.target = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_TARGET, w * hard_scale,h * hard_scale);
	}
	else {
		vid.target = NULL;
	}

	vid.buffer	= SDL_CreateRGBSurfaceFrom(NULL, w,h, FIXED_DEPTH, p, RGBA_MASK_565);

	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int p) {
	resizeVideo(w,h,p);
	return vid.screen;
}

void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	// buh
}
void PLAT_setNearestNeighbor(int enabled) {
	// buh
}
void PLAT_setSharpness(int sharpness) {
	if (vid.sharpness==sharpness) return;
	int p = vid.pitch;
	vid.pitch = 0;
	vid.sharpness = sharpness;
	resizeVideo(vid.width,vid.height,p);
}
static int effect_scale = 1;
static int effect_type = EFFECT_NONE;
static int next_scale = 1;
static int next_effect = EFFECT_NONE;
static void updateEffect(void) {
	if (next_scale==effect_scale && next_effect==effect_type) return; // unchanged
	
	effect_scale = next_scale;
	effect_type = next_effect;
	
	if (vid.effect) SDL_DestroyTexture(vid.effect);
	if (effect_type==EFFECT_NONE) return;
	
	char* effect_path;
	int opacity = 128; // 1 - 1/2 = 50%
	if (effect_type==EFFECT_LINE) {
		if (effect_scale<3) {
			effect_path = RES_PATH "/line-2.png";
		}
		else if (effect_scale<4) {
			effect_path = RES_PATH "/line-3.png";
		}
		else if (effect_scale<5) {
			effect_path = RES_PATH "/line-4.png";
		}
		else if (effect_scale<6) {
			effect_path = RES_PATH "/line-5.png";
		}
		else if (effect_scale<8) {
			effect_path = RES_PATH "/line-6.png";
		}
		else {
			effect_path = RES_PATH "/line-8.png";
		}
	}
	else if (effect_type==EFFECT_GRID) {
		if (effect_scale<3) {
			effect_path = RES_PATH "/grid-2.png";
			opacity = 64; // 1 - 3/4 = 25%
		}
		else if (effect_scale<4) {
			effect_path = RES_PATH "/grid-3.png";
			opacity = 112; // 1 - 5/9 = ~44%
		}
		else if (effect_scale<5) {
			effect_path = RES_PATH "/grid-4.png";
			opacity = 144; // 1 - 7/16 = ~56%
		}
		else if (effect_scale<6) {
			effect_path = RES_PATH "/grid-5.png";
			opacity = 160; // 1 - 9/25 = ~64%
		}
		else if (effect_scale<8) {
			effect_path = RES_PATH "/grid-6.png";
			opacity = 112; // 1 - 5/9 = ~44%
		}
		else if (effect_scale<11) {
			effect_path = RES_PATH "/grid-8.png";
			opacity = 144; // 1 - 7/16 = ~56%
		}
		else {
			effect_path = RES_PATH "/grid-11.png";
			opacity = 136; // 1 - 57/121 = ~52%
		}
	}
	LOG_info("load effect: %s (opacity: %i)\n", effect_path, opacity);
	SDL_Surface* tmp = IMG_Load(effect_path);
	if (tmp) {
		vid.effect = SDL_CreateTextureFromSurface(vid.renderer, tmp);
		SDL_SetTextureAlphaMod(vid.effect, opacity);
		SDL_FreeSurface(tmp);
	}
}
void PLAT_setEffect(int effect) {
	next_effect = effect;
}
void PLAT_vsync(int remaining) {
	if (remaining>0) SDL_Delay(remaining);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	next_scale = renderer->scale;
	return scale1x1_c16;
}

void PLAT_blitRenderer(GFX_Renderer* renderer) {
	vid.blit = renderer;
	SDL_RenderClear(vid.renderer);
	resizeVideo(vid.blit->true_w,vid.blit->true_h,vid.blit->src_p);
}

// TODO: should we be using true_* instead of src_* below?
void PLAT_flip(SDL_Surface* IGNORED, int ignored) {
	if (!vid.blit) {
		resizeVideo(device_width,device_height,FIXED_PITCH); // !!!???
		SDL_UpdateTexture(vid.texture,NULL,vid.screen->pixels,vid.screen->pitch);
		// SDL_LockTexture(vid.texture,NULL,&vid.buffer->pixels,&vid.buffer->pitch);
		// SDL_BlitSurface(vid.screen, NULL, vid.buffer, NULL);
		// SDL_UnlockTexture(vid.texture);
		if (rotate) SDL_RenderCopyEx(vid.renderer,vid.texture,NULL,&(SDL_Rect){0,device_width,device_width,device_height},rotate*90,NULL,SDL_FLIP_NONE);
		else SDL_RenderCopy(vid.renderer, vid.texture, NULL,NULL);
		SDL_RenderPresent(vid.renderer);
		return;
	}
	
	// uint32_t then = SDL_GetTicks();
	// SDL_LockTexture(vid.texture,NULL,&vid.buffer->pixels,&vid.buffer->pitch); // 1ms
	// ((scaler_t)vid.blit->blit)(
	// 	vid.blit->src,vid.buffer->pixels,
	// 	// vid.blit->src_w,vid.blit->src_h,vid.blit->src_p,
	// 	vid.blit->true_w,vid.blit->true_h,vid.blit->src_p, // TODO: fix to be confirmed, issue may not present on this platform
	// 	vid.buffer->w,vid.buffer->h,vid.buffer->pitch
	// ); // 1ms
	// SDL_UnlockTexture(vid.texture); // 9ms
	// LOG_info("SDL_LockTexture/blit/SDL_UnlockTexture blocked for %ims (%i,%i)\n", SDL_GetTicks()-then,vid.buffer->w,vid.buffer->h);
	
	// comparable, 1-2ms faster depending on size of framebuffer
	// uint32_t then = SDL_GetTicks();
	SDL_UpdateTexture(vid.texture,NULL,vid.blit->src,vid.blit->src_p);
	// LOG_info("SDL_UpdateTexture blocked for %ims (%i,%i)\n", SDL_GetTicks()-then,vid.buffer->w,vid.buffer->h);
	
	SDL_Texture* target = vid.texture;
	int w = vid.blit->src_w;
	int h = vid.blit->src_h;
	if (vid.sharpness==SHARPNESS_CRISP) {
		SDL_SetRenderTarget(vid.renderer,vid.target);
		SDL_RenderCopy(vid.renderer, vid.texture, NULL,NULL);
		SDL_SetRenderTarget(vid.renderer,NULL);
		w *= hard_scale;
		h *= hard_scale;
		target = vid.target;
	}
	
	SDL_Rect* src_rect = &(SDL_Rect){0,0,w,h};
	SDL_Rect* dst_rect = &(SDL_Rect){0,0,device_width,device_height};
	if (vid.blit->aspect==0) { // native or cropped
		int w = vid.blit->src_w * vid.blit->scale;
		int h = vid.blit->src_h * vid.blit->scale;
		int x = (device_width - w) / 2;
		int y = (device_height - h) / 2;
		// dst_rect = &(SDL_Rect){x,y,w,h};
		dst_rect->x = x;
		dst_rect->y = y;
		dst_rect->w = w;
		dst_rect->h = h;
	}
	else if (vid.blit->aspect>0) { // aspect
		int h = device_height;
		int w = h * vid.blit->aspect;
		if (w>device_width) {
			double ratio = 1 / vid.blit->aspect;
			w = device_width;
			h = w * ratio;
		}
		int x = (device_width - w) / 2;
		int y = (device_height - h) / 2;
		// dst_rect = &(SDL_Rect){x,y,w,h};
		dst_rect->x = x;
		dst_rect->y = y;
		dst_rect->w = w;
		dst_rect->h = h;
	}
	
	int ox,oy;
	oy = (device_width-device_height)/2;
	ox = -oy;
	if (rotate) SDL_RenderCopyEx(vid.renderer,target,src_rect,&(SDL_Rect){ox+dst_rect->x,oy+dst_rect->y,dst_rect->w,dst_rect->h},rotate*90,NULL,SDL_FLIP_NONE);
	else SDL_RenderCopy(vid.renderer, target, src_rect, dst_rect);
	
	updateEffect();
	if (vid.effect) {
		ox = effect_scale - (dst_rect->x % effect_scale);
		oy = effect_scale - (dst_rect->y % effect_scale);
		if (ox==effect_scale) ox = 0;
		if (oy==effect_scale) oy = 0;
		if (rotate) SDL_RenderCopyEx(vid.renderer,vid.effect,&(SDL_Rect){0,0,device_width,device_height},&(SDL_Rect){oy,ox+device_width,device_width,device_height},rotate*90,&(SDL_Point){0,0},SDL_FLIP_NONE);
		else SDL_RenderCopy(vid.renderer, vid.effect, &(SDL_Rect){0,0,device_width,device_height},&(SDL_Rect){ox,oy,device_width,device_height});
		
		// if (rotate) SDL_RenderCopyEx(vid.renderer,vid.effect,&(SDL_Rect){0,0,device_width,device_height},&(SDL_Rect){0,device_width,device_width,device_height},rotate*90,NULL,SDL_FLIP_NONE);
		// else SDL_RenderCopy(vid.renderer, vid.effect, &(SDL_Rect){0,0,device_width,device_height},&(SDL_Rect){0,0,device_width,device_height});
	}
	
	// uint32_t then = SDL_GetTicks();
	SDL_RenderPresent(vid.renderer);
	// LOG_info("SDL_RenderPresent blocked for %ims\n", SDL_GetTicks()-then);
	
	vid.blit = NULL;
}

///////////////////////////////

// TODO: 
#define OVERLAY_WIDTH PILL_SIZE // unscaled
#define OVERLAY_HEIGHT PILL_SIZE // unscaled
#define OVERLAY_BPP 4
#define OVERLAY_DEPTH 16
#define OVERLAY_PITCH (OVERLAY_WIDTH * OVERLAY_BPP) // unscaled
#define OVERLAY_RGBA_MASK 0x00ff0000,0x0000ff00,0x000000ff,0xff000000 // ARGB
static struct OVL_Context {
	SDL_Surface* overlay;
} ovl;

SDL_Surface* PLAT_initOverlay(void) {
	ovl.overlay = SDL_CreateRGBSurface(SDL_SWSURFACE, SCALE2(OVERLAY_WIDTH,OVERLAY_HEIGHT),OVERLAY_DEPTH,OVERLAY_RGBA_MASK);
	return ovl.overlay;
}
void PLAT_quitOverlay(void) {
	if (ovl.overlay) SDL_FreeSurface(ovl.overlay);
}
void PLAT_enableOverlay(int enable) {

}

///////////////////////////////

static int online = 0;
void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	// *is_charging = 0;
	// *charge = PWR_LOW_CHARGE;
	// return;
	
	*is_charging = getInt("/sys/class/power_supply/ac/online");
	
	// fallback
	// char state[256];
	// getFile("/sys/class/power_supply/battery/status", state, 256);
	// *is_charging = prefixMatch("Charging",state); // this is working better

	int i = getInt("/sys/class/power_supply/battery/capacity");
	// worry less about battery and more about the game you're playing
	     if (i>80) *charge = 100;
	else if (i>60) *charge =  80;
	else if (i>40) *charge =  60;
	else if (i>20) *charge =  40;
	else if (i>10) *charge =  20;
	else           *charge =  10;

	// wifi status, just hooking into the regular PWR polling
	// char status[16];
	// getFile("/sys/class/net/wlan0/operstate", status,16);
	// online = prefixMatch("up", status);
}

#define LED_PATH "/sys/class/leds/led1/brightness"
#define BACKLIGHT_PATH "/sys/class/backlight/backlight/bl_power"

void PLAT_enableBacklight(int enable) {
	if (enable) {
		SetBrightness(GetBrightness());
		putInt(BACKLIGHT_PATH, FB_BLANK_UNBLANK);
	}
	else {
		SetRawBrightness(0);
		system("dd if=/dev/zero of=/dev/fb0");
		putInt(BACKLIGHT_PATH, FB_BLANK_POWERDOWN);
	}
}

void PLAT_powerOff(void) {
	system("rm -f /tmp/minui_exec && sync");
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	// putInt(LED_PATH,255);
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();
	exit(0);
}

///////////////////////////////

#define CPU_PATH "/sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed"
#define GPU_PATH "/sys/devices/platform/ff400000.gpu/devfreq/ff400000.gpu/governor"
#define DMC_PATH "/sys/devices/platform/dmc/devfreq/dmc/governor"
void PLAT_setCPUSpeed(int speed) {
	int freq = 0;
	switch (speed) {
		case CPU_SPEED_MENU: 		freq =  600000; break;
		case CPU_SPEED_POWERSAVE:	freq =  816000; break;
		case CPU_SPEED_NORMAL: 		freq = 1416000; break;
		case CPU_SPEED_PERFORMANCE: freq = 2016000; break;
	}
	
	if (speed==CPU_SPEED_PERFORMANCE) {
		putFile(GPU_PATH, "performance");
		putFile(DMC_PATH, "performance");
	}
	else {
		putFile(GPU_PATH, "simple_ondemand");
		putFile(DMC_PATH, "dmc_ondemand");
	}
	putInt(CPU_PATH, freq); // userspace
}

// #define RUMBLE_PATH "/sys/devices/virtual/timed_output/vibrator/enable"
void PLAT_setRumble(int strength) {
	// putInt(RUMBLE_PATH, strength?1000:0);
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

char* PLAT_getModel(void) {
	return "MagicX XU Mini M";
}

int PLAT_isOnline(void) {
	return online;
}