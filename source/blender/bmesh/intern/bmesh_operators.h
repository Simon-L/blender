/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_OPERATORS_H__
#define __BMESH_OPERATORS_H__

/** \file blender/bmesh/intern/bmesh_operators.h
 *  \ingroup bmesh
 */

/*see comments in intern/bmesh_opdefines.c for documentation of specific operators*/

/*--------defines/enumerations for specific operators-------*/

/*quad innervert values*/
enum {
	SUBD_INNERVERT,
	SUBD_PATH,
	SUBD_FAN,
	SUBD_STRAIGHT_CUT
};

enum {
	SUBDIV_SELECT_ORIG,
	SUBDIV_SELECT_INNER,
	SUBDIV_SELECT_INNER_SEL,
	SUBDIV_SELECT_LOOPCUT
};

/* similar face selection slot values */
enum {
	SIMFACE_MATERIAL = 201,
	SIMFACE_IMAGE,
	SIMFACE_AREA,
	SIMFACE_PERIMETER,
	SIMFACE_NORMAL,
	SIMFACE_COPLANAR
};

/* similar edge selection slot values */
enum {
	SIMEDGE_LENGTH = 101,
	SIMEDGE_DIR,
	SIMEDGE_FACE,
	SIMEDGE_FACE_ANGLE,
	SIMEDGE_CREASE,
	SIMEDGE_BEVEL,
	SIMEDGE_SEAM,
	SIMEDGE_SHARP
};

/* similar vertex selection slot values */
enum {
	SIMVERT_NORMAL = 0,
	SIMVERT_FACE,
	SIMVERT_VGROUP,
	SIMVERT_EDGE
};

enum {
	OPUVC_AXIS_X = 1,
	OPUVC_AXIS_Y
};

enum {
	DIRECTION_CW = 1,
	DIRECTION_CCW
};

/* vertex path selection values */
enum {
	VPATH_SELECT_EDGE_LENGTH = 0,
	VPATH_SELECT_TOPOLOGICAL
};

extern BMOpDefine *opdefines[];
extern int bmesh_total_ops;

/*------specific operator helper functions-------*/

struct Object;

void BM_mesh_esubdivide(BMesh *bm, const char edge_hflag,
                        float smooth, float fractal, float along_normal,
                        int numcuts,
                        int seltype, int cornertype,
                        const short use_singleedge, const short use_gridfill,
                        int seed);

#include "intern/bmesh_operator_api_inline.h"

#endif /* __BMESH_OPERATORS_H__ */
