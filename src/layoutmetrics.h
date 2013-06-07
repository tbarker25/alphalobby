/*
 * Copyright (c) 2013, Thomas Barker
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * It under the terms of the GNU General Public License as published by
 * The Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * But WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * Along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LAYOUTMETRICS_H
#define LAYOUTMETRICS_H

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

#define TEXTBOX_Y MAP_Y(10)

#define PROGRESS_Y MAP_Y(8)
#define PROGRESS_S_X MAP_X(107)
#define PROGRESS_L_X MAP_X(237)


#define TEXTLABEL_Y MAP_Y(8)
#define BOX_Y MAP_Y(10)

#define SMALLEST_SPACING_X MAP_X(2)
#define SMALLEST_SPACING_Y MAP_Y(2)
#define RELATED_SPACING_X MAP_X(4)
#define RELATED_SPACING_Y MAP_Y(4)
#define UNRELATED_SPACING_X MAP_X(7)
#define UNRELATED_SPACING_Y MAP_Y(7)

#define ICON_SIZE 22

typedef struct HFONT__* HFONT;
extern HFONT gFont;
extern uint16_t baseUnitX;
extern uint16_t baseUnitY;
extern uint32_t scrollWidth;

void InitializeSystemMetrics(void);

#endif /* end of include guard: LAYOUTMETRICS_H */
