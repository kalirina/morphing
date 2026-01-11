#ifndef MORPHING_H
#define MORPHING_H

#include "uvsqgraphics_2/uvsqgraphics_2.h"
#include "uvsqgraphics_2/uvsqcouleur_2.h"
#include <SDL2/SDL.h>
#include <stdlib.h> // system
#include <string.h> // strlcat, strcpy
#include <stdio.h> // printf
#include <math.h> // round

# define MAX_COUPLES 50

// Macros for RGB extraction
#define R(coul) ((coul&0xff0000)>>16)
#define G(coul) ((coul&0x00ff00)>>8)
#define B(coul) ((coul&0x0000ff))

typedef struct image {
	int haut, larg, range;
	COULEUR **P; //les pixels
	int x0, y0; // position dans la fenêtre graphique
} IMAGE;

typedef struct triangle {
	POINT* a;
	POINT* b;
	POINT* c;
} TRIANGLE;

typedef struct couple_points {
	POINT g;
	POINT d;
} COUPLE;

typedef struct triangulation {
	TRIANGLE *triangles; // dynamic array of triangles
	int count; // number of triangles stored
	POINT **allocated_points; // pointers to POINTs allocated for triangles
	int alloc_count; // number of allocated POINT pointers
} TRI;

typedef struct morph {
	IMAGE img_depart;
	IMAGE img_arrive;
	int n; // nombre d'images intermédiaires
	char *name_1;
	char *name_2;

	POINT texte_pt;
	int nb_s_couples;

	int total_points;
	COUPLE *all_points;
} MORPH;

void allouer_pixel(IMAGE *I);
IMAGE lire_fichier(char *nom);
void ecrire_fichier(IMAGE I, char *nom);
void afficher(MORPH *data);
TRI triangulate_frame(MORPH *data, int frame_index);
void free_TRI(TRI *t);
COUPLE *build_all_points(MORPH *data, COUPLE *paires);
IMAGE build_intermediate_image(MORPH *data, TRI *tri, int frame_index, COUPLE *pairs);
int point_in_rect(POINT p, POINT tl, POINT br);
TRI create_interpolated_tri_from_fixed(const TRI *fixed_tri, MORPH *data, int f, int M, COUPLE *paires);
void triangulation(MORPH *data, POINT pts_droite[], POINT pts_gauche[]);

#endif
