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

/** \file blender/bmesh/operators/bmo_connect.c
 *  \ingroup bmesh
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_array.h"
#include "BLI_utildefines.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

#define VERT_INPUT	1
#define EDGE_OUT	1
#define FACE_NEW	2
#define EDGE_MARK	4
#define EDGE_DONE	8
#define FACE_OUT	16

void bmo_connect_verts_exec(BMesh *bm, BMOperator *op)
{
	BMIter iter, liter;
	BMFace *f, *nf;
	BMLoop *(*loops_split)[2] = NULL;
	BLI_array_declare(loops_split);
	BMLoop *l, *nl, *lastl = NULL;
	BMVert *(*verts_pair)[2] = NULL;
	BLI_array_declare(verts_pair);
	int i;
	
	BMO_slot_buffer_flag_enable(bm, op, "verts", BM_VERT, VERT_INPUT);

	for (f = BM_iter_new(&iter, bm, BM_FACES_OF_MESH, NULL); f; f = BM_iter_step(&iter)) {
		BLI_array_empty(loops_split);
		BLI_array_empty(verts_pair);
		
		if (BMO_elem_flag_test(bm, f, FACE_NEW)) {
			continue;
		}

		l = BM_iter_new(&liter, bm, BM_LOOPS_OF_FACE, f);
		lastl = NULL;
		for ( ; l; l = BM_iter_step(&liter)) {
			if (BMO_elem_flag_test(bm, l->v, VERT_INPUT)) {
				if (!lastl) {
					lastl = l;
					continue;
				}

				if (lastl != l->prev && lastl != l->next) {
					BLI_array_grow_one(loops_split);
					loops_split[BLI_array_count(loops_split) - 1][0] = lastl;
					loops_split[BLI_array_count(loops_split) - 1][1] = l;

				}
				lastl = l;
			}
		}

		if (BLI_array_count(loops_split) == 0) {
			continue;
		}
		
		if (BLI_array_count(loops_split) > 1) {
			BLI_array_grow_one(loops_split);
			loops_split[BLI_array_count(loops_split) - 1][0] = loops_split[BLI_array_count(loops_split) - 2][1];
			loops_split[BLI_array_count(loops_split) - 1][1] = loops_split[0][0];
		}

		BM_face_legal_splits(bm, f, loops_split, BLI_array_count(loops_split));
		
		for (i = 0; i < BLI_array_count(loops_split); i++) {
			if (loops_split[i][0] == NULL) {
				continue;
			}

			BLI_array_grow_one(verts_pair);
			verts_pair[BLI_array_count(verts_pair) - 1][0] = loops_split[i][0]->v;
			verts_pair[BLI_array_count(verts_pair) - 1][1] = loops_split[i][1]->v;
		}

		for (i = 0; i < BLI_array_count(verts_pair); i++) {
			nf = BM_face_split(bm, f, verts_pair[i][0], verts_pair[i][1], &nl, NULL, FALSE);
			f = nf;
			
			if (!nl || !nf) {
				BMO_error_raise(bm, op, BMERR_CONNECTVERT_FAILED, NULL);
				BLI_array_free(loops_split);
				return;
			}
			BMO_elem_flag_enable(bm, nf, FACE_NEW);
			BMO_elem_flag_enable(bm, nl->e, EDGE_OUT);
		}
	}

	BMO_slot_buffer_from_enabled_flag(bm, op, "edgeout", BM_EDGE, EDGE_OUT);

	BLI_array_free(loops_split);
	BLI_array_free(verts_pair);
}

static BMVert *get_outer_vert(BMesh *bm, BMEdge *e)
{
	BMIter iter;
	BMEdge *e2;
	int i;

	i = 0;
	BM_ITER_ELEM (e2, &iter, e->v1, BM_EDGES_OF_VERT) {
		if (BMO_elem_flag_test(bm, e2, EDGE_MARK)) {
			i++;
		}
	}

	return (i == 2) ? e->v2 : e->v1;
}

/* Clamp x to the interval {0..len-1}, with wrap-around */
static int clamp_index(const int x, const int len)
{
	if (x >= 0) {
		return x % len;
	}
	else {
		int r = len - (-x % len);
		if (r == len)
			return len - 1;
		else
			return r;
	}
}

/* There probably is a better way to swap BLI_arrays, or if there
 * isn't there should be... */
#define ARRAY_SWAP(elemtype, arr1, arr2)                                      \
	{                                                                         \
		int i;                                                                \
		elemtype *arr_tmp = NULL;                                             \
		BLI_array_declare(arr_tmp);                                           \
		for (i = 0; i < BLI_array_count(arr1); i++) {                         \
			BLI_array_append(arr_tmp, arr1[i]);                               \
		}                                                                     \
		BLI_array_empty(arr1);                                                \
		for (i = 0; i < BLI_array_count(arr2); i++) {                         \
			BLI_array_append(arr1, arr2[i]);                                  \
		}                                                                     \
		BLI_array_empty(arr2);                                                \
		for (i = 0; i < BLI_array_count(arr_tmp); i++) {                      \
			BLI_array_append(arr2, arr_tmp[i]);                               \
		}                                                                     \
		BLI_array_free(arr_tmp);                                              \
	} (void)0

/* get the 2 loops matching 2 verts.
 * first attempt to get the face corners that use the edge defined by v1 & v2,
 * if that fails just get any loop thats on the vert (the first one) */
static void bm_vert_loop_pair(BMesh *bm, BMVert *v1, BMVert *v2, BMLoop **l1, BMLoop **l2)
{
	BMIter liter;
	BMLoop *l;

	if ((v1->e && v1->e->l) &&
	    (v2->e && v2->e->l))
	{
		BM_ITER_ELEM (l, &liter, v1, BM_LOOPS_OF_VERT) {
			if (l->prev->v == v2) {
				*l1 = l;
				*l2 = l->prev;
				return;
			}
			else if (l->next->v == v2) {
				*l1 = l;
				*l2 = l->next;
				return;
			}
		}
	}

	/* fallback to _any_ loop */
	*l1 = BM_iter_at_index(bm, BM_LOOPS_OF_VERT, v1, 0);
	*l2 = BM_iter_at_index(bm, BM_LOOPS_OF_VERT, v2, 0);
}

void bmo_bridge_loops_exec(BMesh *bm, BMOperator *op)
{
	BMEdge **ee1 = NULL, **ee2 = NULL;
	BMVert **vv1 = NULL, **vv2 = NULL;
	BLI_array_declare(ee1);
	BLI_array_declare(ee2);
	BLI_array_declare(vv1);
	BLI_array_declare(vv2);
	BMOIter siter;
	BMIter iter;
	BMEdge *e, *nexte;
	int c = 0, cl1 = 0, cl2 = 0;

	/* merge-bridge support */
	const int   use_merge    = BMO_slot_bool_get(op, "use_merge");
	const float merge_factor = BMO_slot_float_get(op, "merge_factor");

	BMO_slot_buffer_flag_enable(bm, op, "edges", BM_EDGE, EDGE_MARK);

	BMO_ITER (e, &siter, bm, op, "edges", BM_EDGE) {
		if (!BMO_elem_flag_test(bm, e, EDGE_DONE)) {
			BMVert *v, *ov;
			/* BMEdge *e2, *e3, *oe = e; */ /* UNUSED */
			BMEdge *e2, *e3;
			
			if (c > 2) {
				BMO_error_raise(bm, op, BMERR_INVALID_SELECTION, "Select only two edge loops");
				goto cleanup;
			}
			
			e2 = e;
			v = e->v1;
			do {
				v = BM_edge_other_vert(e2, v);
				nexte = NULL;
				BM_ITER_ELEM (e3, &iter, v, BM_EDGES_OF_VERT) {
					if (e3 != e2 && BMO_elem_flag_test(bm, e3, EDGE_MARK)) {
						if (nexte == NULL) {
							nexte = e3;
						}
						else {
							/* edges do not form a loop: there is a disk
							 * with more than two marked edges. */
							BMO_error_raise(bm, op, BMERR_INVALID_SELECTION,
							                "Selection must only contain edges from two edge loops");
							goto cleanup;
						}
					}
				}
				
				if (nexte)
					e2 = nexte;
			} while (nexte && e2 != e);
			
			if (!e2)
				e2 = e;

			e = e2;
			ov = v;
			do {
				if (c == 0) {
					BLI_array_append(ee1, e2);
					BLI_array_append(vv1, v);
				}
				else {
					BLI_array_append(ee2, e2);
					BLI_array_append(vv2, v);
				}
				
				BMO_elem_flag_enable(bm, e2, EDGE_DONE);
				
				v = BM_edge_other_vert(e2, v);
				BM_ITER_ELEM (e3, &iter, v, BM_EDGES_OF_VERT) {
					if (e3 != e2 && BMO_elem_flag_test(bm, e3, EDGE_MARK) && !BMO_elem_flag_test(bm, e3, EDGE_DONE)) {
						break;
					}
				}
				if (e3)
					e2 = e3;
			} while (e3 && e2 != e);
			
			if (v && !e3) {
				if (c == 0) {
					if (BLI_array_count(vv1) && v == vv1[BLI_array_count(vv1) - 1]) {
						printf("%s: internal state waning *TODO DESCRIPTION!*\n", __func__);
					}
					BLI_array_append(vv1, v);
				}
				else {
					BLI_array_append(vv2, v);
				}
			}
			
			/* test for connected loops, and set cl1 or cl2 if so */
			if (v == ov) {
				if (c == 0) {
					cl1 = 1;
				}
				else {
					cl2 = 1;
				}
			}
			
			c++;
		}
	}

	if (ee1 && ee2) {
		int i, j;
		BMVert *v1, *v2, *v3, *v4;
		int starti = 0, dir1 = 1, wdir = 0, lenv1, lenv2;

		/* Simplify code below by avoiding the (!cl1 && cl2) case */
		if (!cl1 && cl2) {
			SWAP(int, cl1, cl2);
			ARRAY_SWAP(BMVert *, vv1, vv2);
			ARRAY_SWAP(BMEdge *, ee1, ee2);
		}

		lenv1 = lenv2 = BLI_array_count(vv1);

		/* Below code assumes vv1/vv2 each have at least two verts. should always be
		 * a safe assumption, since ee1/ee2 are non-empty and an edge has two verts. */
		BLI_assert((lenv1 > 1) && (lenv2 > 1));

		/* BMESH_TODO: Would be nice to handle cases where the edge loops
		 * have different edge counts by generating triangles & quads for
		 * the bridge instead of quads only. */
		if (BLI_array_count(ee1) != BLI_array_count(ee2)) {
			BMO_error_raise(bm, op, BMERR_INVALID_SELECTION,
			                "Selected loops must have equal edge counts");
			goto cleanup;
		}

		if (vv1[0] == vv1[lenv1 - 1]) {
			lenv1--;
		}
		if (vv2[0] == vv2[lenv2 - 1]) {
			lenv2--;
		}

		/* Find starting point and winding direction for two unclosed loops */
		if (!cl1 && !cl2) {
			/* First point of loop 1 */
			v1 = get_outer_vert(bm, ee1[0]);
			/* Last point of loop 1 */
			v2 = get_outer_vert(bm, ee1[clamp_index(-1, BLI_array_count(ee1))]);
			/* First point of loop 2 */
			v3 = get_outer_vert(bm, ee2[0]);
			/* Last point of loop 2 */
			v4 = get_outer_vert(bm, ee2[clamp_index(-1, BLI_array_count(ee2))]);

			/* If v1 is a better match for v4 than v3, AND v2 is a better match
			 * for v3 than v4, the loops are in opposite directions, so reverse
			 * the order of reads from vv1. We can avoid sqrt for comparison */
			if (len_squared_v3v3(v1->co, v3->co) > len_squared_v3v3(v1->co, v4->co) &&
			    len_squared_v3v3(v2->co, v4->co) > len_squared_v3v3(v2->co, v3->co))
			{
				dir1 = -1;
				starti = clamp_index(-1, lenv1);
			}
		}

		/* Find the smallest sum of distances from verts in vv1 to verts in vv2,
		 * finding a starting point in the first loop, to start with vv2[0] in the
		 * second loop. This is a simplistic attempt to get a better edge-to-edge
		 * match between two loops. */
		if (cl1) {
			float min = 1e32;

			for (i = 0; i < lenv1; i++) {
				float len;

				/* compute summed length between vertices in forward direction */
				len = 0.0f;
				for (j = 0; j < lenv2; j++) {
					len += len_v3v3(vv1[clamp_index(i + j, lenv1)]->co, vv2[j]->co);
				}

				if (len < min) {
					min = len;
					starti = i;
				}

				/* compute summed length between vertices in backward direction */
				len = 0.0f;
				for (j = 0; j < lenv2; j++) {
					len += len_v3v3(vv1[clamp_index(i - j, lenv1)]->co, vv2[j]->co);
				}

				if (len < min) {
					min = len;
					starti = i;
					dir1 = -1;
				}
			}
		}

		/* Vert rough attempt to determine proper winding for the bridge quads:
		 * just uses the first loop it finds for any of the edges of ee2 or ee1 */
		if (wdir == 0) {
			for (i = 0; i < BLI_array_count(ee2); i++) {
				if (ee2[i]->l) {
					wdir = (ee2[i]->l->v == vv2[i]) ? (-1) : (1);
					break;
				}
			}
		}
		if (wdir == 0) {
			for (i = 0; i < BLI_array_count(ee1); i++) {
				j = clamp_index((i * dir1) + starti, BLI_array_count(ee1));
				if (ee1[j]->l && ee2[j]->l) {
					wdir = (ee2[j]->l->v == vv2[j]) ? (1) : (-1);
					break;
				}
			}
		}
		
		/* merge loops of bridge faces */
		if (use_merge) {
			const int vert_len = mini(BLI_array_count(vv1), BLI_array_count(vv2)) - ((cl1 || cl2) ? 1 : 0);
			const int edge_len = mini(BLI_array_count(ee1), BLI_array_count(ee2));

			if (merge_factor <= 0.0f) {
				/* 2 --> 1 */
				for (i = 0; i < vert_len; i++) {
					BM_vert_splice(bm, vv2[i], vv1[i]);
				}
				for (i = 0; i < edge_len; i++) {
					BM_edge_splice(bm, ee2[i], ee1[i]);
				}
			}
			else if (merge_factor >= 1.0f) {
				/* 1 --> 2 */
				for (i = 0; i < vert_len; i++) {
					BM_vert_splice(bm, vv1[i], vv2[i]);
				}
				for (i = 0; i < edge_len; i++) {
					BM_edge_splice(bm, ee1[i], ee2[i]);
				}
			}
			else {
				/* mid factor, be tricky */
				/* 1 --> 2 */
				for (i = 0; i < vert_len; i++) {
					BM_data_interp_from_verts(bm, vv1[i], vv2[i], vv2[i], merge_factor);
					interp_v3_v3v3(vv2[i]->co, vv1[i]->co, vv2[i]->co, merge_factor);
					BM_elem_flag_merge(vv1[i], vv2[i]);
					BM_vert_splice(bm, vv1[i], vv2[i]);
				}
				for (i = 0; i < edge_len; i++) {
					BM_data_interp_from_edges(bm, ee1[i], ee2[i], ee2[i], merge_factor);
					BM_elem_flag_merge(ee1[i], ee2[i]);
					BM_edge_splice(bm, ee1[i], ee2[i]);
				}
			}
		}
		else {
			/* Generate the bridge quads */
			for (i = 0; i < BLI_array_count(ee1) && i < BLI_array_count(ee2); i++) {
				BMFace *f;

				BMLoop *l_1 = NULL;
				BMLoop *l_2 = NULL;
				BMLoop *l_1_next = NULL;
				BMLoop *l_2_next = NULL;
				BMLoop *l_iter;
				BMFace *f_example;

				int i1, i1next, i2, i2next;

				i1 = clamp_index(i * dir1 + starti, lenv1);
				i1next = clamp_index((i + 1) * dir1 + starti, lenv1);
				i2 = i;
				i2next = clamp_index(i + 1, lenv2);

				if (vv1[i1] == vv1[i1next]) {
					continue;
				}

				if (wdir < 0) {
					SWAP(int, i1, i1next);
					SWAP(int, i2, i2next);
				}

				/* get loop data - before making the face */
				bm_vert_loop_pair(bm, vv1[i1], vv2[i2], &l_1, &l_2);
				bm_vert_loop_pair(bm, vv1[i1next], vv2[i2next], &l_1_next, &l_2_next);
				/* copy if loop data if its is missing on one ring */
				if (l_1 && l_1_next == NULL) l_1_next = l_1;
				if (l_1_next && l_1 == NULL) l_1 = l_1_next;
				if (l_2 && l_2_next == NULL) l_2_next = l_2;
				if (l_2_next && l_2 == NULL) l_2 = l_2_next;
				f_example = l_1 ? l_1->f : (l_2 ? l_2->f : NULL);

				f = BM_face_create_quad_tri(bm,
				                            vv1[i1],
				                            vv2[i2],
				                            vv2[i2next],
				                            vv1[i1next],
				                            f_example, TRUE);
				if (UNLIKELY((f == NULL) || (f->len != 4))) {
					fprintf(stderr, "%s: in bridge! (bmesh internal error)\n", __func__);
				}
				else {
					BMO_elem_flag_enable(bm, f, FACE_OUT);

					l_iter = BM_FACE_FIRST_LOOP(f);

					if (l_1)      BM_elem_attrs_copy(bm, bm, l_1,      l_iter); l_iter = l_iter->next;
					if (l_2)      BM_elem_attrs_copy(bm, bm, l_2,      l_iter); l_iter = l_iter->next;
					if (l_2_next) BM_elem_attrs_copy(bm, bm, l_2_next, l_iter); l_iter = l_iter->next;
					if (l_1_next) BM_elem_attrs_copy(bm, bm, l_1_next, l_iter);
				}
			}
		}
	}

	BMO_slot_buffer_from_enabled_flag(bm, op, "faceout", BM_FACE, FACE_OUT);

cleanup:
	BLI_array_free(ee1);
	BLI_array_free(ee2);
	BLI_array_free(vv1);
	BLI_array_free(vv2);
}
