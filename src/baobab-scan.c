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

#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

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
   GFileInfo.

   EDIT: /me stupid. I realize that this code was not called that often
   1 call per file with st_nlink > 1. BUT, i'm using pdumpfs to backup
   my /etc. pdumpfs massively uses hard links. So there are more than
   5000 files with st_nlink > 1. I believe this is the worst case.
*/

typedef struct {
	guint64		    inode;
	dev_t               device;
} BaobabHardLink;

typedef GArray BaobabHardLinkArray;

static BaobabHardLinkArray *
baobab_hardlinks_array_create (void)
{
	return g_array_new (FALSE, FALSE, sizeof(BaobabHardLink));
}

static gboolean
baobab_hardlinks_array_has (BaobabHardLinkArray   *a,
			    GFileInfo  		  *s)
{
	guint i;

	for (i = 0; i < a->len; ++i) {
		BaobabHardLink *cur = &g_array_index (a, BaobabHardLink, i);

		/*
		 * cur->st_dev == s->st_dev is the common case and may be more
		 * expansive than cur->st_ino == s->st_ino
		 * so keep this order */
		if (cur->inode == g_file_info_get_attribute_uint64 (s,
							G_FILE_ATTRIBUTE_UNIX_INODE) &&
		    cur->device == g_file_info_get_attribute_uint32 (s,
							G_FILE_ATTRIBUTE_UNIX_DEVICE))
			return TRUE;
	}

	return FALSE;
}

/* return FALSE if the element was already in the array */
static gboolean
baobab_hardlinks_array_add (BaobabHardLinkArray *a,
			    GFileInfo    *s)
{
	BaobabHardLink hl;

	if (baobab_hardlinks_array_has (a, s))
		return FALSE;

	hl.inode = g_file_info_get_attribute_uint64 (s,
				G_FILE_ATTRIBUTE_UNIX_INODE);
	hl.device = g_file_info_get_attribute_uint32 (s,
				G_FILE_ATTRIBUTE_UNIX_DEVICE);
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
	goffset size;
	goffset alloc_size;
};

static const char *dir_attributes = 
	G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_TYPE "," \
	G_FILE_ATTRIBUTE_STANDARD_SIZE "," G_FILE_ATTRIBUTE_UNIX_BLOCKS "," \
	G_FILE_ATTRIBUTE_ACCESS_CAN_READ;

static const char *child_attributes =
	G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_TYPE "," \
	G_FILE_ATTRIBUTE_UNIX_INODE "," G_FILE_ATTRIBUTE_UNIX_DEVICE;


static struct allsizes
loopdir (GFile	*file,
	 guint count,
	 BaobabHardLinkArray *hla)
{
	GList *file_list;
	guint64 tempHLsize = 0;
	gint elements = 0;
	struct chan_data data;
	struct allsizes retloop, temp;
	GFileInfo *dir_info, *temp_info;
	GFileEnumerator *file_enum;
	gchar *dir_uri = NULL;
	gchar *dir_path = NULL;
	gchar *string_to_display = NULL;
	GError *err = NULL;

	count++;
	retloop.size = 0;
	retloop.alloc_size = 0;
	dir_uri = g_file_get_uri (file);
	dir_path = g_file_get_path (file);
 
	dir_info = g_file_query_info (file, dir_attributes,
				      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				      NULL, &err);

	if (dir_info == NULL && err != NULL) {
		g_warning ("couldn't get info for dir %s: %s\n",
			   dir_path,
			   err->message);
		goto exit;
	}

	/* Skip the virtual file systems */
	if ((strcmp (dir_path, "/proc") == 0) ||
            (strcmp (dir_path, "/sys") == 0))
 		goto exit;


	/* Skip the user excluded folders */
	if (baobab_is_excluded_location (file))
		goto exit;

	string_to_display = g_file_get_parse_name (file);	
	
	/* Folders we can't access (e.g perms 644). Skip'em. */
	if (!g_file_info_get_attribute_boolean(dir_info, G_FILE_ATTRIBUTE_ACCESS_CAN_READ))
		goto exit;
	if (g_file_info_get_file_type (dir_info) == G_FILE_TYPE_UNKNOWN)
		goto exit;

	if (g_file_info_has_attribute (dir_info, G_FILE_ATTRIBUTE_STANDARD_SIZE))
		retloop.size = g_file_info_get_size (dir_info);
	if (g_file_info_has_attribute (dir_info, G_FILE_ATTRIBUTE_UNIX_BLOCKS))
		retloop.alloc_size = BLOCK_SIZE * 
			g_file_info_get_attribute_uint64 (dir_info,
							  G_FILE_ATTRIBUTE_UNIX_BLOCKS);
 
	/* load up the file enumerator */
	file_enum = g_file_enumerate_children (file,
					child_attributes,
					G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
					NULL, &err);

	if (file_enum == NULL && err != NULL) {
		g_warning ("couldn't get dir enum for dir %s: %s\n",
			   dir_path,
			   err->message);
		goto exit;
	}


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

	g_clear_error (&err);
	while ((temp_info = g_file_enumerator_next_file (file_enum,
							 NULL,
							 &err)) != NULL) {
		GFileType temp_type = g_file_info_get_file_type (temp_info);
		if (baobab.STOP_SCANNING) {
			g_object_unref (file_enum);
			goto exit;
		}

		/* is a symlink? */
		if (temp_type == G_FILE_TYPE_SYMBOLIC_LINK)
			continue;
 
		/* is a directory? */
		if (temp_type == G_FILE_TYPE_DIRECTORY) {
			GFile *child_dir = g_file_get_child (file, 
						g_file_info_get_name (temp_info));
			temp = loopdir (child_dir, count, hla);
			retloop.size += temp.size;
			retloop.alloc_size += temp.alloc_size;
			elements++;
			g_object_unref (child_dir);
		}

		/* is it a regular file? */
		else if (temp_type == G_FILE_TYPE_REGULAR) {
 
			/* check for hard links only on local files */
			if (g_file_info_has_attribute (temp_info,
						       G_FILE_ATTRIBUTE_UNIX_NLINK) &&
				g_file_info_get_attribute_uint32 (temp_info,
							    G_FILE_ATTRIBUTE_UNIX_NLINK) > 1) {
 
				if (!baobab_hardlinks_array_add (hla, temp_info)) {

					/* we already acconted for it */
					tempHLsize += g_file_info_get_size (temp_info);
					continue;
				}
			}

			if (g_file_info_has_attribute (temp_info, G_FILE_ATTRIBUTE_UNIX_BLOCKS)) {
				retloop.alloc_size += BLOCK_SIZE *
					g_file_info_get_attribute_uint64 (temp_info,
							G_FILE_ATTRIBUTE_UNIX_BLOCKS);
			}
			retloop.size += g_file_info_get_size (temp_info);
			elements++;
		}
	}

	/* won't be an error if we've finished normally */
	if (err != NULL) {
		g_warning ("error in dir %s: %s\n", 
			   dir_path, err->message);
	}

	data.dir = string_to_display;
	data.size = retloop.size;
	data.alloc_size = retloop.alloc_size;
	data.depth = count - 1;
	data.elements = elements;
	data.tempHLsize = tempHLsize;
	fill_model (&data);
	g_object_unref (file_enum);

 exit:
 	g_object_unref (dir_info);
	g_free (dir_uri);
	g_free (dir_path);
	g_free (string_to_display);

	return retloop;
}

void
getDir (const gchar *uri_dir)
{
	BaobabHardLinkArray *hla;
	GFile *file;
	
	file = g_file_new_for_uri (uri_dir);
	
	if (baobab_is_excluded_location (file)) {
		g_object_unref (file);
		return;
		}

	hla = baobab_hardlinks_array_create ();

	loopdir (file, 0, hla);

	baobab_hardlinks_array_free (hla);
	g_object_unref (file);
}
