/*
Cheat Device Own The City 0.13, Made by MicSuit
ONLY WORKS ON NFSC OTC US v1.00!!!
Things To Do:
  - Make the cheats
  - Improve the Explorer
*/

#include <pspkernel.h>
#include <pspctrl.h>
#include <systemctrl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PLG_VER 0.13 // Plugin Version
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

char logfile[32] = "ms0:/CDOTC/log.txt"; // Used for Log
char savefile[32] = "ms0:/CDOTC/cdotc_sett.ini"; // Used for Settings
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

void (* NFS_Print3)(char* string);
void NFS_Print3_Patched(char* string);

void (*GetGameLanguageID)(int LanguageNumber);
void GetGameLanguageIDPatched(int LanguageNumber);

void(* LoadLanguage)(int *param_1, int LanguageID);
void LoadLanguagePatched(int *param_1, int LanguageID);

void (*FUN_0017c324)(int param_1, u32 param_2);
void FUN_0017c324_Patched(int param_1, u32 param_2);

PSP_MODULE_INFO("CheatDeviceOwnTheCity", 0, 0, 1);

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

u32 mem_start = 0x08800000; // Used to stop out of bounds reads
int data_var = 0; // needed for nfs_command, seems to use a 4 byte.

char MAIN_TITLE[64];
char MAIN_CHEATS_STRINGS[8][32] = {"Unlimited Nitrous - ", "Car Acceleration - ", "Car Steering - ", "Freeze Opponents - ", "Unlimited Crew Power - ", "Go to Screen - ", "Set Game Language to: ", "Exit Game"};
int CHEAT_LENGTH = sizeof(MAIN_CHEATS_STRINGS)/sizeof(MAIN_CHEATS_STRINGS[0]);    
int MAIN_CHEATS_BOOLS[] = {0, 2, 2, 0, 0, 2, 2, 2};
int MAIN_CHEATS_VALUES[] = {-1, 1, 1, -1, -1, 1, 1, -1};
char MAIN_CHEAT_SCREEN[6][29] = {"Main Menu", "Crew House", "Public Service Annoucement", "Boot Title Screen", "Infrastucture Menu", "Adhoc Menu"};
char MAIN_CHEAT_SCREEN_INTERNAL[6][17] = {"MainMenu", "CrewHouse", "BootPSA", "BootTitle", "INTERNETMAINMENU", "AdhocLobby"};
char MAIN_CHEAT_LANGUAGES[6][8] = {"English", "French", "German", "Italian", "Spanish", "Dutch"};
char MAIN_CHEAT_TEXT[256] = "";
int SELECTED_CHEAT = 0;
int LOCKED_CHEAT_SCREEN = 1;
int LOCKED_CHEAT_SCREEN_INFRA = 1;
int LOCKED_CHEAT_SCREEN_ELSE = 0;
char LanguageIDSettingSaveFile[2] = "";
int LanguageIDSettingSaveFileInt = 0;

void CHECK_CHEATS() {
  if (MAIN_CHEAT_TEXT[0] != '\0')
  {
    MAIN_CHEAT_TEXT[0] = '\0';
  }
  char value[2];
  char screen[27];
  char language[8];
  int i = 0;
  for (i=0; i < CHEAT_LENGTH; i++) {
    if (i == SELECTED_CHEAT)
      strcat(MAIN_CHEAT_TEXT, ">  ");

    if (strcmp(MAIN_CHEATS_STRINGS[i], "Go to Screen - ") == 0) {
      if (strcmp(MAIN_CHEAT_SCREEN[MAIN_CHEATS_VALUES[i]-1], "Infrastucture Menu") == 0 || (strcmp(MAIN_CHEAT_SCREEN[MAIN_CHEATS_VALUES[i]-1], "Adhoc Menu")) == 0) 
      {
        if (LOCKED_CHEAT_SCREEN_INFRA == 1) {
          strcat(MAIN_CHEAT_TEXT, "(Locked) ");
        } 
      } else {
        if (LOCKED_CHEAT_SCREEN == 1 | LOCKED_CHEAT_SCREEN_ELSE == 1) {
          strcat(MAIN_CHEAT_TEXT, "(Locked) ");
        }
      }
    }
    strcat(MAIN_CHEAT_TEXT, MAIN_CHEATS_STRINGS[i]);
    if (MAIN_CHEATS_BOOLS[i] < 2)
      strcat(MAIN_CHEAT_TEXT, MAIN_CHEATS_BOOLS[i]?"ON":"OFF");
    else {
      if (strcmp(MAIN_CHEATS_STRINGS[i], "Go to Screen - ") == 0) {
        sprintf(screen, "%s", MAIN_CHEAT_SCREEN[MAIN_CHEATS_VALUES[i]-1]);
        strcat(MAIN_CHEAT_TEXT, screen);
      }
      else if (strcmp(MAIN_CHEATS_STRINGS[i], "Set Game Language to: ") == 0) {
        sprintf(language, "%s", MAIN_CHEAT_LANGUAGES[MAIN_CHEATS_VALUES[i]-1]);
        strcat(MAIN_CHEAT_TEXT, language);
      }
      else if (strcmp(MAIN_CHEATS_STRINGS[i], "Exit Game") == 0) {
      // Don't do anything in case it's "Exit Game"
      } else {
        sprintf(value, "%d", MAIN_CHEATS_VALUES[i]);
        strcat(MAIN_CHEAT_TEXT, value);
      }
    }

    if (i < CHEAT_LENGTH-1)
      strcat(MAIN_CHEAT_TEXT, "\n");

  }
}

void UPDATE_TEXT_EXPLORER() {
  NFS_Command_Patched("CloseMessageBox", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0); // To Display Message Box with text
  CHECK_CHEATS();
  NFS_Command_Patched("DisplayMessageBox", &data_var, "_root", 9, MAIN_TITLE, MAIN_CHEAT_TEXT, "1", &data_var, "0", "0", "0", &data_var, &data_var); // To Display Message Box with text, only works on menu
}

void GOTO_SCREEN_CHEAT() {
  if (strcmp(MAIN_CHEAT_SCREEN[MAIN_CHEATS_VALUES[SELECTED_CHEAT]-1], "Infrastucture Menu") == 0) {
    if (LOCKED_CHEAT_SCREEN_INFRA == 0)
      NFS_Command("AIPNet_GoToScreen",0,0,1,"INTERNETMAINMENU",0,0,0,0,0,0,0,0);
  }
  else if (strcmp(MAIN_CHEAT_SCREEN[MAIN_CHEATS_VALUES[SELECTED_CHEAT]-1], "Adhoc Menu") == 0) {
    if (LOCKED_CHEAT_SCREEN_INFRA == 0)
      NFS_Command("AIPNet_GoToScreen",0,0,1,"Adhoc Menu",0,0,0,0,0,0,0,0);
  }
  else {
    if (LOCKED_CHEAT_SCREEN == 0 && LOCKED_CHEAT_SCREEN_ELSE == 0)
      NFS_Command("OpenScreen",0,"/_root",1,MAIN_CHEAT_SCREEN_INTERNAL[MAIN_CHEATS_VALUES[SELECTED_CHEAT]-1], 0,0,0,0,0,0,0,0);
  }
}

void SET_LANG_TO_CHEAT() {
  char LanguageChangeIDStr[2] = "";
  char* err_msg = "";
  sprintf(LanguageChangeIDStr, "%d", MAIN_CHEATS_VALUES[SELECTED_CHEAT]-1);
  SceUID ffd = sceIoOpen(savefile, PSP_O_CREAT | PSP_O_TRUNC | PSP_O_WRONLY, 0777);
  if (ffd >= 0) {
    sceIoWrite(ffd, LanguageChangeIDStr, strlen(LanguageChangeIDStr));
    sceIoClose(ffd);
    NFS_Command_Patched("DisplayMessageBox", &data_var, "_root", 9, "INFORMATION", "Restart Your Game to Change Game Language", "1", &data_var, "1", "0", "$I_OK", &data_var, &data_var); // To Display Message Box with text, only works on menu
  } else {
    sprintf(err_msg, "Unable to save Language Config\nError: 0x%08X", (u32)ffd);
    NFS_Command_Patched("DisplayMessageBox", &data_var, "_root", 9, "ERROR", err_msg, "1", &data_var, "1", "0", "$I_OK", &data_var, &data_var); // To Display Message Box with text, only works on menu

  }
}

void EXIT_GAME_CHEAT() {
  NFS_Command_Patched("DisplayMessageBox", &data_var, "_root", 9, "EXIT GAME CHEAT", "Exiting Game...", "1", &data_var, "0", "0", "0", &data_var, &data_var); // To Display Message Box with text, only works on menu
  sceKernelExitGame();
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

  if (strcmp(MAIN_CHEATS_STRINGS[SELECTED_CHEAT], "Go to Screen - ") == 0) {
    GOTO_SCREEN_CHEAT();
  }

  if (strcmp(MAIN_CHEATS_STRINGS[SELECTED_CHEAT], "Exit Game") == 0) {
    EXIT_GAME_CHEAT();
  }
}

void CIRCLE_CHEAT_EXPLORER() {
  if (strcmp(MAIN_CHEATS_STRINGS[SELECTED_CHEAT], "Set Game Language to: ") == 0) {
    SET_LANG_TO_CHEAT();
  }
}

void RIGHT_CHEAT_EXPLORER() {
  if (strcmp(MAIN_CHEATS_STRINGS[SELECTED_CHEAT], "Go to Screen - ") == 0 || strcmp(MAIN_CHEATS_STRINGS[SELECTED_CHEAT], "Set Game Language to: ") == 0) {
    if (MAIN_CHEATS_VALUES[SELECTED_CHEAT] < 6) {
      MAIN_CHEATS_VALUES[SELECTED_CHEAT] += 1;
      UPDATE_TEXT_EXPLORER();
    }
  }
  else {
    if (MAIN_CHEATS_VALUES[SELECTED_CHEAT] > -1 && MAIN_CHEATS_VALUES[SELECTED_CHEAT] < 5) {
      MAIN_CHEATS_VALUES[SELECTED_CHEAT] += 1;
      UPDATE_TEXT_EXPLORER();
    }
  }

}

void LEFT_CHEAT_EXPLORER() {
  if (MAIN_CHEATS_VALUES[SELECTED_CHEAT] > 1) {
    MAIN_CHEATS_VALUES[SELECTED_CHEAT] -= 1;
    UPDATE_TEXT_EXPLORER();
  }
}

void HACK_CHEATS() {
  // Nothing for now...
}

int (*FUN_00051f9c)(int param_1, char* param_2, int param_3, int param_4);
int (*FUN_00094180)(char* param_1, int param_2, int param_3);


SceInt64 sceKernelGetSystemTimeWidePatched(void) { // Game Loop
  // Calculations for FPS
  //SceInt64 cur_micros = sceKernelGetSystemTimeWide();
  //if (cur_micros >= (last_micros + 1000000)) { //every second
  //  delta_micros = cur_micros - last_micros;
  //  last_micros = cur_micros;
  //  fps = (frames / (double)delta_micros) * 1000000.0f;
  //  frames = 0;
  //} frames++;
  HACK_CHEATS();

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

      if ((pad.Buttons & PSP_CTRL_CIRCLE) && CURR_BUTTONS != OLD_BUTTONS) { // Pressed Down
          CIRCLE_CHEAT_EXPLORER();
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
      NFS_Command_Patched("DisplayMessageBox", &data_var, "_root", 9, MAIN_TITLE, MAIN_CHEAT_TEXT, "1", &data_var, "0", "0", "0", &data_var, &data_var); // To Display Message Box with text, only works on menu
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
  //NFS_Command("OpenScreen",0,"/_root",1,"MainMenu", 0,0,0,0,0,0,0,0); // Opens NFS's Main Menu
  //logPrintf("command: %s", command);
  if (strcmp(command, "OpenScreen") == 0) {
    if (strcmp(title, "MainMenu") == 0) {
      if (LOCKED_CHEAT_SCREEN == 1) LOCKED_CHEAT_SCREEN = 0;
      if (LOCKED_CHEAT_SCREEN_INFRA == 0) LOCKED_CHEAT_SCREEN_INFRA = 1;
      if (LOCKED_CHEAT_SCREEN_ELSE == 1) LOCKED_CHEAT_SCREEN_ELSE = 0;
    }
  }

  if (strcmp(command, "AIPNet_GoToScreen") == 0) {
    if (strcmp(title, "AdhocLobby") == 0) {
      if (LOCKED_CHEAT_SCREEN_INFRA == 1) LOCKED_CHEAT_SCREEN_INFRA = 0;
      if (LOCKED_CHEAT_SCREEN_ELSE == 0) LOCKED_CHEAT_SCREEN_ELSE = 1;
    }

  }
  NFS_Command(command, param_2, param_3, param_4, title, text, param_7, param_8, param_9, param_10, param_11, param_12, param_13);
  //Example Code:
  //NFS_Command("DisplayMessageBox", 0, 0, 9, 0, "Welcome to CheatDeviceOwnTheCity!", 0, 0, 0, 0, 0, 0, 0); // Close Message Box
  //logPrintf("command: %s\nparam_2: %d\nparam_3: %s\nparam_4: %d\nparam_5: %s\nparam_7: %x\nparam_8: %x\nparam_9: %x\n", command, param_2, param_3, param_4, param_5, param_7, param_8, param_9);
}

void NFS_Print3_Patched(char* string) {
  //char pointer[32];
  //sprintf(pointer, "%p", param_1-0x08800000);
  NFS_Print3(string);
}

void NFS_Print_Patched(int param_1, char* param_2) { // Print Custom Message, still to discover param_1
  NFS_Print(param_1, param_2);
  //logPrintf("%s", param_2);
}

void GetGameLanguageIDPatched(int LanguageNumber) {
  GetGameLanguageID(LanguageIDSettingSaveFileInt);
}

void LoadLanguagePatched(int *param_1, int LanguageID) {
  LoadLanguage(param_1, 1000+LanguageIDSettingSaveFileInt);
}

void FUN_0017c324_Patched(int param_1, u32 param_2) {
  FUN_0017c324(param_1, param_2);
  //logPrintf("Function called");
}


void PatchNFSC(u32 text_addr) {

  SceIoStat info;
	sceIoGetstat(savefile, &info);
  SceUID fd = sceIoOpen(savefile, PSP_O_RDONLY, 0777);
  if (fd >= 0) {
    sceIoRead(fd, &LanguageIDSettingSaveFile, (int)info.st_size);
    LanguageIDSettingSaveFileInt = atoi(LanguageIDSettingSaveFile);
    sceIoClose(fd);
  }
  MAIN_CHEATS_VALUES[6] = LanguageIDSettingSaveFileInt+1;

  FUN_00051f9c = (void*)(text_addr + 0x00051f9c);
  FUN_00094180 = (void*)(text_addr + 0x00094180);
  FUN_0017c324 = (void*)(text_addr + 0x0017c324);
  sprintf(MAIN_TITLE, "CheatDeviceOTC v%0.2f Made By MicSuit", PLG_VER);
  HIJACK_FUNCTION(text_addr + 0x00271834, NFS_Command_Patched, NFS_Command); 
  HIJACK_FUNCTION(text_addr + 0x00168ae8, NFS_Print_Patched, NFS_Print);
  HIJACK_FUNCTION(text_addr + 0x0009fc8c, NFS_Print3_Patched, NFS_Print3);
  HIJACK_FUNCTION(text_addr + 0x0009fa4c, GetGameLanguageIDPatched, GetGameLanguageID);
  HIJACK_FUNCTION(text_addr + 0x002ea46c, LoadLanguagePatched, LoadLanguage);
  HIJACK_FUNCTION(text_addr + 0x0017c324, FUN_0017c324_Patched, FUN_0017c324);

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
