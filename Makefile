NAME = morphing

CC = gcc
CFLAGS = -Wall -Wextra -Werror

LDLIBS= `sdl2-config --libs` -lm -lSDL2_ttf
LIBDIR= uvsqgraphics_2

SRCS = main.c lire_ecrire.c uvsqgraphics_2/uvsqgraphics_2.c afficher.c \
		triangulation.c images.c
OBJS = $(SRCS:.c=.o)

LATEXMK := $(shell command -v latexmk 2>/dev/null)
DOC_BUILD := $(if $(LATEXMK),latexmk -pdf, pdflatex description.tex \&\& pdflatex description.tex)
DOC_CLEAN := $(if $(LATEXMK),latexmk -C, rm -f description.log description.aux description.fls description.fdb_latexmk description.out)
DOC_FCLEAN := $(if $(LATEXMK),latexmk -C, rm -f description.pdf)

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(OBJS) -o $(NAME) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

doc: description.tex
	$(DOC_BUILD)

doc_clean:
	$(DOC_CLEAN)

doc_fclean: doc_clean
	$(DOC_FCLEAN)

film:
	ffmpeg -y -framerate 25 -i images_transformed/intermediate_%d.ppm -vf "pad=width=if(eq(mod(iw\,2)\,1)\,iw+1\,iw):height=if(eq(mod(ih\,2)\,1)\,ih+1\,ih)" -c:v libx264 -pix_fmt yuv420p morphing_output.mp4

clean:
	rm -f $(OBJS)
	rm -f images_transformed/*.ppm

fclean: clean
	rm -f $(NAME)
	rm -f *.mp4

re: fclean all
