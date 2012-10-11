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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_LOG_H__
#define __BMESH_LOG_H__

struct BMFace;
struct BMVert;
struct BMesh;

typedef struct BMLog BMLog;
typedef struct BMLogEntry BMLogEntry;

/* Allocate and initialize a new BMLog */
BMLog *BM_log_create(BMesh *bm);

/* Free all the data in a BMLog including the log itself */
void BM_log_free(BMLog *log);

/* Start a new log entry and update the log entry list */
BMLogEntry *BM_log_entry_add(BMLog *log);

void BM_log_undo(BMesh *bm, BMLog *log);
void BM_log_redo(BMesh *bm, BMLog *log);

void BM_log_vert_moved(BMLog *log, struct BMVert *v);
void BM_log_vert_added(BMLog *log, struct BMVert *v);
void BM_log_face_added(BMLog *log, struct BMFace *f);
void BM_log_face_added(BMLog *log, struct BMFace *f);
void BM_log_vert_removed(BMLog *log, struct BMVert *v);
void BM_log_face_removed(BMLog *log, struct BMFace *f);

void BM_log_all_added(BMesh *bm, BMLog *log);
void BM_log_all_removed(BMesh *bm, BMLog *log);

const float *BM_log_original_vert_co(BMLog *log, BMVert *v);

#endif
