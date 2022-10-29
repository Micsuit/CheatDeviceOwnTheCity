/*
Cheat Device Own The City 0.11, Made by MicSuit
ONLY WORKS ON NFSC OTC US v1.00!!!
Things To Do:
  - Make the cheats
  - Improve the Explorer

Useful Functions:
  NFS_Command("OpenScreen",0,"/_root",1,"CrewHouse", 0,0,0,0,0,0,0,0); // Opens Crew House
  NFS_Command("OpenScreen",0,"/_root",1,"BootPSA", 0,0,0,0,0,0,0,0); // Opens Public Service Annoucement Screen
  NFS_Command("OpenScreen",0,"/_root",1,"MainMenu", 0,0,0,0,0,0,0,0); // Opens NFS's Main Menu
*/

#include <pspkernel.h>
#include <pspctrl.h>
#include <systemctrl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#define PLG_VER 0.11 // Plugin Version
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

void (* NFS_Command)(char* command,int param_2,char* param_3,int param_4, char* title, char* text, u32 param_7,u32 param_8, u32 param_9, u32 param_10, u32 param_11, u32 param_12, u32 param_13);
void NFS_Command_Patched(char* command,int param_2,char* param_3,int param_4, char* title, char* text, u32 param_7,u32 param_8, u32 param_9, u32 param_10, u32 param_11, u32 param_12, u32 param_13);

void (* NFS_Print)(int param_1, char* param_2);
void NFS_Print_Patched(int param_1, char* param_2);

void (* NFS_Print2)(int param_1, char* param_2, char* param_3);
void NFS_Print2_Patched(int param_1, char* param_2, char* param_3);

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

//SceInt64 cur_micros = 0, delta_micros = 0, last_micros = 0;
//u32 frames = 0; float fps = 0.0f;
char MAIN_TITLE[64];
char MAIN_CHEATS_STRINGS[6][128] = {"Unlimited Nitrous - ", "Car Acceleration - ", "Car Steering - ", "Freeze Opponents - ", "Unlimited Crew Power - ", "Teleport Somewhere - "};
int CHEAT_LENGTH = sizeof(MAIN_CHEATS_STRINGS)/sizeof(MAIN_CHEATS_STRINGS[0]);    
int MAIN_CHEATS_BOOLS[] = {0, 2, 2, 0, 0, 0};
int MAIN_CHEATS_VALUES[] = {-1, 1, 1, -1, -1, -1};
char* MAIN_CHEAT_TEXT = "";
int SELECTED_CHEAT = 0;

void CHECK_CHEATS() {
  if (MAIN_CHEAT_TEXT[0] != '\0')
  {
    MAIN_CHEAT_TEXT[0] = '\0';
  }
  char value[2];
  for (int i=0; i < CHEAT_LENGTH; i++) {
    if (i == SELECTED_CHEAT)
      strcat(MAIN_CHEAT_TEXT, ">  ");
    strcat(MAIN_CHEAT_TEXT, MAIN_CHEATS_STRINGS[i]);
    //logPrintf("MAIN_CHEAT_TEXT: %s", MAIN_CHEAT_TEXT);
    if (MAIN_CHEATS_BOOLS[i] < 2)
      strcat(MAIN_CHEAT_TEXT, MAIN_CHEATS_BOOLS[i]?"ON":"OFF");
    else {
      sprintf(value, "%d", MAIN_CHEATS_VALUES[i]);
      strcat(MAIN_CHEAT_TEXT, value);
    }

    if (i < CHEAT_LENGTH-1)
      strcat(MAIN_CHEAT_TEXT, "\n");

  }
}

void UPDATE_TEXT_EXPLORER() {
  NFS_Command_Patched("CloseMessageBox", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0); // To Display Message Box with text
  CHECK_CHEATS();
  NFS_Command_Patched("DisplayMessageBox", 0, 0, 9, MAIN_TITLE, MAIN_CHEAT_TEXT, 0x31000000, 0, 0x32000000, 0x30000000, 0, 0, 0); // To Display Message Box with text, only works on menu

}

void DOWN_CHEAT_EXPLORER() {
  SELECTED_CHEAT += 1;
  UPDATE_TEXT_EXPLORER();
}

void UP_CHEAT_EXPLORER() {
  SELECTED_CHEAT -= 1;
  UPDATE_TEXT_EXPLORER();
}

void CROSS_CHEAT_EXPLORER() {
  if (MAIN_CHEATS_BOOLS[SELECTED_CHEAT] < 2) {
    if(MAIN_CHEATS_BOOLS[SELECTED_CHEAT] == 0) MAIN_CHEATS_BOOLS[SELECTED_CHEAT] = 1;
    else MAIN_CHEATS_BOOLS[SELECTED_CHEAT] = 0;
    UPDATE_TEXT_EXPLORER();  
  }

}

void RIGHT_CHEAT_EXPLORER() {
  if (MAIN_CHEATS_VALUES[SELECTED_CHEAT] > -1 && MAIN_CHEATS_VALUES[SELECTED_CHEAT] < 5) {
    MAIN_CHEATS_VALUES[SELECTED_CHEAT] += 1;
    UPDATE_TEXT_EXPLORER();
  }

}

void LEFT_CHEAT_EXPLORER() {
  if (MAIN_CHEATS_VALUES[SELECTED_CHEAT] > 1) {
    MAIN_CHEATS_VALUES[SELECTED_CHEAT] -= 1;
    UPDATE_TEXT_EXPLORER();
  }
}

SceInt64 sceKernelGetSystemTimeWidePatched(void) { // Game Loop
  // Calculations for FPS
  //SceInt64 cur_micros = sceKernelGetSystemTimeWide();
  //if (cur_micros >= (last_micros + 1000000)) { //every second
  //  delta_micros = cur_micros - last_micros;
  //  last_micros = cur_micros;
  //  fps = (frames / (double)delta_micros) * 1000000.0f;
  //  frames = 0;
  //} frames++;

  sceCtrlPeekBufferPositive(&pad, 1);

  if (pad.Buttons != 0) {
    OLD_BUTTONS = CURR_BUTTONS;
    CURR_BUTTONS = pad.Buttons;
    if ((pad.Buttons & PSP_CTRL_SQUARE) && CURR_BUTTONS != OLD_BUTTONS) { // Pressed Square
      if ((pad.Buttons & PSP_CTRL_LTRIGGER)) {
        if (SHOW_CHEAT == 0) SHOW_CHEAT = 1;
        else SHOW_CHEAT = 0;
      }
    }

    if (SHOW_CHEAT == 1) {
      if ((pad.Buttons & PSP_CTRL_CROSS) && CURR_BUTTONS != OLD_BUTTONS) // Pressed Cross
          CROSS_CHEAT_EXPLORER();

      if ((pad.Buttons & PSP_CTRL_DOWN) && CURR_BUTTONS != OLD_BUTTONS) { // Pressed Down
        if (SELECTED_CHEAT+1 < CHEAT_LENGTH) {
          DOWN_CHEAT_EXPLORER();
        }
      }

      if ((pad.Buttons & PSP_CTRL_UP) && CURR_BUTTONS != OLD_BUTTONS) { // Pressed Up
        if (SELECTED_CHEAT > 0)
          UP_CHEAT_EXPLORER();
      }

      if ((pad.Buttons & PSP_CTRL_LEFT) && CURR_BUTTONS != OLD_BUTTONS) // Pressed Left
          LEFT_CHEAT_EXPLORER();

      if ((pad.Buttons & PSP_CTRL_RIGHT) && CURR_BUTTONS != OLD_BUTTONS) // Pressed Right
          RIGHT_CHEAT_EXPLORER();
    }


  } else {
    CURR_BUTTONS = 0;
  }

  if (SHOW_CHEAT == 1) {
    if (SHOW_CHEAT_1 == 0) {
      CHECK_CHEATS();
      NFS_Command_Patched("DisplayMessageBox", 0, 0, 9, MAIN_TITLE, MAIN_CHEAT_TEXT, 0x31000000, 0, 0x32000000, 0x30000000, 0, 0, 0); // To Display Message Box with text, only works on menu
      
      SHOW_CHEAT_1 = 1;
    }
  }

  else {
    if (SHOW_CHEAT_1 == 1) {
      NFS_Command_Patched("CloseMessageBox", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0); // To Display Message Box with text

      SHOW_CHEAT_1 = 0; SHOW_CHEAT = 0;
    }
  }

  return 0;
}

void NFS_Command_Patched(char* command,int param_2,char* param_3,int param_4, char* title, char* text, u32 param_7,u32 param_8, u32 param_9, u32 param_10, u32 param_11, u32 param_12, u32 param_13) { // Can use "DisplayMessageBox" and "CloseMessageBox" for param_1, param_5: title, param_6: text, still to discover the rest
  NFS_Command(command, param_2, param_3, param_4, title, text, param_7, param_8, param_9, param_10, param_11, param_12, param_13);
  //Example Code:
  //NFS_Command("DisplayMessageBox", 0, 0, 9, 0, "Welcome to CheatDeviceOwnTheCity!", 0, 0, 0, 0, 0, 0, 0); // Close Message Box
  //logPrintf("command: %s\nparam_2: %d\nparam_3: %s\nparam_4: %d\nparam_5: %s\nparam_7: %x\nparam_8: %x\nparam_9: %x\n", command, param_2, param_3, param_4, param_5, param_7, param_8, param_9);
}

void NFS_Print2_Patched(int param_1, char* param_2, char* param_3) {
  NFS_Print2(param_1, param_2, param_3);
  //NFS_Print2(param_1, param_2, "4");
  //logPrintf("param_1: %p", param_1);
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
  sprintf(MAIN_TITLE, "CheatDeviceOTC %0.2f Made By MicSuit", PLG_VER);
  HIJACK_FUNCTION(text_addr + 0x00271834, NFS_Command_Patched, NFS_Command); 
  HIJACK_FUNCTION(text_addr + 0x00168ae8, NFS_Print_Patched, NFS_Print);
  HIJACK_FUNCTION(text_addr + 0x00096db0, NFS_Print2_Patched, NFS_Print2);

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
