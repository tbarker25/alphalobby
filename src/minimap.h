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

#ifndef MINIMAP_H
#define MINIMAP_H

#define WC_MINIMAP L"Minimap"

enum MinimapType {
	MINIMAP_NORMAL,
	MINIMAP_METAL,
	MINIMAP_HEIGHT,
};

void Minimap_set_type(enum MinimapType);
void Minimap_redraw(void);
void Minimap_set_bitmap(const uint16_t *map_pixels,
		uint16_t metal_map_width, uint16_t metal_map_height,
		const uint8_t *metal_map_pixels, uint16_t height_map_width,
		uint16_t height_map_height, const uint8_t *height_map_pixels);


#endif /* end of include guard: MINIMAP_H */
