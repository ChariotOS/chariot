#ifndef DOOM_GENERIC
#define DOOM_GENERIC

#include <stdint.h>

#define DOOMGENERIC_SCALE (1)
#define DOOMGENERIC_RESX (320 * DOOMGENERIC_SCALE)
#define DOOMGENERIC_RESY (200 * DOOMGENERIC_SCALE)


extern uint32_t* DG_ScreenBuffer;




void DG_PumpEventLoop(void);
void DG_Init();
void DG_DrawFrame();
void DG_SleepMs(uint32_t ms);
uint32_t DG_GetTicksMs();
int DG_GetKey(int* pressed, unsigned char* key);
void DG_SetWindowTitle(const char * title);

#endif //DOOM_GENERIC
