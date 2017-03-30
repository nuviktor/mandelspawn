/* cmap.c -- X-independent colormap support */

/*  
    This file is part of MandelSpawn, a network Mandelbrot program.

    Copyright (C) 1993 Andreas Gustafsson

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

#include <math.h>		/* for floor() declaration */
#include <string.h>

#ifdef NO_STRDUP
char *strdup();
#endif

/*
  It is impossible to declare malloc() in a portable way.
  Be prepared to change these declarations. 
*/
void *malloc(), *realloc();
void free();

#include "color.h"

parse_spectrum(spectrum, ncolours, colors)
char *spectrum;
int ncolours;			/* number of colormap slots available */
color_t *colors;		/* buffer to write colors into (size ncolours) */
{
	color_t *stops;
	unsigned stops_allocated = 4;	/* initial allocation of colour stops */
	int nstops, i;
	char *spectrum_copy, *p;

	spectrum_copy = strdup(spectrum);
	p = spectrum_copy;

	stops = (color_t *) malloc(stops_allocated * sizeof(color_t));

	i = 0;
	while (1) {
		char *end;
		color_t dummy;
		end = strchr(p, '-');
		if (end)
			*end = '\0';
		if (i >= stops_allocated) {
			stops_allocated *= 2;
			stops = (color_t *)
			    realloc((char *)stops,
				    stops_allocated * sizeof(color_t));
		}
		if (!name_to_color(p, &stops[i])) {
			static char *err = "unrecognized colour in spectrum: ";
			char *msg = malloc(sizeof(err) + strlen(p));
			strcpy(msg, err);
			strcat(msg, p);
			cmap_error(msg);
			free(msg);
		/*NOTREACHED*/}
		i++;
		if (end)
			p = end + 1;
		else
			break;
	}

	nstops = i;

	/* interpolate intermediate colours */

	for (i = 0; i < ncolours; i++) {
		double flstop, stop, fpart;
		int istop, jstop;
		int comp;

		flstop =
		    (double)i / (double)(ncolours - 1) * (double)(nstops - 1);

		/*
		   Calculate the number of the next lower stop 
		   (should be ==nstops-1 only for i==ncolours-1)
		 */
		stop = floor(flstop);
		fpart = flstop - stop;

		/*
		   Calculate the indices of the two stops to interpolate between 
		   If we are at the last stop, interpolate between it and itself 
		   to avoid accesses past the end of the array (the value will be 
		   multiplied by zero anyway)
		 */
		istop = (int)stop;
		jstop = (istop + 1 >= nstops ? nstops - 1 : istop + 1);

		for (comp = 0; comp < 3; comp++)
			colors[i].comps[comp] =
			    (1 - fpart) * stops[istop].comps[comp] +
			    fpart * stops[jstop].comps[comp];
	}
	free(stops);
	free(spectrum_copy);
}
