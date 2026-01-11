#include "../includes/morphing.h"

// Échantillonnage nearest-neighbor dans une image aux coordonnées flottantes
static COULEUR sample_image_nn(const IMAGE *img, float x, float y) {
	// arrondir aux entiers les plus proches
	int ix = (int)roundf(x);
	int iy = (int)roundf(y);
	// verifier les bornes
	if (ix < 0) ix = 0;
	if (iy < 0) iy = 0;
	if (ix >= img->larg) ix = img->larg - 1;
	if (iy >= img->haut) iy = img->haut - 1;
	// retourner la couleur
	return img->P[iy][ix];
}

// Calcule les coordonnées barycentriques (lambda, mu) du point P par rapport au triangle ABC
// Pour apres tester si le point P est à l'intérieur du triangle (A,B,C)
static int point_in_triangle_barycentric(const POINT *A, const POINT *B, const POINT *C, POINT P, float *out_lambda, float *out_mu) {
	// calcul des vecteurs
	float v0x = B->x - A->x;
	float v0y = B->y - A->y;
	float v1x = C->x - A->x;
	float v1y = C->y - A->y;
	float v2x = P.x - A->x;
	float v2y = P.y - A->y;
	// calcul des coordonnées barycentriques
	float det = v0x * v1y - v1x * v0y;
	if (fabsf(det) < 1e-6f) return 0; // triangle dégénéré (det proche de 0)
	*out_lambda = (v2x * v1y - v1x * v2y) / det;
	*out_mu = (v0x * v2y - v2x * v0y) / det;
	return 1;
}

// Trouve les indices des POINT* A, B, C dans le tableau alloc
static int find_alloc_indices(POINT **alloc, int alloc_count, const POINT *A, const POINT *B, const POINT *C, int *ia, int *ib, int *ic) {
	// initialiser les indices à -1 (non trouvés)
	*ia = *ib = *ic = -1;
	for (int j = 0; j < alloc_count; j++) {
		// comparer les pointeurs
		if (alloc[j] == A) *ia = j;
		if (alloc[j] == B) *ib = j;
		if (alloc[j] == C) *ic = j;
		// si tous les 3 trouvés, on peut arrêter
		if (*ia != -1 && *ib != -1 && *ic != -1) break;
	}
	return (*ia != -1 && *ib != -1 && *ic != -1);
}

// Calcule la couleur du pixel P en utilisant le triangle ABC et les images source/destination
static int shade_pixel_with_triangle(MORPH *data, POINT P, POINT *A, POINT *B, POINT *C, COUPLE *pairs, float alpha, COULEUR *out_col, POINT **alloc, int alloc_count) {
	// calcul des coordonnées barycentriques
	float lambda, mu;
	if (!point_in_triangle_barycentric(A, B, C, P, &lambda, &mu)) return 0;
	if (lambda < -1e-6f || mu < -1e-6f || (lambda + mu) > 1.0f + 1e-6f) return 0;

	// trouver les indices des points A, B, C dans le tableau alloc
	int ia, ib, ic;
	if (!find_alloc_indices(alloc, alloc_count, A, B, C, &ia, &ib, &ic)) return 0;

	// recuperer les positions dans les images source et destination
	POINT Asrc = pairs[ia].g;
	POINT Bsrc = pairs[ib].g;
	POINT Csrc = pairs[ic].g;
	POINT Adst = pairs[ia].d;
	POINT Bdst = pairs[ib].d;
	POINT Cdst = pairs[ic].d;

	// Projection du pixel dans les deux images
	// PD = Asrc + λ(Bsrc − Asrc) + μ(Csrc − Asrc)
	// PA = Adst + λ(Bdst − Adst) + μ(Cdst − Adst)

	float PDx = Asrc.x + lambda * (Bsrc.x - Asrc.x) + mu * (Csrc.x - Asrc.x);
	float PDy = Asrc.y + lambda * (Bsrc.y - Asrc.y) + mu * (Csrc.y - Asrc.y);
	float PAx = Adst.x + lambda * (Bdst.x - Adst.x) + mu * (Cdst.x - Adst.x);
	float PAy = Adst.y + lambda * (Bdst.y - Adst.y) + mu * (Cdst.y - Adst.y);
	//On trouve où se situe P dans : l’image de départ ou l’image d’arrivée

	// On prend la couleur du pixel le plus proche
	COULEUR colD = sample_image_nn(&data->img_depart, PDx, PDy);
	COULEUR colA = sample_image_nn(&data->img_arrive, PAx, PAy);

	int rD = R(colD), gD = G(colD), bD = B(colD);
	int rA = R(colA), gA = G(colA), bA = B(colA);
	// melange des couleurs - morphing !
	// final = (1 − alpha) * départ + alpha * arrivée
	int rf = (int)roundf((1.0f - alpha) * rD + alpha * rA);
	int gf = (int)roundf((1.0f - alpha) * gD + alpha * gA);
	int bf = (int)roundf((1.0f - alpha) * bD + alpha * bA);
	// clamp entre 0 et 255
	if (rf < 0) rf = 0;
	if (rf > 255) rf = 255;
	if (gf < 0) gf = 0;
	if (gf > 255) gf = 255;
	if (bf < 0) bf = 0;
	if (bf > 255) bf = 255;
	// retourner la couleur finale
	*out_col = (rf << 16) | (gf << 8) | bf;
	return 1;
}

// Construit l'image intermédiaire pour le frame `frame_index` en utilisant la triangulation `tri`
IMAGE build_intermediate_image(MORPH *data, TRI *tri, int frame_index, COUPLE *pairs) {
	IMAGE I;
	float a = (float)frame_index / (float)data->n;
	// dimensions interpolées
	I.larg = (int)roundf((1.0f - a) * data->img_depart.larg + a * data->img_arrive.larg);
	I.haut = (int)roundf((1.0f - a) * data->img_depart.haut + a * data->img_arrive.haut);
	I.range = 255;
	allouer_pixel(&I);

	// pour chaque pixel de l'image intermédiaire
	POINT **alloc = tri->allocated_points;
	int alloc_count = tri->alloc_count;

	for (int y = 0; y < I.haut; y++) {
		for (int x = 0; x < I.larg; x++) {
			POINT P = { x, y };
			int found = 0;
			// tester chaque triangle pour voir si P est à l'intérieur
			for (int t = 0; t < tri->count; t++) {
				POINT *A = tri->triangles[t].a;
				POINT *B = tri->triangles[t].b;
				POINT *C = tri->triangles[t].c;
				if (!A || !B || !C) continue;
				COULEUR col;
				// si P est à l'intérieur du triangle, calculer la couleur
				if (shade_pixel_with_triangle(data, P, A, B, C, pairs, a, &col, alloc, alloc_count)) {
					I.P[y][x] = col;
					found = 1;
					break;
				}
			}
			if (!found) I.P[y][x] = 0;
		}
	}

	return I;
}

// Teste si le point p est dans le rectangle défini par les coins tl (top-left) et br (bottom-right)
int point_in_rect(POINT p, POINT tl, POINT br) {
	return (p.x >= tl.x && p.x <= br.x && p.y >= tl.y && p.y <= br.y);
}

// Crée une triangulation interpolée pour le frame `f` à partir de la triangulation fixe `fixed_tri`
TRI create_interpolated_tri_from_fixed(const TRI *fixed_tri, MORPH *data, int f, int M, COUPLE *paires) {
	TRI tri;
	tri.count = fixed_tri->count;
	tri.triangles = (TRIANGLE*)malloc(sizeof(TRIANGLE) * tri.count);
	tri.alloc_count = M;
	tri.allocated_points = (POINT**)malloc(sizeof(POINT*) * M);
	for (int i = 0; i < M; i++)
		tri.allocated_points[i] = NULL;
	// calculer le facteur d'interpolation
	float af = (float)f / (float)data->n;
	// remplir les points interpolés
	for (int i = 0; i < M; i++) {
		POINT p;
		if (i < 4) { // coins
			POINT g, d;
			if (i == 0) { g.x = 0; g.y = 0; d.x = 0; d.y = 0; }
			else if (i == 1) { g.x = data->img_depart.larg; g.y = 0; d.x = data->img_arrive.larg; d.y = 0; }
			else if (i == 2) { g.x = data->img_depart.larg; g.y = data->img_depart.haut; d.x = data->img_arrive.larg; d.y = data->img_arrive.haut; }
			else { g.x = 0; g.y = data->img_depart.haut; d.x = 0; d.y = data->img_arrive.haut; }
			// interpoler
			p.x = (int)roundf((1.0f - af) * g.x + af * d.x);
			p.y = (int)roundf((1.0f - af) * g.y + af * d.y);
		} else { // points sélectionnés
			POINT g = paires[i-4].g; POINT d = paires[i-4].d;
			// interpoler
			p.x = (int)roundf((1.0f - af) * g.x + af * d.x);
			p.y = (int)roundf((1.0f - af) * g.y + af * d.y);
		}
		// allouer et stocker le point
		tri.allocated_points[i] = (POINT*)malloc(sizeof(POINT));
		*tri.allocated_points[i] = p;
	}
	// remplir les triangles en référençant les nouveaux points
	for (int t = 0; t < fixed_tri->count; t++) {
		// retrouver les indices des points originaux
		POINT *origA = fixed_tri->triangles[t].a;
		POINT *origB = fixed_tri->triangles[t].b;
		POINT *origC = fixed_tri->triangles[t].c;
		int ia=-1, ib=-1, ic=-1;
		// trouver les indices dans allocated_points
		for (int j = 0; j < fixed_tri->alloc_count; j++) {
			if (fixed_tri->allocated_points[j] == origA) ia = j;
			if (fixed_tri->allocated_points[j] == origB) ib = j;
			if (fixed_tri->allocated_points[j] == origC) ic = j;
		}
		// assigner les nouveaux points aux triangles
		tri.triangles[t].a = tri.allocated_points[ia];
		tri.triangles[t].b = tri.allocated_points[ib];
		tri.triangles[t].c = tri.allocated_points[ic];
	}
	return tri;
}
