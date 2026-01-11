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
	// vérifier si tous les signes sont identiques (ou zéro)
	int has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
	int has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
	return !(has_neg && has_pos);
}

// Libère les points alloués et le tableau de triangles contenus dans `t`.
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
		// Si P est à l'intérieur du triangle courant, on "splite" ce
		// triangle: on le remplace par trois nouveaux triangles partageant P.
		if (point_in_triangle(P, A, B, C)) {
			// remplacer le triangle courant par le dernier, puis ajouter 3 nouveaux
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
// de POINT (pts_values[0..nb_pts-1]). Retourne un TRI avec ownership des
// points alloués et du tableau de triangles (utiliser free_TRI pour libérer).
static TRI triangulation_from_points(POINT *pts_values, int nb_pts) {
	TRI result;
	result.triangles = NULL;
	result.count = 0;
	result.allocated_points = NULL;
	result.alloc_count = 0;

	if (nb_pts < 5) return result;

	// allouer et copier les points
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

	// creer les 4 premiers triangles
	create_initial_triangles(&result, pts);

	// inserer les points restants
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
TRI triangulate_frame(MORPH *data, int frame_index) {
	TRI empty = {.triangles = NULL, .count = 0, .allocated_points = NULL, .alloc_count = 0};
	if (!data || data->n <= 0) return empty;
	if (frame_index < 0 || frame_index >= data->n) return empty;

	if (!data->all_points || data->total_points < 5) return empty;

	int M = data->total_points;
	POINT *frame_pts = (POINT*)malloc(sizeof(POINT) * M);
	if (!frame_pts) return empty;

	float a = (float)frame_index / (float)data->n;
	for (int i = 0; i < M; i++) {
		POINT g = data->all_points[i].g;
		POINT d = data->all_points[i].d;
		frame_pts[i].x = (int)round((1.0f - a) * g.x + a * d.x);
		frame_pts[i].y = (int)round((1.0f - a) * g.y + a * d.y);
	}

	TRI res = triangulation_from_points(frame_pts, M);
	free(frame_pts);
	return res;
}

// Construit un tableau de COUPLE incluant les coins des images
COUPLE *build_all_points(MORPH *data, COUPLE *paires) {
	data->total_points = data->nb_s_couples + 4;
	COUPLE *full_pairs = (COUPLE*)malloc(sizeof(COUPLE) * data->total_points);
	if (!full_pairs) return NULL;
	// ajouter les coins
	full_pairs[0].g.x = 0; full_pairs[0].g.y = 0; full_pairs[0].d.x = 0; full_pairs[0].d.y = 0;
	full_pairs[1].g.x = data->img_depart.larg; full_pairs[1].g.y = 0; full_pairs[1].d.x = data->img_arrive.larg; full_pairs[1].d.y = 0;
	full_pairs[2].g.x = data->img_depart.larg; full_pairs[2].g.y = data->img_depart.haut; full_pairs[2].d.x = data->img_arrive.larg; full_pairs[2].d.y = data->img_arrive.haut;
	full_pairs[3].g.x = 0; full_pairs[3].g.y = data->img_depart.haut; full_pairs[3].d.x = 0; full_pairs[3].d.y = data->img_arrive.haut;
	// ajouter les paires sélectionnées
	for (int i = 0; i < data->nb_s_couples; i++)
		full_pairs[4 + i] = paires[i];

	return full_pairs;
}

void triangulation(MORPH *data, POINT pts_droite[], POINT pts_gauche[]) {
	// Triangulation et génération des images intermédiaires
	COUPLE paires[data->nb_s_couples];
	for (int i = 0; i < data->nb_s_couples; i++) {
		// convertir en coordonnées locales
		paires[i].g.x = pts_gauche[i].x - data->img_depart.x0;
		paires[i].g.y = pts_gauche[i].y - data->img_depart.y0;
		paires[i].d.x = pts_droite[i].x - data->img_arrive.x0;
		paires[i].d.y = pts_droite[i].y - data->img_arrive.y0;
	}

	// Construire le tableau complet de paires incluant les coins
	data->all_points = build_all_points(data, paires);
	if (!data->all_points) return;

	// Trianguler une fois au milieu pour obtenir une triangulation fixe
	int mid = data->n / 2;
	TRI fixed_tri = triangulate_frame(data, mid);

	// Si la triangulation fixe est vide, générer chaque frame indépendamment
	if (fixed_tri.count == 0) {
		for (int f = 0; f < data->n; f++) {
			// Générer chaque frame indépendamment
			TRI tri = triangulate_frame(data, f);
			IMAGE I = build_intermediate_image(data, &tri, f, data->all_points);
			// Sauvegarder l'image
			char filename[256]; snprintf(filename, sizeof(filename), "images_transformed/intermediate_%d.ppm", f);
			ecrire_fichier(I, filename);
			free_TRI(&tri);
			for (int yy = 0; yy < I.haut; yy++) free(I.P[yy]);
			free(I.P);
		}
		free(data->all_points);
		wait_escape();
	}
	else {
		// Générer les images intermédiaires en utilisant la triangulation fixe
		//system("mkdir -p images_transformed");
		int M = data->nb_s_couples + 4;
		for (int f = 0; f < data->n; f++) {
			TRI tri = create_interpolated_tri_from_fixed(&fixed_tri, data, f, M, data->all_points + 4);
			IMAGE I = build_intermediate_image(data, &tri, f, data->all_points);
			// Sauvegarder l'image
			char filename[256]; snprintf(filename, sizeof(filename), "images_transformed/intermediate_%d.ppm", f);
			ecrire_fichier(I, filename);
			for (int yy = 0; yy < I.haut; yy++) free(I.P[yy]);
			free(I.P);
			free_TRI(&tri);
		}
	}

	free_TRI(&fixed_tri);
	free(data->all_points);
}
