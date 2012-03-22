/*
 * Copyright (c) 2011, Edd Barrett <vext01@gmail.com>
 * Copyright (c) 2011, Martin Ellis <ellism88@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This code is from hgd:
 * http://github.com/vext01/hgd
 */

#define _GNU_SOURCE	/* linux */

#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "common.h"

void *
xmalloc(size_t sz)
{
	void			*ptr;

	ptr = malloc(sz);
	if (!ptr)
		DPRINTF(DM_D_ERROR, "Could not allocate");

	return (ptr);

}

void *
xcalloc(size_t sz, size_t size)
{
	void			*ptr;

	ptr = calloc(sz, size);
	if (!ptr)
		DPRINTF(DM_D_ERROR, "Could not allocate");

	return (ptr);
}

void *
xrealloc(void *old_p, size_t sz)
{
	void			*ptr;

	ptr = realloc(old_p, sz);
	if (!ptr)
		DPRINTF(DM_D_ERROR, "Could not reallocate");

	return (ptr);
}

char *
xstrdup(const char *s)
{
	char *dup = strdup(s);

	if (dup == NULL)
		DPRINTF(DM_D_ERROR, "Could not duplicate string");

	return (dup);
}

int
xasprintf(char **buf, char *fmt, ...)
{
	va_list			ap;
	int			ret;

	va_start(ap, fmt);
	ret = vasprintf(buf, fmt, ap);

	if (ret == -1)
		DPRINTF(DM_D_ERROR, "Can't allocate");

	return (ret);
}
