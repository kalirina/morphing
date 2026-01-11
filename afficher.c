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

/* Helper: nearest-neighbor sample from IMAGE at floating coords */
static COULEUR sample_image_nn(const IMAGE *img, float x, float y) {
	int ix = (int)roundf(x);
	int iy = (int)roundf(y);
	if (ix < 0) ix = 0;
	if (iy < 0) iy = 0;
	if (ix >= img->larg) ix = img->larg - 1;
	if (iy >= img->haut) iy = img->haut - 1;
	return img->P[iy][ix];
}

/* Build an intermediate IMAGE from a TRI for frame `frame_index`.
   Uses barycentric coordinates and maps to the source/destination images.
   Returns an allocated IMAGE; caller is responsible for freeing its pixels. */
static IMAGE build_intermediate_image(MORPH *data, TRI *tri, int frame_index, COUPLE *pairs) {
	IMAGE I;
	float a = (float)frame_index / (float)data->n;
	I.larg = (int)roundf((1.0f - a) * data->img_depart.larg + a * data->img_arrive.larg);
	I.haut = (int)roundf((1.0f - a) * data->img_depart.haut + a * data->img_arrive.haut);
	I.range = 255;
	allouer_pixel(&I);

	POINT **alloc = tri->allocated_points;
	int alloc_count = tri->alloc_count;

	for (int y = 0; y < I.haut; y++) {
		for (int x = 0; x < I.larg; x++) {
			POINT P = { x, y };
			int found = 0;
			for (int t = 0; t < tri->count; t++) {
				POINT *A = tri->triangles[t].a;
				POINT *B = tri->triangles[t].b;
				POINT *C = tri->triangles[t].c;
				if (!A || !B || !C) continue;

				/* compute barycentric lambda, mu solving P = A + lambda*(B-A) + mu*(C-A) */
				float v0x = B->x - A->x;
				float v0y = B->y - A->y;
				float v1x = C->x - A->x;
				float v1y = C->y - A->y;
				float v2x = P.x - A->x;
				float v2y = P.y - A->y;
				float det = v0x * v1y - v1x * v0y;
				if (fabsf(det) < 1e-6f) continue; /* degenerate triangle */
				float lambda = (v2x * v1y - v1x * v2y) / det;
				float mu = (v0x * v2y - v2x * v0y) / det;
				/* check if inside triangle (inclusive) */
				if (lambda < -1e-6f || mu < -1e-6f || (lambda + mu) > 1.0f + 1e-6f) continue;

				/* find indices of A,B,C in alloc array */
				int ia = -1, ib = -1, ic = -1;
				for (int j = 0; j < alloc_count; j++) {
					if (alloc[j] == A) ia = j;
					if (alloc[j] == B) ib = j;
					if (alloc[j] == C) ic = j;
					if (ia != -1 && ib != -1 && ic != -1) break;
				}
				if (ia == -1 || ib == -1 || ic == -1) continue;

				/* Get corresponding points in source and destination images */
				POINT Asrc, Bsrc, Csrc, Adst, Bdst, Cdst;
				/* `pairs` is the full array (corners then selected); indices already align */
				Asrc = pairs[ia].g;
				Bsrc = pairs[ib].g;
				Csrc = pairs[ic].g;

				Adst = pairs[ia].d;
				Bdst = pairs[ib].d;
				Cdst = pairs[ic].d;

				/* Compute PD = Asrc + lambda*(Bsrc-Asrc) + mu*(Csrc-Asrc) */
				float PDx = Asrc.x + lambda * (Bsrc.x - Asrc.x) + mu * (Csrc.x - Asrc.x);
				float PDy = Asrc.y + lambda * (Bsrc.y - Asrc.y) + mu * (Csrc.y - Asrc.y);
				/* Compute PA = Adst + lambda*(Bdst-Adst) + mu*(Cdst-Adst) */
				float PAx = Adst.x + lambda * (Bdst.x - Adst.x) + mu * (Cdst.x - Adst.x);
				float PAy = Adst.y + lambda * (Bdst.y - Adst.y) + mu * (Cdst.y - Adst.y);

				/* Sample colors from source and destination images (nearest neighbor) */
				COULEUR colD = sample_image_nn(&data->img_depart, PDx, PDy);
				COULEUR colA = sample_image_nn(&data->img_arrive, PAx, PAy);

				/* blend components using factor a (frame interpolation) */
				float alpha = a;
				int rD = R(colD), gD = G(colD), bD = B(colD);
				int rA = R(colA), gA = G(colA), bA = B(colA);
				int rf = (int)roundf((1.0f - alpha) * rD + alpha * rA);
				int gf = (int)roundf((1.0f - alpha) * gD + alpha * gA);
				int bf = (int)roundf((1.0f - alpha) * bD + alpha * bA);
				if (rf < 0) rf = 0;
				if (rf > 255) rf = 255;
				if (gf < 0) gf = 0;
				if (gf > 255) gf = 255;
				if (bf < 0) bf = 0;
				if (bf > 255) bf = 255;
				COULEUR final = (rf << 16) | (gf << 8) | bf;
				I.P[y][x] = final;
				found = 1;
				break;
			}
			if (!found) {
				/* fallback: black */
				I.P[y][x] = 0;
			}
		}
	}

	return I;
}

/* (Removed debug-only Bresenham line drawing helper.) */

void afficher(MORPH *data) {
	init_graphics(1920, 1080);
	affiche_auto_off();
	fill_screen(hotpink);

	//some border checks maybe even earlier
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

	// 3️⃣ Prepare for clicks
	// maximum 6 points
	POINT pts_gauche[3];
	POINT pts_droite[3];
	int nb_couples = 0;
	int attendre_gauche = 1; // 1 = gauche, 0 = droite

	// 4️⃣ Loop to get clicks
	while (nb_couples < 3) {
		POINT p = wait_clic();

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

		// 🔄 Redessiner
		fill_screen(hotpink);
		afficher_image(data->img_depart, x_depart, y_depart);
		afficher_image(data->img_arrive, x_arrive, y_arrive);
		aff_pol_centre(
			attendre_gauche ?
			"Cliquez sur l'image de GAUCHE" :
			"Cliquez sur l'image de DROITE",
			30, texte, noir
		);

		// Dessiner les couples
		for (int i = 0; i < nb_couples; i++) {
			draw_circle(pts_gauche[i], 10, rouge);
			draw_circle(pts_droite[i], 10, rouge);

			POINT t1 = { pts_gauche[i].x + 14, pts_gauche[i].y + 14 };
			POINT t2 = { pts_droite[i].x + 14, pts_droite[i].y + 14 };

			aff_int(i + 1, 20, t1, rouge);
			aff_int(i + 1, 20, t2, rouge);
		}

		// Si un point gauche est cliqué mais pas encore le droit
		if (!attendre_gauche) {
			draw_circle(pts_gauche[nb_couples], 10, rouge);
		}

		affiche_all();
	}

	COUPLE paires[nb_couples];
	for (int i = 0; i < nb_couples; i++) {
		/* Convert clicked screen coordinates to image-local coordinates */
		paires[i].g.x = pts_gauche[i].x - x_depart;
		paires[i].g.y = pts_gauche[i].y - y_depart;
		paires[i].d.x = pts_droite[i].x - x_arrive;
		paires[i].d.y = pts_droite[i].y - y_arrive;
	}

	/* Build full_pairs: first 4 entries are the image corners, then the selected pairs */
	int full_count = nb_couples + 4;
	COUPLE *full_pairs = (COUPLE*)malloc(sizeof(COUPLE) * full_count);
	if (!full_pairs) return; /* allocation failed */
	/* fill corners (top-left, top-right, bottom-right, bottom-left) */
	full_pairs[0].g.x = 0; full_pairs[0].g.y = 0; full_pairs[0].d.x = 0; full_pairs[0].d.y = 0;
	full_pairs[1].g.x = data->img_depart.larg; full_pairs[1].g.y = 0; full_pairs[1].d.x = data->img_arrive.larg; full_pairs[1].d.y = 0;
	full_pairs[2].g.x = data->img_depart.larg; full_pairs[2].g.y = data->img_depart.haut; full_pairs[2].d.x = data->img_arrive.larg; full_pairs[2].d.y = data->img_arrive.haut;
	full_pairs[3].g.x = 0; full_pairs[3].g.y = data->img_depart.haut; full_pairs[3].d.x = 0; full_pairs[3].d.y = data->img_arrive.haut;
	/* copy selected pairs into indices 4.. */
	for (int i = 0; i < nb_couples; i++) full_pairs[4 + i] = paires[i];

	/* Build a fixed triangulation at midpoint and reuse its connectivity for
	   all frames. This avoids flickering/topology changes between frames. */
	int mid = data->n / 2;
	TRI fixed_tri = triangulate_frame(data, mid, nb_couples, paires);
	if (fixed_tri.count == 0) {
		/* fallback to per-frame triangulation if something went wrong.
		   We do not write debug files here; simply compute frames. */
		for (int f = 0; f < data->n; f++) {
			TRI tri = triangulate_frame(data, f, nb_couples, paires);
			IMAGE I = build_intermediate_image(data, &tri, f, full_pairs);
			/* No debug overlay or file output in fallback mode. */
			free_TRI(&tri);
			for (int yy = 0; yy < I.haut; yy++) free(I.P[yy]);
			free(I.P);
		}
		free(full_pairs);
		wait_escape();
		return;
	}

	/* Ensure output directory exists and, for each frame, create a TRI that reuses fixed connectivity but with
	   per-frame interpolated vertex positions. */
	system("mkdir -p images_transformed");
	int M = nb_couples + 4;
	for (int f = 0; f < data->n; f++) {
		TRI tri;
		tri.count = fixed_tri.count;
		tri.triangles = (TRIANGLE*)malloc(sizeof(TRIANGLE) * tri.count);
		tri.alloc_count = M;
		tri.allocated_points = (POINT**)malloc(sizeof(POINT*) * M);
		for (int i = 0; i < M; i++) tri.allocated_points[i] = NULL;
		float af = (float)f / (float)data->n;
		/* create interpolated points for this frame */
		for (int i = 0; i < M; i++) {
			POINT p;
			if (i < 4) {
				POINT g, d;
				if (i == 0) { g.x = 0; g.y = 0; d.x = 0; d.y = 0; }
				else if (i == 1) { g.x = data->img_depart.larg; g.y = 0; d.x = data->img_arrive.larg; d.y = 0; }
				else if (i == 2) { g.x = data->img_depart.larg; g.y = data->img_depart.haut; d.x = data->img_arrive.larg; d.y = data->img_arrive.haut; }
				else { g.x = 0; g.y = data->img_depart.haut; d.x = 0; d.y = data->img_arrive.haut; }
				p.x = (int)roundf((1.0f - af) * g.x + af * d.x);
				p.y = (int)roundf((1.0f - af) * g.y + af * d.y);
			} else {
				POINT g = paires[i-4].g; POINT d = paires[i-4].d;
				p.x = (int)roundf((1.0f - af) * g.x + af * d.x);
				p.y = (int)roundf((1.0f - af) * g.y + af * d.y);
			}
			tri.allocated_points[i] = (POINT*)malloc(sizeof(POINT));
			*tri.allocated_points[i] = p;
		}
		/* map connectivity from fixed_tri to this tri's point pointers */
		for (int t = 0; t < fixed_tri.count; t++) {
			POINT *origA = fixed_tri.triangles[t].a;
			POINT *origB = fixed_tri.triangles[t].b;
			POINT *origC = fixed_tri.triangles[t].c;
			int ia=-1, ib=-1, ic=-1;
			for (int j = 0; j < fixed_tri.alloc_count; j++) {
				if (fixed_tri.allocated_points[j] == origA) ia = j;
				if (fixed_tri.allocated_points[j] == origB) ib = j;
				if (fixed_tri.allocated_points[j] == origC) ic = j;
			}
			tri.triangles[t].a = tri.allocated_points[ia];
			tri.triangles[t].b = tri.allocated_points[ib];
			tri.triangles[t].c = tri.allocated_points[ic];
		}

		  IMAGE I = build_intermediate_image(data, &tri, f, full_pairs);
		  /* Save intermediate frame so the sequence can be encoded into a movie. */
		  char filename[256]; snprintf(filename, sizeof(filename), "images_transformed/intermediate_%d.ppm", f);
		  ecrire_fichier(I, filename);
		  for (int yy = 0; yy < I.haut; yy++) free(I.P[yy]);
		  free(I.P);
		free_TRI(&tri);
	}

	free_TRI(&fixed_tri);
	free(full_pairs);

	// Wait for escape to close
	wait_escape();
}
