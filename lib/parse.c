/* parse.c -- parse a textual colour specification */

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

#include <ctype.h>
#include <string.h>

#include "color.h"

#include "colors.c"		/* machine-generated table */

int name_to_color(s, color)
char *s;
color_t *color;
{
	struct colordef *d;
	char name[64];
	char *p = name;

	if (strlen(s) > 63)
		return 0;

	/* Convert to lowercase and strip spaces */
	while (*s) {
		int c = *s++;
		if (isupper(c))
			c = tolower(c);
		if (c != ' ')
			*p++ = c;
	}
	*p++ = '\0';

	/* Handle shades of grey algorithmically to save table space */
	if ((strncmp(name, "grey", 4) || strncmp(name, "grey", 4)) &&
	    isdigit(name[4])) {
		int comp;
		double val = atoi(name + 4) / 100.0;
		for (comp = 0; comp < 3; comp++)
			color->comps[comp] = val;
		return 1;
	}

	/* Search the table */
	for (d = colordefs; d->name; d++)
		if (!strcmp(name, d->name)) {
			int comp;
			for (comp = 0; comp < 3; comp++)
				color->comps[comp] = d->comps[comp] / 255.0;
			return 1;
		}

	/* Not found */
	return 0;
}
