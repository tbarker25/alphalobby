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

#ifndef UNITSYNC_H
#define UNITSYNC_H

// #define STRBUF_SIZE 100000


/**
 * @addtogroup unitsync_api Unitsync API
 * @{
*/

/**
 * @brief 2d vector storing a map defined starting position
 * @sa MapInfo
 * @deprecated
 */
typedef struct StartPos
{
	int x; ///< X component
	int z; ///< Z component
}StartPos;


/**
 * @brief Metadata of a map
 * @sa GetMapInfo GetMapInfoEx
 * @deprecated
 */
typedef struct MapInfo
{
	char* description;   ///< Description (max 255 chars)
	int tidal_strength;   ///< Tidal strength
	int gravity;         ///< Gravity
	float max_metal;      ///< Metal scale factor
	int extractor_radius; ///< Extractor radius (of metal extractors)
	int min_wind;         ///< Minimum wind speed
	int max_wind;         ///< Maximum wind speed

	// 0.61b1+
	int width;              ///< Width of the map
	int height;             ///< Height of the map
	int pos_len;           ///< Number of defined start positions
	StartPos positions[16]; ///< Start positions defined by the map (max 16)

	// VERSION>=1
	char* author;   ///< Creator of the map (max 200 chars)
}MapInfo;


/**
 * @brief Available bitmap type_hints
 * @sa GetInfoMap
 */
enum BitmapType {
	bm_grayscale_8  = 1, ///< 8 bits per pixel grayscale bitmap
	bm_grayscale_16 = 2  ///< 16 bits per pixel grayscale bitmap
};

/** @} */

#endif // UNITSYNC_H
