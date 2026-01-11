#include "morphing.h"
#include <math.h>

/*
peut etre une autre structure pour la triangulation

plus grand est N = plus d'images = plus fluide est la transformation


steps pour faire i-eme image:
a = i/N
1.Calculer les points de base de l’image intermédiaire
	i_x = (1 - a)*d_x + a*a_x;
	i_y = (1 - a)*d_y + a*a_y;
les points sont : les 4 ongles + les points selectiones
min 1 point selectione
2.Trianguler l’image intermédiaire
ensemble des triangles
array of TRIANGLE
triangle = point i >= 5 , points des ongles plus proches

*/

static POINT *copy_point(POINT p) {
	POINT *q = (POINT*)malloc(sizeof(POINT));
	if (q) *q = p;
	return q;
}

/* helper: signed area (twice area) of triangle (p1,p2,p3) */
static double signed_area(const POINT *p1, const POINT *p2, const POINT *p3) {
	return ((double)(p1->x) - p3->x) * ((double)(p2->y) - p3->y) - ((double)(p2->x) - p3->x) * ((double)(p1->y) - p3->y);
}

/* point-in-triangle inclusive of edges, P,A,B,C passed by pointer */
static int point_in_triangle(const POINT *P, const POINT *A, const POINT *B, const POINT *C) {
	double d1 = signed_area(P, A, B);
	double d2 = signed_area(P, B, C);
	double d3 = signed_area(P, C, A);
	int has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
	int has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
	return !(has_neg && has_pos);
}

static void fill_corners(MORPH* data, TRI* base, COUPLE *paire) {
	(void)base; // unused for now

	/* Fill corners in clockwise order: top-left, top-right, bottom-right, bottom-left
	   This adjacency order is used by the incremental insertion algorithm. */
	POINT tl_src = {0, 0};
	POINT tl_dst = {0, 0};
	paire[0].g = tl_src; paire[0].d = tl_dst;

	POINT tr_src = { data->img_depart.larg, 0 };
	POINT tr_dst = { data->img_arrive.larg, 0 };
	paire[1].g = tr_src; paire[1].d = tr_dst;

	POINT br_src = { data->img_depart.larg, data->img_depart.haut };
	POINT br_dst = { data->img_arrive.larg, data->img_arrive.haut };
	paire[2].g = br_src; paire[2].d = br_dst;

	POINT bl_src = { 0, data->img_depart.haut };
	POINT bl_dst = { 0, data->img_arrive.haut };
	paire[3].g = bl_src; paire[3].d = bl_dst;
}

TRI triangulation(MORPH *data, int nb_pts_select, COUPLE *dots) {
	/*
	  Implement incremental insertion triangulation as specified:
	  - first 4 points are the rectangle corners (in clockwise order)
	  - 5th point produces 4 triangles (with each adjacent corner pair)
	  - for each subsequent point P: find triangle (A,B,C) that contains P,
		remove that triangle and add (A,B,P),(B,C,P),(C,A,P)
	  - final triangle count = 2*N - 6
	*/

	TRI result;
	result.triangles = NULL;
	result.count = 0;
	result.allocated_points = NULL;
	result.alloc_count = 0;

	/* Get corners */
	COUPLE corners[4];
	fill_corners(data, &result, corners);

	int N = nb_pts_select + 4;
	if (N < 5) {
		// need at least 5 points to run this algorithm
		return result;
	}

	/* Allocate and copy input points onto heap; store pointers in pts[].
	   pts[0..3] are corners, pts[4..] are user-selected points (in provided order). */
	POINT **pts = (POINT**)malloc(sizeof(POINT*) * N);
	if (!pts) return result;
	pts[0] = copy_point(corners[0].g);
	pts[1] = copy_point(corners[1].g);
	pts[2] = copy_point(corners[2].g);
	pts[3] = copy_point(corners[3].g);
	for (int i = 0; i < nb_pts_select; i++) {
		pts[4 + i] = copy_point(dots[i].g);
	}

	/* If any allocation failed, clean up and return empty result */
	for (int i = 0; i < N; i++) {
		if (!pts[i]) {
			for (int j = 0; j < N; j++) if (pts[j]) free(pts[j]);
			free(pts);
			return result;
		}
	}

	/* allocate triangle array with expected size 2N - 6 */
	int expected = 2 * N - 6;
	result.triangles = (TRIANGLE*)malloc(sizeof(TRIANGLE) * expected);
	if (!result.triangles) {
		for (int i = 0; i < N; i++) free(pts[i]);
		free(pts);
		return result;
	}
	result.count = 0;
	result.allocated_points = pts;
	result.alloc_count = N;

	/* (helpers are defined at top of file) */

	/* 5th point (index 4): create 4 triangles with adjacent corner pairs */
	int p5 = 4;
	result.triangles[result.count++] = (TRIANGLE){ pts[p5], pts[0], pts[1] };
	result.triangles[result.count++] = (TRIANGLE){ pts[p5], pts[1], pts[2] };
	result.triangles[result.count++] = (TRIANGLE){ pts[p5], pts[2], pts[3] };
	result.triangles[result.count++] = (TRIANGLE){ pts[p5], pts[3], pts[0] };

	/* Insert remaining points one by one */
	for (int idx = 5; idx < N; idx++) {
		POINT *P = pts[idx];
		int inserted = 0;
		for (int t = 0; t < result.count; t++) {
			POINT *A = result.triangles[t].a;
			POINT *B = result.triangles[t].b;
			POINT *C = result.triangles[t].c;
			if (!A || !B || !C) continue;
			if (point_in_triangle(P, A, B, C)) {
				/* remove triangle t by swapping with last */
				result.triangles[t] = result.triangles[result.count - 1];
				result.count--;
				/* add three new triangles (A,B,P),(B,C,P),(C,A,P) */
				result.triangles[result.count++] = (TRIANGLE){ A, B, P };
				result.triangles[result.count++] = (TRIANGLE){ B, C, P };
				result.triangles[result.count++] = (TRIANGLE){ C, A, P };
				inserted = 1;
				break;
			}
		}
		if (!inserted) {
			/* Fallback if not found (numerical issue): attach P with a small fan to corner 0 */
			if (result.count + 2 <= expected) {
				result.triangles[result.count++] = (TRIANGLE){ pts[0], pts[1], P };
				result.triangles[result.count++] = (TRIANGLE){ pts[0], P, pts[3] };
			}
		}
	}

	return result;
}

/* free_TRI: free allocated POINTs and triangle array stored in TRI */
void free_TRI(TRI *t) {
	if (!t) return;
	if (t->allocated_points) {
		for (int i = 0; i < t->alloc_count; i++) {
			if (t->allocated_points[i]) free(t->allocated_points[i]);
		}
		free(t->allocated_points);
		t->allocated_points = NULL;
		t->alloc_count = 0;
	}
	if (t->triangles) {
		free(t->triangles);
		t->triangles = NULL;
	}
	t->count = 0;
}

/* Build a TRI from an array of POINT values (pts[0..nb_pts-1]).
   This follows the incremental insertion algorithm described by the user.
   The function allocates POINT copies and triangles inside the returned TRI; use free_TRI() to free. */
static TRI triangulation_from_points(POINT *pts_values, int nb_pts) {
	TRI result;
	result.triangles = NULL;
	result.count = 0;
	result.allocated_points = NULL;
	result.alloc_count = 0;

	if (nb_pts < 5) return result;

	/* copy input points to heap-allocated POINT* array (we will own them) */
	POINT **pts = (POINT**)malloc(sizeof(POINT*) * nb_pts);
	if (!pts) return result;
	for (int i = 0; i < nb_pts; i++) {
		pts[i] = copy_point(pts_values[i]);
		if (!pts[i]) {
			for (int j = 0; j < i; j++) free(pts[j]);
			free(pts);
			return result;
		}
	}

	int expected = 2 * nb_pts - 6;
	result.triangles = (TRIANGLE*)malloc(sizeof(TRIANGLE) * expected);
	if (!result.triangles) {
		for (int i = 0; i < nb_pts; i++) free(pts[i]);
		free(pts);
		return result;
	}
	result.count = 0;
	result.allocated_points = pts;
	result.alloc_count = nb_pts;

	/* Create initial 4 triangles using point 4 and corners 0..3 */
	int p5 = 4;
	result.triangles[result.count++] = (TRIANGLE){ pts[p5], pts[0], pts[1] };
	result.triangles[result.count++] = (TRIANGLE){ pts[p5], pts[1], pts[2] };
	result.triangles[result.count++] = (TRIANGLE){ pts[p5], pts[2], pts[3] };
	result.triangles[result.count++] = (TRIANGLE){ pts[p5], pts[3], pts[0] };

	/* Insert remaining points */
	for (int idx = 5; idx < nb_pts; idx++) {
		POINT *P = pts[idx];
		int inserted = 0;
		for (int t = 0; t < result.count; t++) {
			POINT *A = result.triangles[t].a;
			POINT *B = result.triangles[t].b;
			POINT *C = result.triangles[t].c;
			if (!A || !B || !C) continue;
			if (point_in_triangle(P, A, B, C)) {
				/* remove triangle t */
				result.triangles[t] = result.triangles[result.count - 1];
				result.count--;
				/* add new triangles */
				result.triangles[result.count++] = (TRIANGLE){ A, B, P };
				result.triangles[result.count++] = (TRIANGLE){ B, C, P };
				result.triangles[result.count++] = (TRIANGLE){ C, A, P };
				inserted = 1;
				break;
			}
		}
		if (!inserted) {
			/* fallback: simple fan to corner 0 */
			if (result.count + 2 <= expected) {
				result.triangles[result.count++] = (TRIANGLE){ pts[0], pts[1], P };
				result.triangles[result.count++] = (TRIANGLE){ pts[0], P, pts[3] };
			}
		}
	}

	return result;
}

/* Triangulate every intermediate frame. Returns an allocated array of TRI of length data->n.
   Caller must call free_TRI on each TRI and then free the returned array, or use free_TRI_array(). */
TRI *triangulate_frames(MORPH *data, int nb_pts_select, COUPLE *dots) {
	if (!data || data->n <= 0) return NULL;
	int frames = data->n;
	TRI *arr = (TRI*)malloc(sizeof(TRI) * frames);
	if (!arr) return NULL;

	COUPLE corner_pairs[4];
	fill_corners(data, NULL, corner_pairs);

	int M = nb_pts_select + 4;
	POINT *frame_pts = (POINT*)malloc(sizeof(POINT) * M);
	if (!frame_pts) { free(arr); return NULL; }

	for (int f = 0; f < frames; f++) {
		float a = (float)f / (float)frames; // interpolation factor
		/* interpolate corners */
		for (int c = 0; c < 4; c++) {
			frame_pts[c].x = (int)round((1.0f - a) * corner_pairs[c].g.x + a * corner_pairs[c].d.x);
			frame_pts[c].y = (int)round((1.0f - a) * corner_pairs[c].g.y + a * corner_pairs[c].d.y);
		}
		/* interpolate selected points */
		for (int i = 0; i < nb_pts_select; i++) {
			POINT sg = dots[i].g;
			POINT sd = dots[i].d;
			frame_pts[4 + i].x = (int)round((1.0f - a) * sg.x + a * sd.x);
			frame_pts[4 + i].y = (int)round((1.0f - a) * sg.y + a * sd.y);
		}

		arr[f] = triangulation_from_points(frame_pts, M);
	}

	free(frame_pts);
	return arr;
}

/* Free an array of TRI produced by triangulate_frames */
void free_TRI_array(TRI *arr, int frames) {
	if (!arr) return;
	for (int i = 0; i < frames; i++) free_TRI(&arr[i]);
	free(arr);
}

/* Triangulate a single frame index (0..data->n-1).
   Returns an owned TRI that must be freed with free_TRI(). */
TRI triangulate_frame(MORPH *data, int frame_index, int nb_pts_select, COUPLE *dots) {
	TRI empty = {.triangles = NULL, .count = 0, .allocated_points = NULL, .alloc_count = 0};
	if (!data || data->n <= 0) return empty;
	if (frame_index < 0 || frame_index >= data->n) return empty;

	COUPLE corner_pairs[4];
	fill_corners(data, NULL, corner_pairs);

	int M = nb_pts_select + 4;
	POINT *frame_pts = (POINT*)malloc(sizeof(POINT) * M);
	if (!frame_pts) return empty;

	float a = (float)frame_index / (float)data->n;
	for (int c = 0; c < 4; c++) {
		frame_pts[c].x = (int)round((1.0f - a) * corner_pairs[c].g.x + a * corner_pairs[c].d.x);
		frame_pts[c].y = (int)round((1.0f - a) * corner_pairs[c].g.y + a * corner_pairs[c].d.y);
	}
	for (int i = 0; i < nb_pts_select; i++) {
		POINT sg = dots[i].g;
		POINT sd = dots[i].d;
		frame_pts[4 + i].x = (int)round((1.0f - a) * sg.x + a * sd.x);
		frame_pts[4 + i].y = (int)round((1.0f - a) * sg.y + a * sd.y);
	}

	TRI res = triangulation_from_points(frame_pts, M);
	free(frame_pts);
	return res;
}

