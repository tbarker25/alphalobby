#pragma once
#include <inttypes.h>

#define MAP_X(x)\
	((x) * baseUnitX / 4)
#define MAP_Y(y)\
	((y) * baseUnitY / 8)
#define TEXT_LENGTH(n)\
	(n * baseUnitX)

	// DIALOGBOX1_X = 212, DIALOGBOX1_Y = 188,
	// DIALOGBOX2_X = 227, DIALOGBOX2_Y = 215,
	// DIALOGBOX3_X = 252, DIALOGBOX3_Y = 218,
	// DIALOGBOX4_X = 263, DIALOGBOX4_Y = 263,

#define COMMANDBUTTON_X MAP_X(50)
#define COMMANDBUTTON_Y MAP_Y(14)

#define PROGRESS_Y MAP_Y(8)
#define PROGRESS_S_X MAP_X(107)
#define PROGRESS_L_X MAP_X(237)

#define TEXTBOX_Y MAP_Y(14)
#define TEXTLABEL_Y MAP_Y(8)
#define BOX_Y MAP_Y(10)

#define SMALLEST_SPACING_X MAP_X(2)
#define SMALLEST_SPACING_Y MAP_Y(2)
#define RELATED_SPACING_X MAP_X(4)
#define RELATED_SPACING_Y MAP_Y(4)
#define UNRELATED_SPACING_X MAP_X(7)
#define UNRELATED_SPACING_Y MAP_Y(7)


extern struct HFONT__ *gFont;
extern uint16_t baseUnitX;
extern uint16_t baseUnitY;
extern uint32_t scrollWidth;

void InitializeSystemMetrics(void);
