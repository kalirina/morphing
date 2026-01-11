#include "morphing.h"

// Copie un POINT en allouant de la mémoire sur le tas et renvoie le pointeur.
static POINT *copy_point(POINT p) {
	POINT *q = (POINT*)malloc(sizeof(POINT));
	if (q) *q = p;
	return q;
}

// Calcule l'aire signée (2 * area) du triangle (p1,p2,p3). Utile pour tests
// d'orientation et inclusion dans un triangle.
static double signed_area(const POINT *p1, const POINT *p2, const POINT *p3) {
	return ((double)(p1->x) - p3->x) * ((double)(p2->y) - p3->y)
		 - ((double)(p2->x) - p3->x) * ((double)(p1->y) - p3->y);
}


// Teste si le point P est à l'intérieur (ou sur la bordure) du triangle
// défini par A,B,C. Utilise les aires signées pour vérifier la position
// relative aux trois arêtes.
static int point_in_triangle(const POINT *P, const POINT *A, const POINT *B, const POINT *C) {
	double d1 = signed_area(P, A, B);
	double d2 = signed_area(P, B, C);
	double d3 = signed_area(P, C, A);
	/* marqueurs pour présence de signes opposés */
	int has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
	int has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
	return !(has_neg && has_pos);
}

// Remplit `paire[0..3]` avec les coins des images source et destination,
// dans l'ordre horaire: top-left, top-right, bottom-right, bottom-left.
static void fill_corners(MORPH* data, COUPLE *paire) {

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

// Libère les points alloués et le tableau de triangles contenus dans `t`.
void free_TRI(TRI *t) {
	if (!t) return;
	if (t->allocated_points) {
		for (int i = 0; i < t->alloc_count; i++) {
			/* libère chaque POINT alloué */
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

// Crée les 4 premiers triangles entourant le point d'indice 4 (convention:
// indices 0..3 = coins, 4 = premier point intérieur). Remplit result->triangles
// en incrémentant result->count.
static void create_initial_triangles(TRI *result, POINT **pts) {
	int p5 = 4;
	result->triangles[result->count++] = (TRIANGLE){ pts[p5], pts[0], pts[1] };
	result->triangles[result->count++] = (TRIANGLE){ pts[p5], pts[1], pts[2] };
	result->triangles[result->count++] = (TRIANGLE){ pts[p5], pts[2], pts[3] };
	result->triangles[result->count++] = (TRIANGLE){ pts[p5], pts[3], pts[0] };
}

// Cherche un triangle contenant P; si trouvé, remplace ce triangle par
// trois nouveaux triangles qui incluent P. Retourne 1 si inséré, 0 sinon.
static int insert_point_into_triangulation(TRI *result, POINT *P) {
	for (int t = 0; t < result->count; t++) {
		POINT *A = result->triangles[t].a;
		POINT *B = result->triangles[t].b;
		POINT *C = result->triangles[t].c;
		if (!A || !B || !C) continue;
		/* Si P est à l'intérieur du triangle courant, on "splite" ce
		   triangle: on le remplace par trois nouveaux triangles partageant P. */
		if (point_in_triangle(P, A, B, C)) {
			/* remplacer le triangle courant par le dernier, puis ajouter 3 nouveaux */
			result->triangles[t] = result->triangles[result->count - 1];
			result->count--;
			result->triangles[result->count++] = (TRIANGLE){ A, B, P };
			result->triangles[result->count++] = (TRIANGLE){ B, C, P };
			result->triangles[result->count++] = (TRIANGLE){ C, A, P };
			return 1;
		}
	}
	return 0;
}


// Si aucun triangle n'a contenu P, ajoute un petit fan vers le coin 0
// (stratégie de repli conservant la connectivité minimale). Ne change pas
// la logique générale de l'algorithme initial.
static void fallback_add_fan(TRI *result, POINT *P, POINT **pts, int expected) {
	if (result->count + 2 <= expected) {
		result->triangles[result->count++] = (TRIANGLE){ pts[0], pts[1], P };
		result->triangles[result->count++] = (TRIANGLE){ pts[0], P, pts[3] };
	}
}


// Fonction principale qui construit une triangulation à partir d'un tableau
// de POINT (pts_values[0..nb_pts-1]). Implémente l'algorithme d'insertion
// incrémentale décrit initialement. Retourne un TRI avec ownership des
// points alloués et du tableau de triangles (utiliser free_TRI pour libérer).
static TRI triangulation_from_points(POINT *pts_values, int nb_pts) {
	TRI result;
	result.triangles = NULL;
	result.count = 0;
	result.allocated_points = NULL;
	result.alloc_count = 0;

	if (nb_pts < 5) return result;

	/* copier les points d'entrée vers des POINT* alloués (on en prendra la
	   possession et on les libérera dans free_TRI) */
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

	/* créer les 4 triangles initiaux autour du point d'indice 4 */
	create_initial_triangles(&result, pts);

	/* insérer les points restants */
	for (int idx = 5; idx < nb_pts; idx++) {
		POINT *P = pts[idx];
		int inserted = insert_point_into_triangulation(&result, P);
		if (!inserted) {
			fallback_add_fan(&result, P, pts, expected);
		}
	}

	return result;
}

// Calcule la triangulation pour un frame donné en interpolant les coins
// et points sélectionnés (nb_pts_select). Retourne un TRI propriétaire.
TRI triangulate_frame(MORPH *data, int frame_index, int nb_pts_select, COUPLE *dots) {
	TRI empty = {.triangles = NULL, .count = 0, .allocated_points = NULL, .alloc_count = 0};
	if (!data || data->n <= 0) return empty;
	if (frame_index < 0 || frame_index >= data->n) return empty;

	COUPLE corner_pairs[4];
	fill_corners(data, corner_pairs);

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
