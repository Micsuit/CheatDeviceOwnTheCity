/*
Cheat Device Own The City 0.1, Made by MicSuit
ONLY WORKS ON NFSC OTC US v1.00!!!
The Plugin for now doesn't do anything.
Things To Do:
  - Print text to screen
  - Get Input from User
*/

#include <pspkernel.h>
#include <pspctrl.h>
#include <systemctrl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#define PLG_VER 0.1 // Plugin Version
#define EMULATOR_DEVCTL__IS_EMULATOR 0x00000003

////////////////////////////////////////////////////////////////////////////
#define MAKE_CALL(a, f) _sw(0x0C000000 | (((u32)(f) >> 2) & 0x03FFFFFF), a);

#define MAKE_JUMP(a, f) _sw(0x08000000 | (((u32)(f) & 0x0FFFFFFC) >> 2), a);

#define MAKE_DUMMY_FUNCTION(a, r) { \
  u32 _func_ = a; \
  if (r == 0) { \
    _sw(0x03E00008, _func_); \
    _sw(0x00001021, _func_ + 4); \
  } else { \
    _sw(0x03E00008, _func_); \
    _sw(0x24020000 | r, _func_ + 4); \
  } \
}

#define REDIRECT_FUNCTION(a, f) { \
  u32 _func_ = a; \
  _sw(0x08000000 | (((u32)(f) >> 2) & 0x03FFFFFF), _func_); \
  _sw(0, _func_ + 4); \
}

#define HIJACK_FUNCTION(a, f, ptr) { \
  u32 _func_ = a; \
  static u32 patch_buffer[3]; \
  _sw(_lw(_func_), (u32)patch_buffer); \
  _sw(_lw(_func_ + 4), (u32)patch_buffer + 8);\
  MAKE_JUMP((u32)patch_buffer + 4, _func_ + 8); \
  _sw(0x08000000 | (((u32)(f) >> 2) & 0x03FFFFFF), _func_); \
  _sw(0, _func_ + 4); \
  ptr = (void *)patch_buffer; \
}
//////////////////////////////////////////////////////////////////////////////

char *logfile = ""; // Used for Log
static STMOD_HANDLER previous;
SceCtrlData pad;
u32 mod_text_addr;
u32 mod_text_size;
u32 mod_data_size;
int hold_n = 0;
int SHOW_CHEAT = 0;
int SHOW_CHEAT_1 = 0;
int CURR_BUTTONS;
int OLD_BUTTONS;

register int gp asm("gp");

void (* NFS_Command)(char* param_1,int param_2,int param_3,int param_4, char* param_5, char* param_6, u32 param_7,u32 param_8, u32 param_9, u32 param_10, u32 param_11, u32 param_12, u32 param_13);
void NFS_Command_Patched(char* param_1,int param_2,int param_3,int param_4, char* param_5, char* param_6, u32 param_7,u32 param_8, u32 param_9, u32 param_10, u32 param_11, u32 param_12, u32 param_13);

void (* NFS_Print)(int param_1, char* param_2);
void NFS_Print_Patched(int param_1, char* param_2);

PSP_MODULE_INFO("cheatdevice_otc", 0, 0, 1);

int logPrintf(const char *text, ...) { // Log Things...
  va_list list;
  char string[512];

  va_start(list, text);
  vsprintf(string, text, list);
  va_end(list);

  SceUID fd = sceIoOpen(logfile, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
  if (fd >= 0) {
    sceIoWrite(fd, string, strlen(string));
    sceIoWrite(fd, "\n", 1);
    sceIoClose(fd);
  }

  return 0;
}

SceInt64 cur_micros = 0, delta_micros = 0, last_micros = 0;
u32 frames = 0; float fps = 0.0f;

SceInt64 sceKernelGetSystemTimeWidePatched(void) { // Game Loop
  // Calculations for FPS
  SceInt64 cur_micros = sceKernelGetSystemTimeWide();
  if (cur_micros >= (last_micros + 1000000)) { //every second
    delta_micros = cur_micros - last_micros;
    last_micros = cur_micros;
    fps = (frames / (double)delta_micros) * 1000000.0f;
    frames = 0;
  } frames++;

  sceCtrlPeekBufferPositive(&pad, 1);
  if (pad.Buttons != 0) {
    OLD_BUTTONS = CURR_BUTTONS;
    CURR_BUTTONS = pad.Buttons;
    if ((pad.Buttons & PSP_CTRL_SQUARE) && CURR_BUTTONS != OLD_BUTTONS) { // Pressed Square
      if (SHOW_CHEAT == 0) SHOW_CHEAT = 1;
      else SHOW_CHEAT = 0;
    }
  } else {
    OLD_BUTTONS = 0;
    CURR_BUTTONS = 0;
  }

  if (SHOW_CHEAT == 1) {
    if (SHOW_CHEAT_1 == 0) {
      NFS_Command_Patched("DisplayMessageBox", 0, 0, 9, "You Pressed Square!", "Welcome to CheatDeviceOwnTheCity!", 0, 0, 0, 0, 0, 0, 0); // To Display Message Box with text, only works on menu
      SHOW_CHEAT_1 = 1;
    }
  }

  else {
    if (SHOW_CHEAT_1 == 1) {
      NFS_Command_Patched("CloseMessageBox", 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0); // To Display Message Box with text
      SHOW_CHEAT_1 = 0;
      SHOW_CHEAT = 0;
    }
  }



  return NULL;
}

void NFS_Command_Patched(char* param_1,int param_2,int param_3,int param_4, char* param_5, char* param_6, u32 param_7,u32 param_8, u32 param_9, u32 param_10, u32 param_11, u32 param_12, u32 param_13) { // Can use "DisplayMessageBox" and "CloseMessageBox" for param_1, param_5: title, param_6: text, still to discover the rest
  NFS_Command(param_1, param_2, param_3, param_4, param_5, param_6, param_7, param_8, param_9, param_10, param_11, param_12, param_13);
  
  //Example Code:
  //NFS_Command("DisplayMessageBox", 0, 0, 9, "You Pressed Square!", "Welcome to CheatDeviceOwnTheCity!", 0, 0, 0, 0, 0, 0, 0); // Close Message Box
  
}

void NFS_Print_Patched(int param_1, char* param_2) { // Print Custom Message, still to discover param_1
  NFS_Print(param_1, param_2);
  
  /*
  Example Code:

  if (strcmp(param_2, "M_Laps") == 0) { // if current print text == "Laps", display our text
    //Tweak x, y:
    *(u32*) (param_1 + 0x28) = 0x42c80000; // x, 100.0
    *(u32*) (param_1 + 0x2C) = 0x43480000; // y, 200.0
    NFS_Print(param_1, "Custom Text Here"); // Print Text
  } else {
    NFS_Print(param_1, param_2);
  }
  */
}

void PatchNFSC(u32 text_addr) {
  HIJACK_FUNCTION(text_addr + 0x00271834, NFS_Command_Patched, NFS_Command); 
  HIJACK_FUNCTION(text_addr + 0x00168ae8, NFS_Print_Patched, NFS_Print);
  MAKE_CALL(text_addr + 0x001df474, sceKernelGetSystemTimeWidePatched); // Loop
}

int patch(u32 text_addr) {
  if (strcmp((char *)(text_addr + 0x0039043C), "nfsGame") == 0) { // NFSC OTC ULUS10114 v1.00 (US)
    logPrintf("Found Need For Speed Own The City, 'nfsGame' > 0x%08X", text_addr + 0x0039043C);
    PatchNFSC(text_addr);
  }
  return 0;
}


static void CheckModules() { // PPSSPP
  logfile = "ms0:/PSP/PLUGINS/CheatDeviceOwnTheCity/log.txt";
  sceIoRemove(logfile); // Log Things...
  SceUID modules[10];
  int count = 0;
  if (sceKernelGetModuleIdList(modules, sizeof(modules), &count) >= 0) {
    int i;
    SceKernelModuleInfo info;
     for (i = 0; i < count; ++i) {
      info.size = sizeof(SceKernelModuleInfo);
      if (sceKernelQueryModuleInfo(modules[i], &info) < 0)
        continue;
      
      logPrintf("info.name: %s", info.name);
      if (strcmp(info.name, "nfsGame") == 0) {
		
		    logPrintf("[INFO] PPSSPP detected!");
		    mod_text_addr = info.text_addr;
		    mod_text_size = info.text_size;
		    mod_data_size = info.data_size;
		
		    // With this approach the game continues to run when patch() is still in progress
		    int ret = patch(mod_text_addr);
		    if(ret != 0)
			    return;

		    sceKernelDcacheWritebackAll();
		    return;
      }
    }
  }
}

int OnModuleStart(SceModule2 *mod) { // PSP
  logfile = "ms0:/seplugins/log.txt";
  sceIoRemove(logfile); // Log Things...
  char *modname = mod->modname;
  logPrintf("Modname: %s", modname);
  if(strcmp(modname, "nfsGame") == 0 ) {
	  mod_text_addr = mod->text_addr;
	  mod_text_size = mod->text_size;
	  mod_data_size = mod->data_size;
	  int ret = patch(mod_text_addr);
	  if( ret != 0 )
		  return -1;
	
    sceKernelDcacheWritebackAll();
  }

  if(!previous)
    return 0;

  return previous(mod);
}


int module_start(SceSize argc, void* argp) {
	if(sceIoDevctl("kemulator:", EMULATOR_DEVCTL__IS_EMULATOR, NULL, 0, NULL, 0) == 0) // PPSSPP
		CheckModules(); // Scan the modules using normal/official syscalls.

	else // PSP
    previous = sctrlHENSetStartModuleHandler(OnModuleStart); 
	
	return 0;
}
