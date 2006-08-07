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

#include <string.h>
#include <time.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs.h>
#include <glib/gstdio.h>
#include <fnmatch.h>

#include "baobab.h"
#include "baobab-scan.h"


struct allsizes {
	GnomeVFSFileSize size;
	GnomeVFSFileSize alloc_size;
};

struct _baobab_hardlinks_array {
	/* Array of GnomeFileInfo *
	   as long as we're optimistic about hardlinks count
	   over the whole system (250 files with st_nlink > 1 here),
	   we keep linear search.

	   TODO: get real timings about this code. find out the average
	   number of files with st_nlink > 1 on average computer.

	   MAYBE: store only { inode, dev } instead of full struct stat.
	   I don't know if dev_t and ino_t are standard enough so i guess
	   i would be safier to store both as guint64.
	   On my ppc :
	   - dev_t is 8 bytes
	   - ino_t is 4 bytes
	   - struct stat is 88 bytes

	   MAYBE: turn into a hashtable or tree for faster lookups.
	   Would use st_ino or (st_ino and st_dev) for hash
	   and use st_ino and st_dev for equality.
	   Again, as we don't know the sizeof of these st_*, i don't
	   think using g_direct_hash / g_direct_equal / boxing st_something
	   into a void* would be safe.

	   EDIT: /me stupid. I realize that this code was not called that often
	   1 call per file with st_nlink > 1. BUT, i'm using pdumpfs to backup
	   my /etc. pdumpfs massively uses hard links. So there are more than
	   5000 files with st_nlink > 1. I believe this is the worst case.
	 */

	GArray *inodes;
};

baobab_hardlinks_array *
baobab_hardlinks_array_create (void)
{
	baobab_hardlinks_array *array;

	array = g_new (baobab_hardlinks_array, 1);
	array->inodes = g_array_new (FALSE, FALSE, sizeof (GnomeVFSFileInfo));

	return array;
}

gboolean
baobab_hardlinks_array_has (baobab_hardlinks_array *a,
			    GnomeVFSFileInfo *s)
{
	guint i;

	for (i = 0; i < a->inodes->len; ++i) {
		GnomeVFSFileInfo *cur = &g_array_index (a->inodes, GnomeVFSFileInfo, i);

		/*
		 * cur->st_dev == s->st_dev is the common case and may be more
		 * expansive than cur->st_ino == s->st_ino
		 * so keep this order */
		if (cur->inode == s->inode && cur->device == s->device)
			return TRUE;
	}

	return FALSE;
}

void
baobab_hardlinks_array_add (baobab_hardlinks_array *a,
			    GnomeVFSFileInfo *s)
{
	/* FIXME: maybe slow check */
	g_assert (!baobab_hardlinks_array_has (a, s));
	g_array_append_val (a->inodes, *s);
}

void
baobab_hardlinks_array_free (baobab_hardlinks_array *a)
{
	/*
	g_print("HL len was %u (size %u)\n",
	a->inodes->len,
	(unsigned) a->inodes->len * sizeof(struct stat));
	*/
	g_array_free (a->inodes, TRUE);
	g_free (a);
}

static void
loopsearch (GnomeVFSURI *vfs_uri_dir, const gchar *search)
{
	GList *file_list, *l;
	gchar *dir;
	struct BaobabSearchRet bbret;
	GnomeVFSResult result;
	GnomeVFSURI *new_uri;

	dir = gnome_vfs_uri_to_string (vfs_uri_dir, GNOME_VFS_URI_HIDE_NONE);

	if (is_excluded_dir (dir))
		return;

	/* get the GnomeVFSFileInfo stuct for every directory entry */
	result = gnome_vfs_directory_list_load (&file_list, dir, GNOME_VFS_FILE_INFO_GET_MIME_TYPE);
	g_free (dir);

	while (gtk_events_pending ())
		gtk_main_iteration ();

	if (result == GNOME_VFS_OK) {
		for (l = file_list; l != NULL; l = l->next) {
			GnomeVFSFileInfo *info = l->data;

			if (baobab.STOP_SCANNING) {
				gnome_vfs_file_info_list_free (file_list);
				return;
			}

			if ((info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_TYPE) == 0)
				continue;

			if (strcmp (info->name, ".") == 0 ||
			    strcmp (info->name, "..") == 0)
				continue;

			new_uri = gnome_vfs_uri_append_file_name (vfs_uri_dir, info->name);
			dir = gnome_vfs_uri_to_string (new_uri, GNOME_VFS_URI_HIDE_NONE);

			if (fnmatch (search, info->name, FNM_PATHNAME | FNM_PERIOD) == 0) {
				gchar *string_to_display;

				string_to_display = gnome_vfs_format_uri_for_display (dir);
				bbret.fullpath = string_to_display;
				bbret.size = info->size;
				bbret.lastacc = info->mtime;
				bbret.owner = info->uid;
				bbret.alloc_size = info->block_count * 512;
				bbret.mime_type = info->mime_type;
				fill_search_model (&bbret);
				g_free (string_to_display);
			}

			/* is a directory? */
			if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY &&
			    !bbSearchOpt.dont_recurse_dir) {
				if (strcmp (gnome_vfs_uri_get_path (vfs_uri_dir), "/proc") != 0)
					loopsearch (new_uri, search);
			}

			gnome_vfs_uri_unref (new_uri);
			g_free (dir);
		}

		gnome_vfs_file_info_list_free (file_list);
	}
}

static struct allsizes
loopdir (GnomeVFSURI *vfs_uri_dir,
	 guint count,
	 baobab_hardlinks_array *hla)
{
	GList *file_list, *l;
	guint64 tempHLsize;
	gint elements;
	struct chan_data data;
	struct allsizes retloop;
	struct allsizes temp;
	GnomeVFSResult result;
	gchar *dir, *string_to_display;
	GnomeVFSURI *new_uri;
	GnomeVFSFileInfo *dir_info;

	count++;
	elements = 0;
	tempHLsize = 0;
	retloop.size = 0;
	retloop.alloc_size = 0;

	dir = gnome_vfs_uri_to_string (vfs_uri_dir, GNOME_VFS_URI_HIDE_NONE);

	if (!baobab.is_local)
		string_to_display = gnome_vfs_unescape_string_for_display (dir);
	else
		string_to_display = gnome_vfs_format_uri_for_display (dir);

	if (is_excluded_dir (dir))
		goto exit;

	if (strcmp (gnome_vfs_uri_get_path (vfs_uri_dir), "/proc") == 0)
		goto exit;

	/* prefill the model */
	data.size = 1;
	data.alloc_size = 1;
	data.depth = count - 1;
	data.elements = -1;
	data.dir = string_to_display;
	data.tempHLsize = tempHLsize;
	fill_model (&data);

	/* get the directory-entry sizes */
	dir_info = gnome_vfs_file_info_new ();
	gnome_vfs_get_file_info (dir, dir_info,
				 GNOME_VFS_FILE_INFO_DEFAULT);
	if ((dir_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_SIZE) != 0)
		retloop.size = dir_info->size;
	if ((dir_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_BLOCK_COUNT) != 0)
		retloop.alloc_size = dir_info->block_count * dir_info->io_block_size;
	gnome_vfs_file_info_unref (dir_info);

	/* get the GnomeVFSFileInfo stuct for every directory entry */
	result = gnome_vfs_directory_list_load (&file_list,
						dir,
						GNOME_VFS_FILE_INFO_DEFAULT);

	if (result == GNOME_VFS_OK) {
		for (l = file_list; l != NULL; l = l->next) {
			GnomeVFSFileInfo *info = l->data;

			if (baobab.STOP_SCANNING) {
				gnome_vfs_file_info_list_free (file_list);
				goto exit;
			}

			if (strcmp (info->name, ".") == 0 ||
			    strcmp (info->name, "..") == 0)
				continue;

			if ((info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_TYPE) == 0) {
				gnome_vfs_file_info_list_free (file_list);
				goto exit;
			}

			/* is a symlink? */
			if (info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK)
				continue;

			/* is a directory? */
			if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
				new_uri = gnome_vfs_uri_append_file_name (vfs_uri_dir, info->name);
				temp = loopdir (new_uri, count, hla);
				retloop.size += temp.size;
				retloop.alloc_size += temp.alloc_size;
				elements++;
				gnome_vfs_uri_unref (new_uri);
			}

			else if (info->type == GNOME_VFS_FILE_TYPE_REGULAR) {

				/* check for hard links only on local files */
				if (GNOME_VFS_FILE_INFO_LOCAL (info)) {
					if (info->link_count > 1) {

						if (baobab_hardlinks_array_has (hla, info)) {
							tempHLsize += (info->block_count *
							               info->io_block_size);
							continue;
						}
						else {
							baobab_hardlinks_array_add (hla, info);
						}
					}
				}

				if ((info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_BLOCK_COUNT) != 0)
					retloop.alloc_size += (info->block_count * 512);

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
	baobab_hardlinks_array *hla;
	GnomeVFSURI *vfs_uri;

	if (is_excluded_dir (uri_dir))
		return;

	hla = baobab_hardlinks_array_create ();

	vfs_uri = gnome_vfs_uri_new (uri_dir);

	loopdir (vfs_uri, 0, hla);

	baobab_hardlinks_array_free (hla);
	gnome_vfs_uri_unref (vfs_uri);
}

void
searchDir (const gchar *uri_dir, const gchar *search)
{
	GnomeVFSURI *vfs_uri;

	vfs_uri = gnome_vfs_uri_new (uri_dir);
	loopsearch (vfs_uri, search);

	gnome_vfs_uri_unref (vfs_uri);
}
