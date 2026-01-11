#include "morphing.h"
#include <math.h>
#include <stdlib.h>

// Une fonction qui affiche une image dans la fenêtre graphique
void afficher_image(IMAGE I, int x0, int y0) {
	for (int i = 0; i < I.haut; i++) {
		for (int j = 0; j < I.larg; j++) {
			POINT p;
			p.x = x0 + j;
			p.y = y0 + i;
			draw_pixel(p, I.P[i][j]);
		}
	}
}

int clic_dans_gauche(POINT p, MORPH *data, int x, int y) {
	return (p.x >= x && p.x < x + data->img_depart.larg &&
			p.y >= y && p.y < y + data->img_depart.haut);
}

int clic_dans_droite(POINT p, MORPH *data, int x, int y) {
	return (p.x >= x && p.x < x + data->img_arrive.larg &&
			p.y >= y && p.y < y + data->img_arrive.haut);
}

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
static IMAGE build_intermediate_image(MORPH *data, TRI *tri, int frame_index, COUPLE *pairs) {
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

// Redessine la scène : les deux images et les points sélectionnés
static void redraw_scene(MORPH *data, int x_depart, int y_depart, int x_arrive, int y_arrive,
						 POINT pts_gauche[], POINT pts_droite[], int nb_couples, int attendre_gauche, POINT texte) {
	fill_screen(hotpink);
	afficher_image(data->img_depart, x_depart, y_depart);
	afficher_image(data->img_arrive, x_arrive, y_arrive);
	aff_pol_centre(
		attendre_gauche ?
		"Cliquez sur l'image de GAUCHE" :
		"Cliquez sur l'image de DROITE",
		30, texte, noir
	);

	// dessiner les points sélectionnés et leurs indices
	for (int i = 0; i < nb_couples; i++) {
		draw_circle(pts_gauche[i], 10, rouge);
		draw_circle(pts_droite[i], 10, rouge);
		POINT t1 = { pts_gauche[i].x + 14, pts_gauche[i].y + 14 };
		POINT t2 = { pts_droite[i].x + 14, pts_droite[i].y + 14 };
		aff_int(i + 1, 20, t1, rouge);
		aff_int(i + 1, 20, t2, rouge);
	}
	if (!attendre_gauche) draw_circle(pts_gauche[nb_couples], 10, rouge);
	/* Draw action buttons (Sauvegarder / Quitter) */
	POINT btn_save_tl = {1400, 980};
	POINT btn_save_br = {1550, 1030};
	POINT btn_quit_tl = {1560, 980};
	POINT btn_quit_br = {1710, 1030};
	draw_fill_rectangle(btn_save_tl, btn_save_br, vert);
	draw_fill_rectangle(btn_quit_tl, btn_quit_br, rouge);
	POINT center_save = { (btn_save_tl.x + btn_save_br.x)/2, (btn_save_tl.y + btn_save_br.y)/2 };
	POINT center_quit = { (btn_quit_tl.x + btn_quit_br.x)/2, (btn_quit_tl.y + btn_quit_br.y)/2 };
	aff_pol_centre("Sauvegarder", 20, center_save, noir);
	aff_pol_centre("Quitter", 20, center_quit, blanc);
	affiche_all();
}

// Construit un tableau de COUPLE incluant les coins des images
static COUPLE *build_full_pairs(MORPH *data, COUPLE *paires, int nb_couples, int *out_full_count) {
	int full_count = nb_couples + 4;
	COUPLE *full_pairs = (COUPLE*)malloc(sizeof(COUPLE) * full_count);
	if (!full_pairs) return NULL;
	// ajouter les coins
	full_pairs[0].g.x = 0; full_pairs[0].g.y = 0; full_pairs[0].d.x = 0; full_pairs[0].d.y = 0;
	full_pairs[1].g.x = data->img_depart.larg; full_pairs[1].g.y = 0; full_pairs[1].d.x = data->img_arrive.larg; full_pairs[1].d.y = 0;
	full_pairs[2].g.x = data->img_depart.larg; full_pairs[2].g.y = data->img_depart.haut; full_pairs[2].d.x = data->img_arrive.larg; full_pairs[2].d.y = data->img_arrive.haut;
	full_pairs[3].g.x = 0; full_pairs[3].g.y = data->img_depart.haut; full_pairs[3].d.x = 0; full_pairs[3].d.y = data->img_arrive.haut;
	// ajouter les paires sélectionnées
	for (int i = 0; i < nb_couples; i++)
		full_pairs[4 + i] = paires[i];
	*out_full_count = full_count;
	return full_pairs;
}

/* Helper: test if a point is inside rectangle defined by tl and br (inclusive) */
static int point_in_rect(POINT p, POINT tl, POINT br) {
	return (p.x >= tl.x && p.x <= br.x && p.y >= tl.y && p.y <= br.y);
}

// Crée une triangulation interpolée pour le frame `f` à partir de la triangulation fixe `fixed_tri`
static TRI create_interpolated_tri_from_fixed(const TRI *fixed_tri, MORPH *data, int f, int M, COUPLE *paires) {
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

// La fonction principale d'affichage et d'interaction
void afficher(MORPH *data) {
	init_graphics(1920, 1080);
	affiche_auto_off();
	fill_screen(hotpink);

	int x_depart = 50;
	int y_depart = 50;
	int x_arrive = 1920 - 50 - data->img_arrive.larg;
	int y_arrive = 50;
	afficher_image(data->img_depart, x_depart, y_depart);
	afficher_image(data->img_arrive, x_arrive, y_arrive);
	POINT texte;
	texte.x = 850;
	texte.y = 1010;
	aff_pol_centre("Cliquer sur l'image a gauche et a droite pour creer un couple des points",30,texte,noir);
	affiche_all();

	// maximum 50 couples (user can stop earlier via Sauvegarder)
	POINT pts_gauche[50];
	POINT pts_droite[50];
	int nb_couples = 0;
	int attendre_gauche = 1; // 1 = gauche, 0 = droite

	/* Button rectangles (consistent with redraw_scene) */
	POINT btn_save_tl = {1400, 980};
	POINT btn_save_br = {1550, 1030};
	POINT btn_quit_tl = {1560, 980};
	POINT btn_quit_br = {1710, 1030};

	int saved = 0;

	// draw initial scene (so buttons are visible immediately)
	redraw_scene(data, x_depart, y_depart, x_arrive, y_arrive, pts_gauche, pts_droite, nb_couples, attendre_gauche, texte);

	// boucle de sélection des points (peut être arrêtée par Sauvegarder ou Quitter)
	while (nb_couples < 50 && !saved) {
		POINT p = wait_clic();

		// check buttons first
		if (point_in_rect(p, btn_save_tl, btn_save_br)) {
			saved = 1; /* proceed */
			break;
		}
		if (point_in_rect(p, btn_quit_tl, btn_quit_br)) {
			// Exit gracefully without proceeding to triangulation
			fill_screen(hotpink);
			aff_pol_centre("Quitter: sortie sans sauvegarde", 24, texte, noir);
			affiche_all();
			wait_escape();
			return;
		}

		// normal image click handling
		if (attendre_gauche && clic_dans_gauche(p, data, x_depart, y_depart)) {
			pts_gauche[nb_couples] = p;
			attendre_gauche = 0; // maintenant on attend la droite
		}
		else if (!attendre_gauche && clic_dans_droite(p, data, x_arrive, y_arrive)) {
			pts_droite[nb_couples] = p;
			attendre_gauche = 1; // prochain couple
			nb_couples++;
		}
		else {
			continue; // clic ignoré
		}

		// Redessiner
		redraw_scene(data, x_depart, y_depart, x_arrive, y_arrive, pts_gauche, pts_droite, nb_couples, attendre_gauche, texte);
	}

	// Triangulation et génération des images intermédiaires
	COUPLE paires[nb_couples];
	for (int i = 0; i < nb_couples; i++) {
		// convertir en coordonnées locales
		paires[i].g.x = pts_gauche[i].x - x_depart;
		paires[i].g.y = pts_gauche[i].y - y_depart;
		paires[i].d.x = pts_droite[i].x - x_arrive;
		paires[i].d.y = pts_droite[i].y - y_arrive;
	}

	// Construire le tableau complet de paires incluant les coins
	int full_count = 0;
	COUPLE *full_pairs = build_full_pairs(data, paires, nb_couples, &full_count);
	if (!full_pairs) return;

	// Trianguler une fois au milieu pour obtenir une triangulation fixe
	int mid = data->n / 2;
	TRI fixed_tri = triangulate_frame(data, mid, nb_couples, paires);
	// Si la triangulation fixe est vide, générer chaque frame indépendamment
	if (fixed_tri.count == 0) {
		for (int f = 0; f < data->n; f++) {
			// Générer chaque frame indépendamment
			TRI tri = triangulate_frame(data, f, nb_couples, paires);
			// Construire l'image intermédiaire
			IMAGE I = build_intermediate_image(data, &tri, f, full_pairs);
			free_TRI(&tri);
			for (int yy = 0; yy < I.haut; yy++) free(I.P[yy]);
			free(I.P);
		}
		free(full_pairs);
		wait_escape();
		return;
	}

	// Générer les images intermédiaires en utilisant la triangulation fixe
	system("mkdir -p images_transformed");
	int M = nb_couples + 4;
	for (int f = 0; f < data->n; f++) {
		TRI tri = create_interpolated_tri_from_fixed(&fixed_tri, data, f, M, paires);
		IMAGE I = build_intermediate_image(data, &tri, f, full_pairs);
		// Sauvegarder l'image
		char filename[256]; snprintf(filename, sizeof(filename), "images_transformed/intermediate_%d.ppm", f);
		ecrire_fichier(I, filename);
		for (int yy = 0; yy < I.haut; yy++) free(I.P[yy]);
		free(I.P);
		free_TRI(&tri);
	}

	free_TRI(&fixed_tri);
	free(full_pairs);

	// Attendre l'évasion avant de fermer
	wait_escape();
}

