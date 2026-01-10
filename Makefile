NAME = morphing

CC = gcc
CFLAGS = -Wall -Wextra -Werror

LDLIBS= `sdl2-config --libs` -lm -lSDL2_ttf
LIBDIR= uvsqgraphics_2

SRCS = main.c lire_ecrire.c uvsqgraphics_2/uvsqgraphics_2.c afficher.c
OBJS = $(SRCS:.c=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(OBJS) -o $(NAME) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)
	rm -f images_transformed/*.ppm

fclean: clean
	rm -f $(NAME)

re: fclean all
