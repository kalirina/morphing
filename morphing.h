#ifndef MORPHING_H
#define MORPHING_H

#include "uvsqgraphics_2/uvsqgraphics_2.h"
#include "uvsqgraphics_2/uvsqcouleur_2.h"
#include <SDL2/SDL.h>
#include <stdlib.h> // system
#include <string.h> // strlcat, strcpy
#include <stdio.h> // printf

// Macros for RGB extraction
#define R(coul) ((coul&0xff0000)>>16)
#define G(coul) ((coul&0x00ff00)>>8)
#define B(coul) ((coul&0x0000ff))

typedef struct image {
	int haut, larg, range;
	COULEUR **P; //les pixels
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
	int count;           // number of triangles stored
	POINT **allocated_points; // pointers to POINTs allocated for triangles
	int alloc_count;     // number of allocated POINT pointers
} TRI;

typedef struct morph {
	IMAGE img_depart;
	IMAGE img_arrive;
	int n;
	char *name_1;
	char *name_2;
} MORPH;

void allouer_pixel(IMAGE *I);
IMAGE lire_fichier(char *nom);
void ecrire_fichier(IMAGE I, char *nom);
void afficher(MORPH *data);
/* Triangulation APIs */
TRI *triangulate_frames(MORPH *data, int nb_pts_select, COUPLE *dots);
void free_TRI_array(TRI *arr, int frames);
/* Triangulate a single frame (frame_index in [0..data->n-1]) */
TRI triangulate_frame(MORPH *data, int frame_index, int nb_pts_select, COUPLE *dots);
/* Free a single TRI produced by triangulation functions */
void free_TRI(TRI *t);

#endif
