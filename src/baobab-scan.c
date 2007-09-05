/*
 * baobab-scan.c
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


#include <config.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs.h>

#include "baobab.h"
#include "baobab-scan.h"


/*
   Hardlinks handling.

   As long as we're optimistic about hardlinks count
   over the whole system (250 files with st_nlink > 1 here),
   we keep linear search. If it turns out to be an bottleneck
   we can switch to an hash table or tree.

   TODO: get real timings about this code. find out the average
   number of files with st_nlink > 1 on average computer.

   To save memory, we store only { inode, dev } instead of full
   GnomeVfsFileInfo.

   EDIT: /me stupid. I realize that this code was not called that often
   1 call per file with st_nlink > 1. BUT, i'm using pdumpfs to backup
   my /etc. pdumpfs massively uses hard links. So there are more than
   5000 files with st_nlink > 1. I believe this is the worst case.
*/

typedef struct {
	GnomeVFSInodeNumber inode;
	dev_t               device;
} BaobabHardLink;

typedef GArray BaobabHardLinkArray;

static BaobabHardLinkArray *
baobab_hardlinks_array_create (void)
{
	return g_array_new (FALSE, FALSE, sizeof(BaobabHardLink));
}

static gboolean
baobab_hardlinks_array_has (BaobabHardLinkArray *a,
			    GnomeVFSFileInfo    *s)
{
	guint i;

	for (i = 0; i < a->len; ++i) {
		BaobabHardLink *cur = &g_array_index (a, BaobabHardLink, i);

		/*
		 * cur->st_dev == s->st_dev is the common case and may be more
		 * expansive than cur->st_ino == s->st_ino
		 * so keep this order */
		if (cur->inode == s->inode && cur->device == s->device)
			return TRUE;
	}

	return FALSE;
}

/* return FALSE if the element was already in the array */
static gboolean
baobab_hardlinks_array_add (BaobabHardLinkArray *a,
			    GnomeVFSFileInfo    *s)
{
	BaobabHardLink hl;

	if (baobab_hardlinks_array_has (a, s))
		return FALSE;

	hl.inode = s->inode;
	hl.device = s->device;

	g_array_append_val (a, hl);

	return TRUE;
}

static void
baobab_hardlinks_array_free (BaobabHardLinkArray *a)
{
/*	g_print ("HL len was %d\n", a->len); */

	g_array_free (a, TRUE);
}

#define BLOCK_SIZE 512

struct allsizes {
	GnomeVFSFileSize size;
	GnomeVFSFileSize alloc_size;
};

static struct allsizes
loopdir (GnomeVFSURI *vfs_uri_dir,
	 GnomeVFSFileInfo *dir_info,
	 guint count,
	 BaobabHardLinkArray *hla)
{
	GList *file_list;
	guint64 tempHLsize;
	gint elements;
	struct chan_data data;
	struct allsizes retloop;
	struct allsizes temp;
	GnomeVFSResult result;
	gchar *dir;
	gchar *string_to_display;
	GnomeVFSURI *new_uri;

	count++;
	elements = 0;
	tempHLsize = 0;
	retloop.size = 0;
	retloop.alloc_size = 0;
	dir = NULL;
	string_to_display = NULL;

	/* Skip the virtual file systems */
	if ((strcmp (gnome_vfs_uri_get_path (vfs_uri_dir), "/proc") == 0) ||
            (strcmp (gnome_vfs_uri_get_path (vfs_uri_dir), "/sys") == 0))
		goto exit;

	dir = gnome_vfs_uri_to_string (vfs_uri_dir, GNOME_VFS_URI_HIDE_NONE);

	/* Skip the user excluded folders */
	if (is_excluded_dir (dir))
		goto exit;

	if (!baobab.is_local)
		string_to_display = gnome_vfs_unescape_string_for_display (dir);
	else
		string_to_display = gnome_vfs_format_uri_for_display (dir);

	/* Folders we can't access (e.g perms 644). Skip'em. */
	if ((dir_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_TYPE) == 0)
		goto exit;				 

	if ((dir_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_SIZE) != 0)
		retloop.size = dir_info->size;
	if ((dir_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_BLOCK_COUNT) != 0)
		retloop.alloc_size = dir_info->block_count * BLOCK_SIZE;

	/* All skipped folders (i.e. bad type, excluded, /proc) must be
	   skept *before* this point. Everything passes the prefill-model
	   will be part of the GUI. */

	/* prefill the model */
	data.size = 1;
	data.alloc_size = 1;
	data.depth = count - 1;
	data.elements = -1;
	data.dir = string_to_display;
	data.tempHLsize = tempHLsize;
	fill_model (&data);

	/* get the GnomeVFSFileInfo stuct for every directory entry */
	result = gnome_vfs_directory_list_load (&file_list,
						dir,
						GNOME_VFS_FILE_INFO_DEFAULT);

	if (result == GNOME_VFS_OK) {
		GList *l;

		for (l = file_list; l != NULL; l = l->next) {
			GnomeVFSFileInfo *info = l->data;

			if (baobab.STOP_SCANNING) {
				gnome_vfs_file_info_list_free (file_list);
				goto exit;
			}

			if (strcmp (info->name, ".") == 0 ||
			    strcmp (info->name, "..") == 0)
				continue;

			/* is a symlink? */
			if (info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK)
				continue;

			/* is a directory? */
			if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
				new_uri = gnome_vfs_uri_append_file_name (vfs_uri_dir, info->name);
				temp = loopdir (new_uri, info, count, hla);
				retloop.size += temp.size;
				retloop.alloc_size += temp.alloc_size;
				elements++;
				gnome_vfs_uri_unref (new_uri);
			}

			else if (info->type == GNOME_VFS_FILE_TYPE_REGULAR) {

				/* check for hard links only on local files */
				if (GNOME_VFS_FILE_INFO_LOCAL (info)) {
					if (info->link_count > 1) {

						if (!baobab_hardlinks_array_add (hla, info)) {

							/* we already acconted for it */
							tempHLsize += info->size;
							continue;
						}
					}
				}

				if ((info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_BLOCK_COUNT) != 0)
					retloop.alloc_size += (info->block_count * BLOCK_SIZE);

				retloop.size += info->size;
				elements++;
			}
		}

		gnome_vfs_file_info_list_free (file_list);
	}

	data.dir = string_to_display;
	data.size = retloop.size;
	data.alloc_size = retloop.alloc_size;
	data.depth = count - 1;
	data.elements = elements;
	data.tempHLsize = tempHLsize;
	fill_model (&data);

 exit:
	g_free (dir);
	g_free (string_to_display);

	return retloop;
}

void
getDir (const gchar *uri_dir)
{
	BaobabHardLinkArray *hla;
	GnomeVFSURI *vfs_uri;
	GnomeVFSFileInfo *info;

	if (is_excluded_dir (uri_dir))
		return;

	hla = baobab_hardlinks_array_create ();

	vfs_uri = gnome_vfs_uri_new (uri_dir);

	info = gnome_vfs_file_info_new ();
	gnome_vfs_get_file_info (uri_dir,
				 info,
	                         GNOME_VFS_FILE_INFO_DEFAULT);

	loopdir (vfs_uri, info, 0, hla);

	baobab_hardlinks_array_free (hla);
	gnome_vfs_uri_unref (vfs_uri);
	gnome_vfs_file_info_unref (info);
}
