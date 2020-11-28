#include "doomgeneric.h"
#include <stdio.h>
#include <stdlib.h>


// static uint32_t FB[DOOMGENERIC_RESX * DOOMGENERIC_RESY];
uint32_t* DG_ScreenBuffer = 0;

void dg_Create()
{
	DG_ScreenBuffer = malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(uint32_t));
	printf("screenbuffer: %p\n", DG_ScreenBuffer);

	DG_Init();
}

