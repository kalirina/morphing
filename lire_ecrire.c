#include "morphing.h"

void allouer_pixel(IMAGE *I) {
	I->P = (COULEUR**)malloc(sizeof(COULEUR*) * I->haut);
	if (!I->P)
		exit(2);
	for (int i = 0; i < I->haut; i++) {
		I->P[i] = (COULEUR*)malloc(sizeof(COULEUR) * I->larg);
		if (!I->P[i])
			exit(2);
	}
}

IMAGE lire_fichier(char *nom) {
	IMAGE I;
	I.P = NULL;
	I.haut = I.larg = I.range = 0;

	FILE *file;
	file = fopen(nom, "r");
	if (file == NULL) {
		printf("Couldn't open file\n");
		exit(2);
	}

	char s[15];
	if (fscanf(file, "%s", s) == 0)
		exit(2);
	if (strcmp(s, "P3") != 0)
		exit(2);
	if (fscanf(file,"%d %d", &I.larg, &I.haut) < 2)
		exit(2);
	if (fscanf(file, "%d", &I.range) == 0)
		exit(2);

	allouer_pixel(&I);
	for (int r = 0; r < I.haut; r++) {
		for (int c = 0; c < I.larg; c++) {
			int rr, gg, bb;
			if (fscanf(file, "%d %d %d", &rr, &gg, &bb) < 3)
				exit(2);
			I.P[r][c] = couleur_RGB(rr,gg,bb);
		}
	}

	fclose(file);
	return I;
}

void ecrire_fichier(IMAGE I, char *nom) {
	FILE *F;
	F = fopen(nom, "w");
	fprintf(F, "P3\n");
	fprintf(F, "%d %d\n", I.larg, I.haut);
	fprintf(F, "%d\n", I.range);

	for (int r = 0; r < I.haut; r++) {
		for (int c = 0; c < I.larg; c++) {
			int rr = R(I.P[r][c]);
			int gg = G(I.P[r][c]);
			int bb = B(I.P[r][c]);
			fprintf(F, "%d %d %d ", rr, gg, bb);
		}
		fprintf(F, "\n");
	}

	fclose(F);
}
