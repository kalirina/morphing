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
	affiche_all();

	// 3️⃣ Prepare for clicks
	POINT points[6];  // maximum 6 points
	int nb_points = 0;

	// 4️⃣ Loop to get clicks
	while (nb_points < 6) {
		POINT p = wait_clic(); // Attente d'un clic

		// Optionnel: vérifier que le clic est dans l'image
		if ((p.x >= x_depart && p.x < x_depart + data->img_depart.larg &&
			p.y >= y_depart && p.y < y_depart + data->img_depart.haut) ||
			(p.x >= x_arrive && p.x < x_arrive + data->img_arrive.larg &&
			p.y >= y_arrive && p.y < y_arrive + data->img_arrive.haut)) {

			points[nb_points] = p;     // Stocker le point
			nb_points++;

			 // Redraw everything
			fill_screen(hotpink);
			afficher_image(data->img_depart, x_depart, y_depart);
			afficher_image(data->img_arrive, x_arrive, y_arrive);

			// Draw all circles so far
			for (int i = 0; i < nb_points; i++)
				draw_circle(points[i], 5, rouge);

			affiche_all(); // Actualiser l'affichage
		}
	}

	// 5️⃣ Wait for escape to close
	wait_escape();

	for (int i = 0; i < nb_points; i++) {
		printf("Point %d: x=%d, y=%d\n", i+1, points[i].x, points[i].y);
	}
}
