/* bms.c - batch-mode MandelSpawn client main program */

/*  
    This file is part of MandelSpawn, a network Mandelbrot program.

    Copyright (C) 1990-1993 Andreas Gustafsson

    MandelSpawn is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 1,
    as published by the Free Software Foundation.

    MandelSpawn is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License,
    version 1, along with this program; if not, write to the Free 
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
extern int errno;		/* at least Sony's errno.h misses this */
#include <math.h>		/* for atof() */

/*
  It is impossible to declare malloc() in a portable way.
  Be prepared to change these declarations. 
*/
void *malloc(), *realloc();
void free();

#include "datarep.h"
#include "io.h"
#include "work.h"
#include "mspawn.h"
#include "version.h"
#include "color.h"

typedef struct {
	unsigned char comps[3];
} pixel8;

#define TIMEOUT 15000		/* default computation server timeout in millisecs */

/*
  Number of pixels to print on each line in PGM and PPM files
  (watch out for line length limit of 70 characters) 
*/
#define PGM_NPL 8
#define PPM_NPL 4

char *me;			/* name of program */

/* various global mode flags */

int done = 0;			/* set when all chunks have arrived */
int verbose = 0;		/* verbosity level */
int statistics = 0;		/* print statistics if set */
int pgm_ascii = 0;		/* force ASCII graymap format */
int nooutput = 0;		/* generate no output (for testing only) */

/* structure for global data */

typedef struct bms_state {
	ms_state ms;
	union {
		unsigned char *bytes;
		unsigned short *shorts;
	} frame;
} bms_state;

/* report a fatal error and die */

void error(s)
char *s;
{
	fprintf(stderr, "%s: %s\n", me, s);
	exit(1);
}

/* handle input from a computation server by drawing in an in-core buffer */

void ms_draw(client, client_data, data) /*ARGSUSED*/ char *client;
char *client_data;
char *data;
{
	ms_client_info *the_info = (ms_client_info *) client_data;
	bms_state *b = (bms_state *) client;
	unsigned int x, y, width, height;
	unsigned int i, j;
	if (verbose) {
		putc('.', stderr);
		fflush(stderr);
	}
	x = the_info->s.x;
	y = the_info->s.y;
	width = the_info->s.width;
	height = the_info->s.height;

	switch (b->ms.bytes_per_count) {
	case 1:
		{
			unsigned char *datap = (unsigned char *)data;
			unsigned char *bufp_left = (unsigned char *)
			    b->frame.bytes + b->ms.width * y + x;
			for (j = 0; j < height; j++) {
				unsigned char *bufp = bufp_left;
				for (i = 0; i < width; i++) {
					*bufp++ = *datap++;
				}
				bufp_left += b->ms.width;
			}
		}
		break;
	case 2:
		{
			unsigned short *datap = (unsigned short *)data;
			unsigned short *bufp_left = (unsigned short *)
			    b->frame.shorts + b->ms.width * y + x;
			for (j = 0; j < height; j++) {
				unsigned short *bufp = bufp_left;
				for (i = 0; i < width; i++) {
					*bufp++ = *datap++;
				}
				bufp_left += b->ms.width;
			}
		}
		break;
	default:
		error("bad iteration count size");
		break;
	}
}

/*
  provide error reporting services to the workforce and
  I/O modules 
*/

void wf_error(s)
char *s;
{
	error(s);
}

void wf_warn(s)
char *s;
{
	fprintf(stderr, "warning: %s\n", s);
}

void io_error(s)
char *s;
{
	error(s);
}

void io_warn(s)
char *s;
{
	fprintf(stderr, "warning: %s\n", s);
}

/* unique values to identify options */

enum bms_opt { opt_height, opt_width, opt_x, opt_y, opt_range,
	opt_julia, opt_colours, opt_cx, opt_cy,
	opt_chunk_width, opt_chunk_height, opt_verbose,
	opt_nooutput, opt_statistics, opt_version, opt_ascii,
	opt_colour, opt_spectrum
};

struct option {
	enum bms_opt id;
	char *name;
	int has_arg;
};

struct option options[] = { {opt_height, "height", 1},
{opt_width, "width", 1},
{opt_x, "x", 1},
{opt_y, "y", 1},
{opt_range, "range", 1},
{opt_julia, "julia", 0},
{opt_colours, "colours", 1},
{opt_colours, "colors", 1},
{opt_colours, "iterations", 1},
{opt_cx, "cx", 1},
{opt_cy, "cy", 1},
{opt_chunk_width, "chunk_width", 1},
{opt_chunk_height, "chunk_height", 1},
{opt_verbose, "verbose", 0},
{opt_nooutput, "nooutput", 0},
{opt_statistics, "statistics", 0},
{opt_version, "version", 0},
{opt_ascii, "ascii", 0},
{opt_colour, "colour", 0},
{opt_colour, "color", 0},
{opt_spectrum, "spectrum", 1}
};

cmap_error(msg)
char *msg;
{
	error(msg);
}

int main(argc, argv)
int argc;
char **argv;
{
	bms_state bms;
	wf_state *workforce;
	int i, j;
	char *optarg;

	/* colour stuff */
	int colour_mode = 0;
	char *spectrum =
	    "blue-aquamarine-cyan-medium sea green-forest green-lime green-\
yellow green-yellow-coral-pink-black";
	color_t *colors = 0;
	pixel8 *pixels = 0;

	me = argv[0];

	/* defaults */
	bms.ms.width = 64;
	bms.ms.height = 24;
	bms.ms.center_x = -0.5;
	bms.ms.center_y = 0.0;
	bms.ms.xrange = 4.0;
	bms.ms.julia = 0;
	/* bms.ms.c_x... */
	bms.ms.job.iteration_limit = 250;

	bms.ms.chunk_height = bms.ms.chunk_width = 32;

	for (i = 1; i < argc; i++) {
		char *arg = argv[i];
		if (*arg == '-')
			for (j = 0; j < sizeof(options) / sizeof(options[0]);
			     j++) {
				if (strcmp(arg + 1, options[j].name) == 0) {
					if (options[j].has_arg) {
						if (argc < i)
							error
							    ("option requires argument");
						optarg = argv[++i];
					}
					switch (options[j].id) {
					case opt_width:
						bms.ms.width = atoi(optarg);
						break;
					case opt_height:
						bms.ms.height = atoi(optarg);
						break;
					case opt_x:
						bms.ms.center_x = atof(optarg);
						break;
					case opt_y:
						bms.ms.center_y = atof(optarg);
						break;
					case opt_range:
						bms.ms.xrange = atof(optarg);
						break;
					case opt_julia:
						bms.ms.julia = 1;
						break;
					case opt_colours:
						bms.ms.job.iteration_limit =
						    atoi(optarg);
						break;
					case opt_cx:
						bms.ms.c_x = atof(optarg);
						break;
					case opt_cy:
						bms.ms.c_y = atof(optarg);
						break;
					case opt_chunk_width:
						bms.ms.chunk_width =
						    atoi(optarg);
						break;
					case opt_chunk_height:
						bms.ms.chunk_height =
						    atoi(optarg);
						break;
					case opt_verbose:
						verbose++;
						break;
					case opt_nooutput:
						nooutput++;
						break;
					case opt_statistics:
						statistics++;
						break;
					case opt_version:
						printf("bms version %s\n",
						       ms_version);
						exit(0);
						break;
					case opt_ascii:
						pgm_ascii++;
						break;
					case opt_colour:
						colour_mode++;
						break;
					case opt_spectrum:
						spectrum = optarg;
						break;
					default:
						error
						    ("internal option procesing error");
					}
					goto nextopt;
				}	/* if !strcmp */
			}	/* for j */
		error("unrecognized option");
 nextopt:	;
	}			/* for i */

	if (colour_mode) {
		int i;
		int ncolours = bms.ms.job.iteration_limit;

		colors = (color_t *) malloc(ncolours * sizeof(color_t));
		pixels = (pixel8 *) malloc(ncolours * sizeof(pixel8));
		parse_spectrum(spectrum, ncolours, colors);

		for (i = 0; i < ncolours; i++) {
			int comp;
			for (comp = 0; comp < 3; comp++)
				pixels[i].comps[comp] =
				    (int)(0xFF * colors[i].comps[comp] + 0.5);
		}
	}

	bms.ms.bytes_per_count = (bms.ms.job.iteration_limit > 256 ? 2 : 1);

	ms_init(&bms.ms, (char *)&bms,
		workforce = wf_init(TIMEOUT, IO_MUX_NONE, IO_MUX_SELECT));

	ms_calculate_job_parameters(&bms.ms, &bms.ms.job);

	/* allocate memory for frame buffer */
	switch (bms.ms.bytes_per_count) {
	case 1:
		bms.frame.bytes = (unsigned char *)
		    malloc(bms.ms.width * bms.ms.height *
			   sizeof(unsigned char));
		break;
	case 2:
		bms.frame.shorts = (unsigned short *)
		    malloc(bms.ms.width * bms.ms.height *
			   sizeof(unsigned short));
		break;
	default:
		error("bad iteration count size");
		break;
	}

	ms_dispatch_rect(&bms.ms, (char *)&bms, 0, 0, bms.ms.width,
			 bms.ms.height);

	ms_main(&bms.ms);

	/* we have received all the replies; write out the finished picture */
	if (!nooutput) {
		if (verbose) {
			putc('\n', stderr);
		}

		/* force ASCII mode if PGM maxval > 255 */
		if (!colour_mode && bms.ms.bytes_per_count > 1)
			pgm_ascii = 1;

		printf("P%d\n%d %d\n%d\n",
		       2 + colour_mode + 3 * (!pgm_ascii),
		       bms.ms.width, bms.ms.height,
		       colour_mode ? 0xFF : bms.ms.job.iteration_limit - 1);

		if (!colour_mode) {
			if (pgm_ascii) {	/* ASCII pgm format */
				unsigned i, j;
				for (j = 0; j < bms.ms.height; j++) {
					for (i = 0; i < bms.ms.width; i++)
						printf("%d%c",
						       bms.ms.bytes_per_count ==
						       2 ? bms.frame.shorts[j *
									    bms.
									    ms.
									    width
									    +
									    i] :
						       bms.frame.bytes[j *
								       bms.ms.
								       width +
								       i],
						       i % PGM_NPL ==
						       (PGM_NPL -
							1) ? '\n' : ' ');
					if (i % PGM_NPL)
						putchar('\n');
				}
			} else {	/* binary pgm format */
				unsigned i, j;
#ifdef PARANOIA
				for (j = 0; j < bms.ms.height; j++)
					for (i = 0; i < bms.ms.width; i++)
						putchar(bms.frame.
							bytes[j * bms.ms.width +
							      i]);
#else
				/* we happen to use the same internal representation for the image */
				/* as the data part of a binary pgm file, so we can dump it as such */
				fwrite(bms.frame.bytes, sizeof(char),
				       bms.ms.width * bms.ms.height, stdout);
#endif
			}
		} else {	/* colour */

			if (pgm_ascii) {	/* ASCII ppm format */
				unsigned i, j;

				for (j = 0; j < bms.ms.height; j++) {
					for (i = 0; i < bms.ms.width; i++) {
						pixel8 *pixel =
						    &pixels[bms.ms.
							    bytes_per_count ==
							    2 ? bms.frame.
							    shorts[j *
								   bms.ms.
								   width +
								   i] : bms.
							    frame.bytes[j *
									bms.ms.
									width +
									i]
						    ];
						printf("%d %d %d%s",
						       pixel->comps[RED_COMP],
						       pixel->comps[GREEN_COMP],
						       pixel->comps[BLUE_COMP],
						       i % PPM_NPL ==
						       (PPM_NPL -
							1) ? "\n" : "  ");
					}
					if (i % PPM_NPL)
						putchar('\n');
				}
			} else {	/* binary ppm format */
				unsigned i, j;

				for (j = 0; j < bms.ms.height; j++)
					for (i = 0; i < bms.ms.width; i++) {
						pixel8 *pixel =
						    &pixels[bms.ms.
							    bytes_per_count ==
							    2 ? bms.frame.
							    shorts[j *
								   bms.ms.
								   width +
								   i] : bms.
							    frame.bytes[j *
									bms.ms.
									width +
									i]
						    ];
						putchar(pixel->comps[RED_COMP]);
						putchar(pixel->
							comps[GREEN_COMP]);
						putchar(pixel->
							comps[BLUE_COMP]);
					}
			}
		}
	}

	if (statistics) {
		wf_print_stats(workforce, stderr);
	}
	exit(0);
}
