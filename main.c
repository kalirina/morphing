#include "morphing.h"

// Convertit les images d'entrée en PPM ASCII en utilisant ImageMagick
void convert_to_ppm(MORPH *data, char *img_file_name_1, char *img_file_name_2) {
	char cmd_1[1024];
	char cmd_2[1024];
	char name_1[1024];
	char name_2[1024];
	strcpy(name_1, img_file_name_1);
	strcpy(name_2, img_file_name_2);
	char *dot_1 = strrchr(name_1, '.');
	char *dot_2 = strrchr(name_2, '.');
	if (dot_1) *dot_1 = '\0';
	if (dot_2) *dot_2 = '\0';
	// Construire les commandes de conversion
	int len_1 = snprintf(cmd_1, sizeof(cmd_1), "convert images/%s -compress none -define ppm:format=ascii images_transformed/%s.ppm", img_file_name_1, name_1);
	int len_2 = snprintf(cmd_2, sizeof(cmd_2), "convert images/%s -compress none -define ppm:format=ascii images_transformed/%s.ppm", img_file_name_2, name_2);
	if (len_1 < 0 || len_1 >= (int)sizeof(cmd_1) || len_2 < 0 || len_2 >= (int)sizeof(cmd_2)) {
		fprintf(stderr, "Command too long\n");
		exit(EXIT_FAILURE);
	}
	// Exécuter les commandes de conversion
	system(cmd_1);
	system(cmd_2);

	// Stocker les chemins des fichiers PPM dans la structure MORPH
	char path_1[1024];
	char path_2[1024];
	int l1 = snprintf(path_1, sizeof(path_1), "images_transformed/%s.ppm", name_1);
	int l2 = snprintf(path_2, sizeof(path_2), "images_transformed/%s.ppm", name_2);
	if (l1 < 0 || l1 >= (int)sizeof(path_1) || l2 < 0 || l2 >= (int)sizeof(path_2)) {
		fprintf(stderr, "Path too long\n");
		exit(EXIT_FAILURE);
	}
	data->name_1 = strdup(path_1);
	data->name_2 = strdup(path_2);
}

int main(int argc, char **argv) {
	if (argc != 4)
		return printf("Wrong number of parameters\n"), -1;
	MORPH data;
	convert_to_ppm(&data ,argv[1], argv[2]);
	data.n = atoi(argv[3]);
	if (data.n <= 0)
		return printf("Wrong number of intermediate images\n"), -1;
	data.img_depart = lire_fichier(data.name_1);
	data.img_arrive = lire_fichier(data.name_2);
	afficher(&data);

	return 0;
}
