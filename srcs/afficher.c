#include "morphing.h"

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

// Redessine la scène : les deux images et les points sélectionnés
static void redraw_scene(MORPH *data, POINT pts_gauche[], POINT pts_droite[], int nb_couples, int attendre_gauche, int saved, int quit) {
	fill_screen(hotpink);
	afficher_image(data->img_depart, data->img_depart.x0, data->img_depart.y0);
	afficher_image(data->img_arrive, data->img_arrive.x0, data->img_arrive.y0);
	if (quit) {
		aff_pol_centre("Quitter", 30, data->texte_pt, noir);
		return;
	}
	else if (saved) {
		aff_pol_centre("Sauvegarde reussie", 30, data->texte_pt, noir);
	}
	else {
		aff_pol_centre(
		attendre_gauche ?
		"Cliquez sur l'image de GAUCHE" :
		"Cliquez sur l'image de DROITE",
		30, data->texte_pt, noir
		);
	}

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
	// dessiner les boutons
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

// Affiche la fenêtre de début avec les deux images
void afficher_fenetre_debut(MORPH *data) {
	data->img_depart.x0 = 50;
	data->img_depart.y0 = 50;
	data->img_arrive.x0 = 1920 - 50 - data->img_arrive.larg;
	data->img_arrive.y0 = 50;
	afficher_image(data->img_depart, data->img_depart.x0, data->img_depart.y0);
	afficher_image(data->img_arrive, data->img_arrive.x0, data->img_arrive.y0);
	data->texte_pt.x = 850;
	data->texte_pt.y = 1010;
	affiche_all();
}

// La fonction principale d'affichage et d'interaction
void afficher(MORPH *data) {
	init_graphics(1920, 1080);
	affiche_auto_off();
	fill_screen(hotpink);

	afficher_fenetre_debut(data);

	// maximum x couples (possible de s'arrêter avant avec Sauvegarder)
	POINT pts_gauche[MAX_COUPLES];
	POINT pts_droite[MAX_COUPLES];
	data->nb_s_couples = 0;
	int attendre_gauche = 1; // flag: 1 = gauche, 0 = droite

	// positions des buttons
	POINT btn_save_tl = {1400, 980};
	POINT btn_save_br = {1550, 1030};
	POINT btn_quit_tl = {1560, 980};
	POINT btn_quit_br = {1710, 1030};

	int saved = 0;

	// dessiner la scène initiale
	redraw_scene(data, pts_gauche, pts_droite, data->nb_s_couples, attendre_gauche, 0, 0);

	// boucle de sélection des points (peut être arrêtée par Sauvegarder ou Quitter)
	int running = 1;
	while (running && data->nb_s_couples < MAX_COUPLES) {
		char mouse_btn = 'G';
		POINT p = wait_clic_GMD(&mouse_btn);

		// verifier si clic sur boutons
		if (point_in_rect(p, btn_save_tl, btn_save_br)) { // SAUVEGARDER
			// si un couple est incomplet, ignorer le clic Sauvegarder
			if (!attendre_gauche) {
				continue;
			}
			// marquer comme sauvegardé et redessiner
			saved = 1;
			redraw_scene(data, pts_gauche, pts_droite, data->nb_s_couples, attendre_gauche, 1, 0);
			continue;
		}

		if (point_in_rect(p, btn_quit_tl, btn_quit_br) || mouse_btn == 'D') { // QUITTER
			// si sauvegardé, on sort de la boucle pour lancer la triangulation
			if (saved) {
				redraw_scene(data, pts_gauche, pts_droite, data->nb_s_couples, attendre_gauche, 0, 1);
				affiche_all();
				running = 0; // sortir de la boucle
				break;
			// sinon, quitter immédiatement
			} else {
				redraw_scene(data, pts_gauche, pts_droite, data->nb_s_couples, attendre_gauche, 0, 1);
				affiche_all();
				SDL_Quit();
				exit(0);
			}
		}

		// verifier si clic dans les images
		if (attendre_gauche && clic_dans_gauche(p, data, data->img_depart.x0, data->img_depart.y0)) {
			pts_gauche[data->nb_s_couples] = p;
			attendre_gauche = 0; // maintenant on attend la droite
		}
		else if (!attendre_gauche && clic_dans_droite(p, data, data->img_arrive.x0, data->img_arrive.y0)) {
			pts_droite[data->nb_s_couples] = p;
			attendre_gauche = 1; // prochain couple
			data->nb_s_couples++;
		}
		else {
			continue; // clic ignoré
		}

		// Redessiner
		redraw_scene(data, pts_gauche, pts_droite, data->nb_s_couples, attendre_gauche, 0, 0);
	}

	// Si sauvegardé, lancer la triangulation
	if (saved) {
		SDL_Quit();
		triangulation(data, pts_droite, pts_gauche);
		return;
	}

	// Sinon, quitter
	SDL_Quit();
	return;
}

