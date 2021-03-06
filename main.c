#include <vitasdk.h>
#include <taihen.h>
#include <taipool.h>
#include <libk/stdlib.h>
#include <libk/stdio.h>
#include "renderer.h"
#include "encoder.h"
#include "rescaler.h"

#define HOOKS_NUM      13
#define MENU_ENTRIES    6
#define QUALITY_ENTRIES 5

enum {
	NOT_TRIGGERED,
	CONFIG_MENU
};

#define NO_DEBUG

// Hooks related variables
static SceUID g_hooks[HOOKS_NUM], record_thread_id, async_mutex;
static tai_hook_ref_t ref[HOOKS_NUM];
static uint8_t cur_hook = 0;

// Status related variables
static SceUID firstBoot = 1;
static uint8_t status = NOT_TRIGGERED;
static int cfg_i = 0;
static int qual_i = 2;
static error[128] = {0};

// Video recording related variables
static uint8_t video_quality = 255;
static encoder jpeg_encoder;
static uint8_t frameskip = 0;
static uint8_t is_async = 1;
static uint8_t* mem;
static uint32_t* rescale_buffer = NULL;
static uint8_t is_recording = 0;

// Drawing related variables
static int loopDrawing = 0;
static uint32_t old_buttons;

// Menu related variables
static char* qualities[] = {"Best", "High", "Default", "Low", "Worst"};
static uint8_t qual_val[] = {0, 64, 128, 192, 255};
static char* menu[] = {"Video Quality: ", "Hardware Acceleration: ", "Downscaler: ", "Frame Skip: ", "Recorder Type: ", " Screen Recording"};

// Generic variables
static uint32_t mempool_size = 0x500000;
static char titleid[16];

#ifndef NO_DEBUG
static int debug = 0;
#endif

SceUID sync_fd = 0;

// Config Menu Renderer
void drawConfigMenu(){
	int i;
	for (i = 0; i < MENU_ENTRIES; i++) {
		(i == cfg_i) ? setTextColor(0x0000FF00) : setTextColor(0x00FFFFFF);
		switch (i) {
			case 0:
				drawStringF(5, 80 + i*20, "%s%s", menu[i], qualities[qual_i]);
				break;
			case 1:
				drawStringF(5, 80 + i*20, "%s%s", menu[i], jpeg_encoder.isHwAccelerated ? "Enabled" : "Disabled");
				break;
			case 2:
				drawStringF(5, 80 + i*20, "%s%s", menu[i], (jpeg_encoder.rescale_buffer != NULL) ? "Enabled" : "Disabled");
				break;
			case 3:
				if (is_async) drawStringF(5, 80 + i*20, "%sAuto", menu[i]);
				else drawStringF(5, 80 + i*20, "%s%u", menu[i], frameskip);
				break;
			case 4:
				drawStringF(5, 80 + i*20, "%s%s", menu[i], is_async ? "Asynchronous" : "Synchronous");
				break;
			case 5:
				drawStringF(5, 80 + i*20, "%s%s", is_recording ? "Stop" : "Start", menu[i]);
				break;
			default:
				drawString(5, 80 + i*20, menu[i]);
				break;
		}
	}
	setTextColor(0x00FFFFFF);
}

// Generic hooking function
void hookFunction(uint32_t nid, const void* func) {
	g_hooks[cur_hook] = taiHookFunctionImport(&ref[cur_hook], TAI_MAIN_MODULE, TAI_ANY_LIBRARY, nid, func);
	cur_hook++;
}

// Asynchronous video recording thread
int record_thread(SceSize args, void *argp) {
	
	int mem_size, frames = 0;
	SceDisplayFrameBuf param;
	param.size = sizeof(SceDisplayFrameBuf);
	SceUID fd = 0;
	char path[256];
	
	for (;;) {
		if (is_recording) {
			sceDisplayGetFrameBuf(&param, SCE_DISPLAY_SETBUF_NEXTFRAME);
			if (rescale_buffer != NULL) { // Downscaler available
				rescaleBuffer((uint32_t*)param.base, rescale_buffer, param.pitch, param.width, param.height);
				mem = encodeARGB(&jpeg_encoder, rescale_buffer, 512, &mem_size);
			} else mem = encodeARGB(&jpeg_encoder, param.base, param.pitch, &mem_size);
			sceIoWrite(fd, mem, mem_size);
		} else {
			if (fd > 0) sceIoClose(fd);
			sceKernelWaitSema(async_mutex, 1, NULL);
			SceDateTime date;
			sceRtcGetCurrentClockLocalTime(&date);
			sprintf(path, "ux0:data/vid_%s-%02d_%02d_%04d-%02d_%02d_%02d.mjpg", titleid, date.day, date.month, date.year, date.hour, date.minute, date.second);
			fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT, 0777);
			if (fd < 0) {
				sprintf(error, "ERROR: sceIoOpen -> 0x%08X", fd);
				is_recording = 0;
			}
		}
	}

	return 0;
}

// Alters recording state
void alterRecordingState() {
	is_recording = !is_recording;
	if (is_recording) {
		encoderSetQuality(&jpeg_encoder, qual_val[qual_i]);
		if (is_async) sceKernelSignalSema(async_mutex, 1);
		status = NOT_TRIGGERED;
	}
}

// Checking buttons startup/closeup
void checkInput(SceCtrlData *ctrl) {
	SceDisplayFrameBuf param;
	if (status != NOT_TRIGGERED) {
		if ((ctrl->buttons & SCE_CTRL_DOWN) && (!(old_buttons & SCE_CTRL_DOWN))) {
			cfg_i++;
			if (cfg_i >= MENU_ENTRIES) cfg_i = 0;
		} else if ((ctrl->buttons & SCE_CTRL_UP) && (!(old_buttons & SCE_CTRL_UP))) {
			cfg_i--;
			if (cfg_i < 0 ) cfg_i = MENU_ENTRIES-1;
		} else if ((ctrl->buttons & SCE_CTRL_CROSS) && (!(old_buttons & SCE_CTRL_CROSS))) {
			switch (cfg_i) {
			case 0:
				qual_i = (qual_i + 1) % QUALITY_ENTRIES;
				encoderSetQuality(&jpeg_encoder, qual_val[qual_i]);
				break;
			case 2:
				param.size = sizeof(SceDisplayFrameBuf);
				sceDisplayGetFrameBuf(&param, SCE_DISPLAY_SETBUF_NEXTFRAME);
				if (param.width == 960 && param.height == 544) encoderSetRescaler(&jpeg_encoder, (rescale_buffer == NULL) ? 1 : 0, param.pitch);
				rescale_buffer = jpeg_encoder.rescale_buffer;
				break;
			case 3:
				if (!is_async) frameskip = (frameskip + 1) % 5;
				break;
			case 4:
				is_async = (is_async + 1) % 2;
				break;
			case 5:
				alterRecordingState();
				break;
			default:
				break;
			}
		}else if ((ctrl->buttons & SCE_CTRL_TRIANGLE) && (!(old_buttons & SCE_CTRL_TRIANGLE))) {
			status = NOT_TRIGGERED;
		}
	} else if ((ctrl->buttons & SCE_CTRL_LTRIGGER) && (ctrl->buttons & SCE_CTRL_SELECT)) {
		status = CONFIG_MENU;
	} else if (((ctrl->buttons & SCE_CTRL_LTRIGGER) && (ctrl->buttons & SCE_CTRL_START))
		&& (!((old_buttons & SCE_CTRL_LTRIGGER) && (old_buttons & SCE_CTRL_START)))) {
		alterRecordingState();
	}
	old_buttons = ctrl->buttons;
}

// This can be considered as our main loop
int sceDisplaySetFrameBuf_patched(const SceDisplayFrameBuf *pParam, int sync) {
	
	if (firstBoot) {
		firstBoot = 0;
		
		// Initializing internal renderer
		setTextColor(0x00FFFFFF);
		
		// Initializing JPG encoder
		encoderInit(pParam->width, pParam->height, pParam->pitch, &jpeg_encoder, video_quality, 0);
		rescale_buffer = (uint32_t*)jpeg_encoder.rescale_buffer;	
	}
	
	updateFramebuf(pParam);
	SceCtrlData pad;
	sceCtrlPeekBufferPositive(0, &pad, 1);
	checkInput(&pad);
	
	if (status == NOT_TRIGGERED) {
		if (error[0]) drawString(5,5, error[0]);
	} else {
		switch (status) {
		case CONFIG_MENU:
			drawString(5, 5, "Vita Recorder v.0.1.1 - CONFIG MENU");
			drawStringF(5, 25, "Title ID: %s", titleid);
			drawStringF(5, 45, "Resolution: %dx%d", pParam->width, pParam->height);
			drawConfigMenu();
			break;
		default:
			break;
		}
	}
	
	if (is_recording) {
		if (!is_async) {
			char path[256];
			if (!sync_fd) {
				SceDateTime date;
				sceRtcGetCurrentClockLocalTime(&date);
				sprintf(path, "ux0:data/vid_%s-%02d_%02d_%04d-%02d_%02d_%02d.mjpg", titleid, date.day, date.month, date.year, date.hour, date.minute, date.second);
				sync_fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT, 0777);
				if (sync_fd < 0) {
					sprintf(error, "ERROR: sceIoOpen -> 0x%08X", sync_fd);
					is_recording = 0;
				}
			}
			if (loopDrawing == frameskip) {				
				int mem_size;
				if (rescale_buffer != NULL) { // Downscaler available
					rescaleBuffer((uint32_t*)pParam->base, rescale_buffer, pParam->pitch, pParam->width, pParam->height);
					mem = encodeARGB(&jpeg_encoder, rescale_buffer, 512, &mem_size);
				} else mem = encodeARGB(&jpeg_encoder, pParam->base, pParam->pitch, &mem_size);
				sceIoWrite(sync_fd, mem, mem_size);
				loopDrawing = 0;
			} else loopDrawing++;
		}
		setTextColor(0xFF0000FF);
		drawString(5, 5, "R");
		setTextColor(0x00FFFFFF);
	} else {
		if (sync_fd > 0) {
			sceIoClose(sync_fd);
			sync_fd = 0;
		}
	}
	
#ifndef NO_DEBUG
	if (status != CONFIG_MENU) {
		setTextColor(0x00FFFFFF);
		drawStringF(5, 100, "taipool free space: %lu KBs", (taipool_get_free_space()>>10));
		drawStringF(5, 120, "Debug: 0x%08X", debug);
	}
#endif
	
	return TAI_CONTINUE(int, ref[0], pParam, sync);
}

int genericInputDisable(int idx, int port, SceCtrlData *ctrl, int count, int is_negative) {
	int ret = TAI_CONTINUE(int, ref[idx], port, ctrl, count);
	
	if (status == CONFIG_MENU) // Disable input handling when in config menu
		ctrl->buttons = is_negative ? 0xFFFFFFFF : 0;
	
	return ret;
}

int sceCtrlPeekBufferPositive_patched(int port, SceCtrlData *ctrl, int count) {
	return genericInputDisable(1, port, ctrl, count, 0);
}

int sceCtrlPeekBufferPositive2_patched(int port, SceCtrlData *ctrl, int count) {
	return genericInputDisable(2, port, ctrl, count, 0);
}

int sceCtrlReadBufferPositive_patched(int port, SceCtrlData *ctrl, int count) {
	return genericInputDisable(3, port, ctrl, count, 0);
}

int sceCtrlReadBufferPositive2_patched(int port, SceCtrlData *ctrl, int count) {
	return genericInputDisable(4, port, ctrl, count, 0);
}

int sceCtrlPeekBufferPositiveExt_patched(int port, SceCtrlData *ctrl, int count) {
	return genericInputDisable(5, port, ctrl, count, 0);
}

int sceCtrlPeekBufferPositiveExt2_patched(int port, SceCtrlData *ctrl, int count) {
	return genericInputDisable(6, port, ctrl, count, 0);
}

int sceCtrlReadBufferPositiveExt_patched(int port, SceCtrlData *ctrl, int count) {
	return genericInputDisable(7, port, ctrl, count, 0);
}

int sceCtrlReadBufferPositiveExt2_patched(int port, SceCtrlData *ctrl, int count) {
	return genericInputDisable(8, port, ctrl, count, 0);
}

int sceCtrlPeekBufferNegative_patched(int port, SceCtrlData *ctrl, int count) {
	return genericInputDisable(9, port, ctrl, count, 1);
}

int sceCtrlPeekBufferNegative2_patched(int port, SceCtrlData *ctrl, int count) {
	return genericInputDisable(10, port, ctrl, count, 1);
}

int sceCtrlReadBufferNegative_patched(int port, SceCtrlData *ctrl, int count) {
	return genericInputDisable(11, port, ctrl, count, 1);
}

int sceCtrlReadBufferNegative2_patched(int port, SceCtrlData *ctrl, int count) {
	return genericInputDisable(12, port, ctrl, count, 1);
}

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args) {
	
	// Setting maximum clocks
	scePowerSetArmClockFrequency(444);
	scePowerSetBusClockFrequency(222);
	scePowerSetGpuClockFrequency(222);
	scePowerSetGpuXbarClockFrequency(166);
	
	// Checking if game is blacklisted
	sceAppMgrAppParamGetString(0, 12, titleid , 256);
	if (strncmp(titleid, "NPXS", 4) == 0) { // System Apps
		return SCE_KERNEL_START_NO_RESIDENT;
	} else if (strncmp(titleid, "PCSE00491", 9) == 0) { // Minecraft (USA)
		mempool_size = 0x200000;
	} else if (strncmp(titleid, "PCSB00074", 9) == 0) { // Assassin's Creed III: Liberation (EUR)
		mempool_size = 0x200000;
	} else if (strncmp(titleid, "PCSF00178", 9) == 0) { // Soul Sacrifice (EUR)
		mempool_size = 0x200000;
	} else if (strncmp(titleid, "PCSF00024", 9) == 0) { // Gravity Rush (EUR)
		mempool_size = 0x200000;
	} else if (strncmp(titleid, "PCSB00170", 9) == 0) { // FIFA 13 (EUR)
		mempool_size = 0x200000;
	} else if (strncmp(titleid, "PCSB00001", 9) == 0) { // Uncharted: Golden Abyss (EUR)
		mempool_size = 0x200000;
	} else if (strncmp(titleid, "PCSB00404", 9) == 0) { // Muramasa Rebirth (EUR)
		mempool_size = 0x200000;
	} else if (strncmp(titleid, "PCSF00217", 9) == 0) { // Smart As... (EUR)
		mempool_size = 0x200000;
	} else if (strncmp(titleid, "PCSF00485", 9) == 0) { // Ratchet and Clank 2 (EUR)
		mempool_size = 0x200000;
	} else if (strncmp(titleid, "PCSF00486", 9) == 0) { // Ratchet and Clank 3 (EUR)
		mempool_size = 0x200000;
	}
	
	// Mutex for asynchronous streaming triggering
	async_mutex = sceKernelCreateSema("async_mutex", 0, 0, 1, NULL);
	
	// Starting secondary thread for asynchronous streaming
	record_thread_id = sceKernelCreateThread("record_thread", record_thread, 0xA0, 0x100000, 0, 0, NULL);
	if (record_thread_id >= 0) sceKernelStartThread(record_thread_id, 0, NULL);
	
	// Initializing taipool mempool for dynamic memory managing
	if (taipool_init(mempool_size) < 0) {
		mempool_size = 0x200000;
		taipool_init(mempool_size);
	}
	
	// Hooking needed functions
	hookFunction(0x7A410B64, sceDisplaySetFrameBuf_patched);
	hookFunction(0xA9C3CED6, sceCtrlPeekBufferPositive_patched);
	hookFunction(0x15F81E8C, sceCtrlPeekBufferPositive2_patched);
	hookFunction(0x67E7AB83, sceCtrlReadBufferPositive_patched);
	hookFunction(0xC4226A3E, sceCtrlReadBufferPositive2_patched);
	hookFunction(0xA59454D3, sceCtrlPeekBufferPositiveExt_patched);
	hookFunction(0x860BF292, sceCtrlPeekBufferPositiveExt2_patched);
	hookFunction(0xE2D99296, sceCtrlReadBufferPositiveExt_patched);
	hookFunction(0xA7178860, sceCtrlReadBufferPositiveExt2_patched);
	hookFunction(0x104ED1A7, sceCtrlPeekBufferNegative_patched);
	hookFunction(0x81A89660, sceCtrlPeekBufferNegative2_patched);
	hookFunction(0x15F96FB0, sceCtrlReadBufferNegative_patched);
	hookFunction(0x27A0C5FB, sceCtrlReadBufferNegative2_patched);
	
	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
	
	// Freeing encoder
	if (!firstBoot) {
		encoderTerm(&jpeg_encoder);
	
		// Freeing hooks
		int i;
		for (i = 0; i < HOOKS_NUM; i++) {
			taiHookRelease(g_hooks[i], ref[i]);
		}
	}
	
	return SCE_KERNEL_STOP_SUCCESS;
	
}