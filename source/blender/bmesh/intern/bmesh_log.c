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

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_mempool.h"

#include "bmesh.h"
#include "range_tree_c_api.h"

#include <assert.h>

typedef struct BMLogEntry {
	struct BMLogEntry *next, *prev;

	/* The following GHashes map from an element ID to one of the log
	 * types above */

	/* Elements that were in the previous entry, but have been
	 * deleted */
	GHash *deleted_verts;
	GHash *deleted_faces;
	/* Elements that were not in the previous entry, but are in the
	 * result of this entry */
	GHash *added_verts;
	GHash *added_faces;

	/* Vertices whose coordinates have changed v  */
	GHash *moved_verts;

	BLI_mempool *pool_verts;
	BLI_mempool *pool_faces;
} BMLogEntry;

struct BMLog {
	/* Tree of free IDs */
	struct RangeTreeUInt *unused_ids;

	/* Mapping from unique ID's to vertices and faces
	 *
	 * More description TODO
	 */
	GHash *id_to_elem;
	GHash *elem_to_id;

	/* All BMLogEntrys, ordered from earliest to most recent */
	ListBase entries;

	/* The current log entry from entries list
	 *
	 * If null, then the original mesh from before any of the log
	 * entries is current (i.e. there is nothing left to undo.)
	 *
	 * If equal to the last entry in the entries list, then all log
	 * entries have been applied (i.e. there is nothing left to redo.)
	 */
	BMLogEntry *current_entry;
};

typedef struct {
	float co[3];
} BMLogVert;

typedef struct {
	unsigned int v_ids[3];
} BMLogFace;

/************************* Get/set element IDs ************************/

/* Get the vertex's unique ID from CustomData */
static unsigned int bm_log_vert_id_get(BMLog *log, BMVert *v)
{
	assert(BLI_ghash_haskey(log->elem_to_id, v));
	return GET_INT_FROM_POINTER(BLI_ghash_lookup(log->elem_to_id, v));
}

static void bm_log_vert_id_set(BMLog *log, BMVert *v, unsigned int id)
{
	void *vid = SET_INT_IN_POINTER(id);
	
	BLI_ghash_remove(log->id_to_elem, vid, NULL, NULL);
	BLI_ghash_insert(log->id_to_elem, vid, v);
	BLI_ghash_remove(log->elem_to_id, v, NULL, NULL);
	BLI_ghash_insert(log->elem_to_id, v, vid);
}

static BMVert *bm_log_vert_from_id(BMLog *log, unsigned int id)
{
	void *key = SET_INT_IN_POINTER(id);
	assert(BLI_ghash_haskey(log->id_to_elem, key));
	return BLI_ghash_lookup(log->id_to_elem, key);
}

/* Get the face's unique ID from CustomData */
static unsigned int bm_log_face_id_get(BMLog *log, BMFace *f)
{
	assert(BLI_ghash_haskey(log->elem_to_id, f));
	return GET_INT_FROM_POINTER(BLI_ghash_lookup(log->elem_to_id, f));
}

static void bm_log_face_id_set(BMLog *log, BMFace *f, unsigned int id)
{
	void *fid = SET_INT_IN_POINTER(id);
	
	BLI_ghash_remove(log->id_to_elem, fid, NULL, NULL);
	BLI_ghash_insert(log->id_to_elem, fid, f);
	BLI_ghash_remove(log->elem_to_id, f, NULL, NULL);
	BLI_ghash_insert(log->elem_to_id, f, fid);
}

static BMFace *bm_log_face_from_id(BMLog *log, unsigned int id)
{
	void *key = SET_INT_IN_POINTER(id);
	assert(BLI_ghash_haskey(log->id_to_elem, key));
	return BLI_ghash_lookup(log->id_to_elem, key);
}

/**********************************************************************/

static void bm_log_assign_ids(BMesh *bm, BMLog *log)
{
	BMIter iter;
	BMVert *v;
	BMFace *f;

	/* Generate vertex IDs */
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		unsigned int id = range_tree_uint_take_any(log->unused_ids);
		bm_log_vert_id_set(log, v, id);
	}

	/* Generate face IDs */
	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		unsigned int id = range_tree_uint_take_any(log->unused_ids);
		bm_log_face_id_set(log, f, id);
	}
}

static BMLogEntry *bm_log_entry_create(void)
{
	BMLogEntry *entry = MEM_callocN(sizeof(BMLogEntry), AT);

	entry->deleted_verts = BLI_ghash_ptr_new(AT);
	entry->deleted_faces = BLI_ghash_ptr_new(AT);
	entry->added_verts = BLI_ghash_ptr_new(AT);
	entry->added_faces = BLI_ghash_ptr_new(AT);
	entry->moved_verts = BLI_ghash_ptr_new(AT);

	entry->pool_verts = BLI_mempool_create(sizeof(BMLogVert), 1, 64, 0);
	entry->pool_faces = BLI_mempool_create(sizeof(BMLogFace), 1, 64, 0);

	return entry;
}

/* Free the data in a log entry; does not free the log entry itself */
static void bm_log_entry_free(BMLogEntry *entry)
{
	BLI_ghash_free(entry->deleted_verts, NULL, NULL);
	BLI_ghash_free(entry->deleted_faces, NULL, NULL);
	BLI_ghash_free(entry->added_verts, NULL, NULL);
	BLI_ghash_free(entry->added_faces, NULL, NULL);
	BLI_ghash_free(entry->moved_verts, NULL, NULL);

	BLI_mempool_destroy(entry->pool_verts);
	BLI_mempool_destroy(entry->pool_faces);
}

static BMLogVert *bm_log_vert_alloc(BMLog *log, BMVert *v)
{
	BMLogEntry *entry = log->current_entry;
	BMLogVert *lv = BLI_mempool_alloc(entry->pool_verts);

	copy_v3_v3(lv->co, v->co);

	return lv;
}

static BMLogFace *bm_log_face_alloc(BMLog *log, BMFace *f)
{
	BMLogEntry *entry = log->current_entry;
	BMLogFace *lf = BLI_mempool_alloc(entry->pool_faces);
	BMVert *v[3];

	assert(f->len == 3);

	BM_iter_as_array(NULL, BM_VERTS_OF_FACE, f, (void **)v, 3);

	lf->v_ids[0] = bm_log_vert_id_get(log, v[0]);
	lf->v_ids[1] = bm_log_vert_id_get(log, v[1]);
	lf->v_ids[2] = bm_log_vert_id_get(log, v[2]);

	return lf;
}

/* Log a change to a vertex's coordinate
 *
 * Handles two separate cases:
 *
 * If the vertex was added in the current log entry, update the
 * coordinate in the map of added vertices.
 *
 * If the vertex already existed prior to the current log entry, a
 * seperate key/value map of moved vertices is used (using the
 * vertex's ID as the key). The coordinates stored in that case are
 * the vertex's original coordinates so that an undo can restore the
 * previous coordinates.
 *
 * On undo, the current coordinates will be swapped with the stored
 * coordinates so that a subsequent redo operation will restore the
 * newer vertex coordinates.
 */
void BM_log_vert_moved(BMLog *log, BMVert *v)
{
	BMLogEntry *entry = log->current_entry;
	BMLogVert *lv;
	unsigned int v_id = bm_log_vert_id_get(log, v);
	void *key = SET_INT_IN_POINTER(v_id);

	/* Find or create the BMLogVert entry */
	if ((lv = BLI_ghash_lookup(entry->added_verts, key))) {
		copy_v3_v3(lv->co, v->co);
	}
	else if (!BLI_ghash_haskey(entry->moved_verts, key)) {
		lv = bm_log_vert_alloc(log, v);
		BLI_ghash_insert(entry->moved_verts, key, lv);
	}
}

/* Log a new vertex as added to the BMesh
 *
 * The new vertex gets a unique ID assigned. (TODO: elaborate on
 * that?) It is then added to a map of added vertices, with the key
 * being its ID and the value containing everything needed to
 * reconstruct that vertex.
 */
void BM_log_vert_added(BMLog *log, BMVert *v)
{
	BMLogVert *lv;
	unsigned int v_id = range_tree_uint_take_any(log->unused_ids);

	bm_log_vert_id_set(log, v, v_id);
	lv = bm_log_vert_alloc(log, v);
	BLI_ghash_insert(log->current_entry->added_verts,
					 SET_INT_IN_POINTER(v_id), lv);
}

/* Log a new face as added to the BMesh
 *
 * The new face gets a unique ID assigned. (TODO: elaborate on that?)
 * It is then added to a map of added faces, with the key being its ID
 * and the value containing everything needed to reconstruct that
 * face.
 */
void BM_log_face_added(BMLog *log, BMFace *f)
{
	BMLogFace *lf;
	unsigned int f_id = range_tree_uint_take_any(log->unused_ids);

	/* Only triangles are supported for now */
	assert(f->len == 3);

	bm_log_face_id_set(log, f, f_id);
	lf = bm_log_face_alloc(log, f);
	BLI_ghash_insert(log->current_entry->added_faces,
					 SET_INT_IN_POINTER(f_id), lf);
}

/* Log a vertex as removed from the BMesh
 *
 * A couple things can happen here:
 * 
 * If the vertex was added as part of the current log entry, then it's
 * deleted and forgotten about entirely. Its unique ID is returned to
 * the unused pool.
 *
 * If the vertex was already part of the BMesh before the current log
 * entry, it is added to a map of deleted vertices, with the key being
 * its ID and the value containing everything needed to reconstruct
 * that vertex.
 *
 * If there's a move record for the vertex, that's used as the
 * vertices original location, then the move record is deleted.
 */
void BM_log_vert_removed(BMLog *log, BMVert *v)
{
	BMLogEntry *entry = log->current_entry;
	unsigned int v_id = bm_log_vert_id_get(log, v);
	void *key = SET_INT_IN_POINTER(v_id);

	if (BLI_ghash_lookup(entry->added_verts, key)) {
		BLI_ghash_remove(entry->added_verts, key, NULL, NULL);
		range_tree_uint_release(log->unused_ids, v_id);
	}
	else {
		BMLogVert *lv, *lv_co;

		lv = bm_log_vert_alloc(log, v);
		BLI_ghash_insert(entry->deleted_verts, key, lv);

		/* If the vertex was moved before deletion, ensure that the
		 * original coordinates are stored */
		if ((lv_co = BLI_ghash_lookup(entry->moved_verts, key))) {
			copy_v3_v3(lv->co, lv_co->co);
			BLI_ghash_remove(entry->moved_verts, key, NULL, NULL);
		}
	}
}

/* Log a face as removed from the BMesh
 *
 * A couple things can happen here:
 * 
 * If the face was added as part of the current log entry, then it's
 * deleted and forgotten about entirely. Its unique ID is returned to
 * the unused pool.
 *
 * If the face was already part of the BMesh before the current log
 * entry, it is added to a map of deleted faces, with the key being
 * its ID and the value containing everything needed to reconstruct
 * that face.
 */
void BM_log_face_removed(BMLog *log, BMFace *f)
{
	BMLogEntry *entry = log->current_entry;
	unsigned int f_id = bm_log_face_id_get(log, f);
	void *key = SET_INT_IN_POINTER(f_id);

	if (BLI_ghash_lookup(entry->added_faces, key)) {
		BLI_ghash_remove(entry->added_faces, key, NULL, NULL);
		range_tree_uint_release(log->unused_ids, f_id);
	}
	else {
		BMLogFace *lf;

		lf = bm_log_face_alloc(log, f);
		BLI_ghash_insert(entry->deleted_faces, key, lf);
	}
}


/************************ Helpers for undo/redo ***********************/

static void bm_log_verts_unmake(BMesh *bm, BMLog *log, GHash *verts)
{
	GHashIterator gh_iter;
	GHASH_ITER (gh_iter, verts) {
		void *key = BLI_ghashIterator_getKey(&gh_iter);
		unsigned int id = GET_INT_FROM_POINTER(key);
		BMVert *v = bm_log_vert_from_id(log, id);

		BM_vert_kill(bm, v);
	}
}

static void bm_log_faces_unmake(BMesh *bm, BMLog *log, GHash *faces)
{
	GHashIterator gh_iter;
	GHASH_ITER (gh_iter, faces) {
		void *key = BLI_ghashIterator_getKey(&gh_iter);
		unsigned int id = GET_INT_FROM_POINTER(key);
		BMFace *f = bm_log_face_from_id(log, id);

		BM_face_kill(bm, f);
	}
}

static void bm_log_verts_restore(BMesh *bm, BMLog *log, GHash *verts)
{
	GHashIterator gh_iter;
	GHASH_ITER (gh_iter, verts) {
		void *key = BLI_ghashIterator_getKey(&gh_iter);
		BMLogVert *lv = BLI_ghashIterator_getValue(&gh_iter);
		BMVert *v = BM_vert_create(bm, lv->co, NULL);
		bm_log_vert_id_set(log, v, GET_INT_FROM_POINTER(key));
	}
}

static void bm_log_faces_restore(BMesh *bm, BMLog *log, GHash *faces)
{
	GHashIterator gh_iter;
	GHASH_ITER (gh_iter, faces) {
		void *key = BLI_ghashIterator_getKey(&gh_iter);
		BMLogFace *lf = BLI_ghashIterator_getValue(&gh_iter);
		BMVert *v[3] = {bm_log_vert_from_id(log, lf->v_ids[0]),
						bm_log_vert_from_id(log, lf->v_ids[1]),
						bm_log_vert_from_id(log, lf->v_ids[2])};
		BMFace *f;

		f = BM_face_create_quad_tri_v(bm, v, 3, NULL, FALSE);
		bm_log_face_id_set(log, f, GET_INT_FROM_POINTER(key));
	}
}

static void bm_log_coords_swap(BMLog *log, GHash *verts)
{
	GHashIterator gh_iter;
	GHASH_ITER (gh_iter, verts) {
		void *key = BLI_ghashIterator_getKey(&gh_iter);
		BMLogVert *lv = BLI_ghashIterator_getValue(&gh_iter);
		unsigned int id = GET_INT_FROM_POINTER(key);
		BMVert *v = bm_log_vert_from_id(log, id);

		swap_v3_v3(v->co, lv->co);
	}
}

#if 0
/* Print the list of entries, marking the current one */
void bm_log_print(const BMLog *log, const char *description)
{
	const BMLogEntry *entry;
	const char *current = " <-- current";
	int i;

	printf("%s:\n", description);
	printf("    % 2d: [ initial ]%s\n", 0,
		   (!log->current_entry) ? current : "");
	for (entry = log->entries.first, i = 1; entry; entry = entry->next, i++) {
		printf("    % 2d: [%p]%s\n", i, entry,
			   (entry == log->current_entry) ? current : "");
	}
}
#endif

/***************************** Public API *****************************/

/* Allocate, initialize, and assign a new BMLog */
BMLog *BM_log_create(BMesh *bm)
{
	BMLog *log = MEM_callocN(sizeof(*log), AT);

	log->unused_ids = range_tree_uint_alloc(0, (unsigned)-1);
	log->id_to_elem = BLI_ghash_ptr_new(AT);
	log->elem_to_id = BLI_ghash_ptr_new(AT);

	/* Assign IDs to all existing vertices and faces */
	bm_log_assign_ids(bm, log);

	return log;
}

/* Free all the data in a BMLog including the log itself */
void BM_log_free(BMLog *log)
{
	BMLogEntry *entry;

	if (log->unused_ids)
		range_tree_uint_free(log->unused_ids);

	if (log->id_to_elem)
		BLI_ghash_free(log->id_to_elem, NULL, NULL);

	for (entry = log->entries.first; entry; entry = entry->next)
		bm_log_entry_free(entry);

	BLI_freelistN(&log->entries);

	MEM_freeN(log);
}

/* Start a new log entry and update the log entry list
 *
 * If the log entry list is empty, or if the current log entry is the
 * last entry, the new entry is simply appended to the end.
 *
 * Otherwise, the new entry is added after the current entry and all
 * following entries are deleted.
 *
 * In either case, the new entry is set as the current log entry.
 */
void BM_log_entry_add(BMLog *log)
{
	BMLogEntry *entry, *next;

	/* Delete any entries after the current one */
	entry = log->current_entry;
	if (entry) {
		for (entry = entry->next; entry; entry = next) {
			next = entry->next;
			bm_log_entry_free(entry);
			BLI_freelinkN(&log->entries, entry);
		}
	}

	/* Create and append the new entry */
	entry = bm_log_entry_create();
	BLI_addtail(&log->entries, entry);
	log->current_entry = entry;
	
}

void BM_log_undo(BMesh *bm, BMLog *log)
{
	BMLogEntry *entry = log->current_entry;

	//bm_log_print(bm_log, "Before undo");

	if (entry) {
		log->current_entry = entry->prev;

		/* TODO: what do we need to with IDs? */

		/* Delete added faces and verts */
		bm_log_faces_unmake(bm, log, entry->added_faces);
		bm_log_verts_unmake(bm, log, entry->added_verts);

		/* Restore deleted verts and faces */
		bm_log_verts_restore(bm, log, entry->deleted_verts);
		bm_log_faces_restore(bm, log, entry->deleted_faces);

		/* Restore vertex coordinates */
		bm_log_coords_swap(log, entry->moved_verts);
	}

	//bm_log_print(bm_log, "After undo");
}

void BM_log_redo(BMesh *bm, BMLog *log)
{
	BMLogEntry *entry = log->current_entry;

	//bm_log_print(bm_log, "Before redo");

	if (entry && entry->next)
		entry = entry->next;
	else
		entry = log->entries.first;

	log->current_entry = entry;

	if (entry) {
		/* TODO: what do we need to with IDs? */

		/* Re-delete previously deleted faces and verts */
		bm_log_faces_unmake(bm, log, entry->deleted_faces);
		bm_log_verts_unmake(bm, log, entry->deleted_verts);

		/* Restore previously added verts and faces */
		bm_log_verts_restore(bm, log, entry->added_verts);
		bm_log_faces_restore(bm, log, entry->added_faces);

		/* Restore vertex coordinates */
		bm_log_coords_swap(log, entry->moved_verts);
	}

	//bm_log_print(bm_log, "After redo");
}

const float *BM_log_original_vert_co(BMLog *log, BMVert *v)
{
	BMLogEntry *entry = log->current_entry;
	const BMLogVert *lv;
	unsigned v_id = bm_log_vert_id_get(log, v);
	void *key = SET_INT_IN_POINTER(v_id);

	assert(entry);

	assert(BLI_ghash_haskey(entry->moved_verts, key));

	lv = BLI_ghash_lookup(entry->moved_verts, key);
	return lv->co;
}

void BM_log_all_added(BMesh *bm, BMLog *log)
{
	BMIter bm_iter;
	BMVert *v;
	BMFace *f;

	// Log all vertices as newly created
	BM_ITER_MESH (v, &bm_iter, bm, BM_VERTS_OF_MESH) {
		BM_log_vert_added(log, v);
	}

	// Log all faces as newly created
	BM_ITER_MESH (f, &bm_iter, bm, BM_FACES_OF_MESH) {
		BM_log_face_added(log, f);
	}
}

void BM_log_all_removed(BMesh *bm, BMLog *log)
{
	BMIter bm_iter;
	BMVert *v;
	BMFace *f;

	// Log deletion of all faces
	BM_ITER_MESH (f, &bm_iter, bm, BM_FACES_OF_MESH) {
		BM_log_face_removed(log, f);
	}

	// Log deletion of all vertices
	BM_ITER_MESH (v, &bm_iter, bm, BM_VERTS_OF_MESH) {
		BM_log_vert_removed(log, v);
	}
}
