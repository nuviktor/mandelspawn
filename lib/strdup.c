/* strdup.c -- strdup() replacement for machines that lack it */

#ifdef NO_STRDUP

#include <string.h>

/*
  It is impossible to declare malloc() in a portable way.
  Be prepared to change this declaration.
*/
void *malloc();

char *strdup(str)
char *str;
{
	unsigned int length = strlen(str);
	char *r = (char *)malloc(length + 1);
	strcpy(r, str);
	return (r);
}

#endif
