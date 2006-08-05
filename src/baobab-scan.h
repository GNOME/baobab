/*
 * baobab-scan.h
 * This file is part of baobab
 *
 * Copyright (C) 2005-2006 Fabio Marzocca  <thesaltydog@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#ifndef __BBTHREAD_H__
#define __BBTHREAD_H__

#include <glib/gtypes.h>
#include <libgnomevfs/gnome-vfs.h>
#include <sys/types.h>

void getDir (const gchar *);

/* hardlinks handling */

typedef struct _baobab_hardlinks_array baobab_hardlinks_array;

baobab_hardlinks_array * baobab_hardlinks_array_create (void);
gboolean baobab_hardlinks_array_has (baobab_hardlinks_array *, GnomeVFSFileInfo *);
void baobab_hardlinks_array_add (baobab_hardlinks_array *, GnomeVFSFileInfo *);
void baobab_hardlinks_array_free (baobab_hardlinks_array *);


#endif /* __BBTHREAD_H__ */
