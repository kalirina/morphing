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

#endif
