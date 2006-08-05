/*
 * baobab-utils.c
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
#include <glib/gstdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <monetary.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <glibtop/mountlist.h>
#include <glibtop/fsusage.h>
#include <libgnome/gnome-help.h>

#include "baobab.h"
#include "baobab-treeview.h"
#include "baobab-utils.h"
#include "callbacks.h"

static int radio_group_get_active (GtkRadioButton *);
static void radio_group_set_active (GtkRadioButton *rb, int btn);

void
baobab_get_filesystem (baobab_fs *fs)
{
	size_t i;
	glibtop_mountlist mountlist;
	glibtop_mountentry *mountentries;

	memset (fs, 0, sizeof *fs);

	mountentries = glibtop_get_mountlist (&mountlist, FALSE);

	for (i = 0; i < mountlist.number; ++i) {
		glibtop_fsusage fsusage;

		if (is_excluded_dir (mountentries[i].mountdir))
			continue;

		glibtop_get_fsusage (&fsusage, mountentries[i].mountdir);

		/*  v.1.1.1 changed bavail with bfree) */
		fs->total += fsusage.blocks * fsusage.block_size;
		fs->avail += fsusage.bfree * fsusage.block_size;
		fs->used += (fsusage.blocks - fsusage.bfree) * fsusage.block_size;
	}

	g_free (mountentries);
}

void
set_statusbar (const gchar *text)
{
	GtkWidget *sb;

	sb = glade_xml_get_widget (baobab.main_xml, "statusbar1");
	gtk_statusbar_pop (GTK_STATUSBAR (sb), 1);
	gtk_statusbar_push (GTK_STATUSBAR (sb), 1, text);

	while (gtk_events_pending ())
		gtk_main_iteration ();
}

/*
 * GtkFileChooser to select a directory to scan
 */
gchar *
dir_select (GtkWidget *parent)
{
	GtkWidget *dialog;
	GtkWidget *toggle;

	dialog = gtk_file_chooser_dialog_new (_("Select a Folder"),
					      GTK_WINDOW (parent),
					      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
					      GTK_STOCK_CANCEL,
					      GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OPEN,
					      GTK_RESPONSE_ACCEPT, NULL);

	gtk_file_chooser_set_show_hidden (GTK_FILE_CHOOSER (dialog), TRUE);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),
					     g_get_home_dir ());
	/* add extra widget */
	toggle = gtk_check_button_new_with_label (_("Show hidden folders"));
	gtk_widget_show (toggle);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), TRUE);
	g_signal_connect ((gpointer) toggle, "toggled",
			  G_CALLBACK (on_toggled), dialog);
	gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (dialog),
					   toggle);


	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		char *filename;

		filename = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
		gtk_widget_destroy (dialog);
		start_proc_on_dir (filename);
		g_free (filename);
		return NULL;
		
	} else {
		gtk_widget_destroy (dialog);
		return NULL;
	}
}

void
on_toggled (GtkToggleButton *togglebutton, gpointer dialog)
{
	gtk_file_chooser_set_show_hidden (GTK_FILE_CHOOSER (dialog),
					  !gtk_file_chooser_get_show_hidden
					  (GTK_FILE_CHOOSER (dialog)));
}

GdkPixbuf *
set_bar (gfloat perc)
{
	GdkPixbuf *srcpxb = NULL, *destpxb;
	gint height, width;

	if (perc <= 33.0f)
		srcpxb = baobab.green_bar;
	else if (perc > 33.0f && perc <= 60.0f)
		srcpxb = baobab.yellow_bar;
	else if (perc > 60.0f)
		srcpxb = baobab.red_bar;

	destpxb = baobab_load_pixbuf ("white.png");
	height = gdk_pixbuf_get_height (destpxb);
	width = gdk_pixbuf_get_width (destpxb) - 1;
	width = (gint) (gfloat) ((width * perc) / 100.0f);	/* if bar width ==50 --> $width = $perc/2 */

	gdk_pixbuf_copy_area (srcpxb,
			      1, 1,
			      width - 2, height - 2,
			      destpxb,
			      1, 1);

	return destpxb;
}

void
set_glade_widget_sens (const gchar *name, gboolean sens)
{
	GtkWidget *w;

	w = glade_xml_get_widget (baobab.main_xml, name);
	gtk_widget_set_sensitive (w, sens);
}

/* menu & toolbar sensitivity */
void
check_menu_sens (gboolean scanning)
{
	if (scanning) {

		while (gtk_events_pending ())
			gtk_main_iteration ();

		set_statusbar (_("Scanning..."));
		set_glade_widget_sens ("menu_treemap", FALSE);
	}

	set_glade_widget_sens ("tbstop", scanning);
	set_glade_widget_sens ("tbscanall", !scanning);
	set_glade_widget_sens ("tbscandir", !scanning);
	set_glade_widget_sens ("menuallfs", !scanning);
	set_glade_widget_sens ("menuscandir", !scanning);
	set_glade_widget_sens ("preferenze1", !scanning);
	set_glade_widget_sens ("menu_scan_rem", !scanning);
	set_glade_widget_sens ("tb_scan_remote", !scanning);
	set_glade_widget_sens ("ck_allocated",
			       !scanning &&
			       baobab.is_local);

}

void
stop_scan (void)
{
	set_statusbar (_("Calculating percentage bars..."));
	gtk_tree_model_foreach (GTK_TREE_MODEL (baobab.model),
					show_bars, NULL);
	gtk_tree_view_columns_autosize (GTK_TREE_VIEW (baobab.tree_view));

	set_busy (FALSE);
	check_menu_sens (FALSE);
	set_statusbar (_("Ready"));
}

gboolean
show_bars (GtkTreeModel *mdl,
	   GtkTreePath *path,
	   GtkTreeIter *iter,
	   gpointer data)
{
	GtkTreeIter parent;
	gfloat perc;
	gint readelements, size_col;
	guint64 refsize, size;
	GdkPixbuf *bar;
	gchar textperc[10];
	char *sizecstr = NULL;

	if (baobab.show_allocated)
		size_col = (gint) COL_H_ALLOCSIZE;
	else
		size_col = (gint) COL_H_SIZE;

	if (gtk_tree_model_iter_parent (mdl, &parent, iter)) {
		gtk_tree_model_get (mdl, &parent, COL_H_ELEMENTS,
				    &readelements, -1);
		if (readelements == -1)
			return FALSE;

		gtk_tree_model_get (mdl, iter, COL_H_ELEMENTS,
				    &readelements, -1);
		if (readelements == -1)
			return FALSE;

		gtk_tree_model_get (mdl, &parent, size_col, &refsize, -1);
                gtk_tree_model_get (mdl, iter, size_col, &size, -1);
                perc = (refsize != 0) ? ((gfloat) size * 100) / (gfloat) refsize : 0.0;
		g_sprintf (textperc, " %.1f %%", perc);
		sizecstr = gnome_vfs_format_file_size_for_display (size);
		bar = set_bar (perc);

		gtk_tree_store_set (GTK_TREE_STORE (mdl), iter,
				    COL_BAR, bar,
				    COL_DIR_SIZE, sizecstr,
				    COL_H_PERC, perc,
				    COL_PERC, textperc, -1);

		g_object_unref (bar);
		g_free (sizecstr);
	} else {
		gtk_tree_model_get (mdl, iter, COL_H_ELEMENTS,
				    &readelements, -1);
		if (readelements != -1) {
			gtk_tree_model_get (mdl, iter, size_col, &size,
					    -1);
			sizecstr = gnome_vfs_format_file_size_for_display (size);
			bar = set_bar (100.0);

			gtk_tree_store_set (GTK_TREE_STORE (mdl), iter,
					    COL_BAR, bar,
					    COL_H_PERC, 100.0,
					    COL_PERC, "100 %",
					    COL_DIR_SIZE, sizecstr, -1);

			g_object_unref (bar);
			g_free (sizecstr);
		}
	}

	return FALSE;
}

void
message (gchar *messaggio, GtkWidget *parent)
{
	GtkWidget *dialog;
	dialog = gtk_message_dialog_new (GTK_WINDOW (parent),
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_INFO,
					 GTK_BUTTONS_OK, "%s", messaggio);
	gtk_message_dialog_set_markup ((GtkMessageDialog *) dialog,
				       messaggio);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

gint
messageyesno (gchar *message, GtkWidget *parent)
{
	GtkWidget *dialog;
	gint response;

	dialog = gtk_message_dialog_new (GTK_WINDOW (parent),
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_YES_NO,
					 "%s", message);
	gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog),
				       message);
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	return response;
}

gboolean
baobab_check_dir (const gchar *dirname)
{
	char *error_msg = NULL;
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;
	gboolean ret = TRUE;

	info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info (dirname, info,
					  GNOME_VFS_FILE_INFO_DEFAULT |
					  GNOME_VFS_FILE_INFO_FOLLOW_LINKS);

	if (result != GNOME_VFS_OK)
		error_msg = g_strdup_printf (_("%s is not a valid folder"),
					     dirname);

	if (info->type != GNOME_VFS_FILE_TYPE_DIRECTORY)
		error_msg = g_strdup_printf (_("%s is not a valid folder"),
					     dirname);

	if (error_msg) {
		message (error_msg, baobab.window);
		g_free (error_msg);
		ret = FALSE;
	}

	gnome_vfs_file_info_unref (info);

	return ret;
}

void
popupmenu_list (GtkTreePath *path, GdkEventButton *event, gboolean is_trash)
{
	GtkWidget *pmenu, *open, *trash, *sep,  *graph_map, *remove;
	gchar *path_to_string;
	GtkWidget *image;

	/* path_to_string is freed in callback function */
	path_to_string = gtk_tree_path_to_string (path);

	pmenu = gtk_menu_new ();
	open = gtk_image_menu_item_new_from_stock ("gtk-open", NULL);
	remove = gtk_image_menu_item_new_with_label(_("Remove from Trash"));
	image = gtk_image_new_from_stock ("gtk-delete", GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (remove), image);
	trash = gtk_image_menu_item_new_with_label(_("Move to Trash"));
	image = gtk_image_new_from_stock ("gtk-delete", GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (trash), image);
	graph_map = gtk_image_menu_item_new_with_label (_("Folder graphical map"));
	image = gtk_image_new_from_stock ("gtk-select-color", GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (graph_map), image);
	sep = gtk_separator_menu_item_new ();
	

	g_signal_connect (open, "activate",
			  G_CALLBACK (open_file_cb), NULL);
	g_signal_connect (trash, "activate",
			  G_CALLBACK (trash_dir_cb), NULL);
	g_signal_connect (remove, "activate",
			  G_CALLBACK (trash_dir_cb), NULL);
	g_signal_connect (graph_map, "activate",
			  G_CALLBACK (graph_map_cb), path_to_string);

	gtk_container_add (GTK_CONTAINER (pmenu), open);
	gtk_container_add (GTK_CONTAINER (pmenu), graph_map);
	
	if (baobab.is_local) {
		gtk_container_add (GTK_CONTAINER (pmenu), sep);
		if (is_trash)
			gtk_container_add (GTK_CONTAINER (pmenu), remove);
		else
			gtk_container_add (GTK_CONTAINER (pmenu), trash);		
	}
	
	gtk_widget_show_all (pmenu);
	gtk_menu_popup (GTK_MENU (pmenu), NULL, NULL, NULL, NULL,
			event->button, event->time);
}


int
radio_group_get_active (GtkRadioButton *rb)
{
	GSList *list, *l;
	int i;

	list = gtk_radio_button_get_group (rb);

	for (l = list, i = 0; l; l = l->next, i++) {
		if (gtk_toggle_button_get_active
		    (GTK_TOGGLE_BUTTON (l->data)))
			break;
	}

	return g_slist_length (list) - i;
}

void
radio_group_set_active (GtkRadioButton *rb, int btn)
{
	GSList *list, *l;
	int i;

	list = gtk_radio_button_get_group (rb);
	btn = g_slist_length (list) - btn;

	for (l = list, i = 0; l; l = l->next, i++) {
		if (i == btn) {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
						      (l->data), TRUE);
			break;
		}
	}
}

void
set_label_scan (baobab_fs *fs)
{
	gchar *markup, *size;
	GString *total;
	GString *used;
	GString *avail;

	if (baobab.label_scan)
		g_free (baobab.label_scan);

	size = gnome_vfs_format_file_size_for_display (fs->total);
	total = g_string_new (size);
	g_free (size);
	size = gnome_vfs_format_file_size_for_display (fs->used);
	used = g_string_new (size);
	g_free (size);

	size = gnome_vfs_format_file_size_for_display (fs->avail);
	avail = g_string_new (size);
	g_free (size);

	markup = g_markup_printf_escaped  ("<small>%s <b>%s</b> (%s %s %s %s )</small>",
					   _("Total filesystem capacity:"), total->str, _("used:"),
					   used->str, _("available:"), avail->str);
	baobab.label_scan = strdup (markup);

	g_string_free (total, TRUE);
	g_string_free (used, TRUE);
	g_string_free (avail, TRUE);
	g_free (markup);
}

void
show_label (gint view)
{
	GtkWidget *label;

	label = glade_xml_get_widget (baobab.main_xml, "label1");

	if (view == VIEW_TREE) {
		gtk_label_set_markup (GTK_LABEL (label),
				      baobab.label_scan);
	}
	else {
		g_assert_not_reached ();
	}
}

void
open_file_with_application (gchar *file)
{
	GnomeVFSMimeApplication *application;
	gchar *mime;
	GnomeVFSFileInfo *info;
	GnomeVFSURI *vfs_uri;

	info = gnome_vfs_file_info_new ();
	vfs_uri = gnome_vfs_uri_new (file);

	gnome_vfs_get_file_info (file, info,
				 GNOME_VFS_FILE_INFO_GET_MIME_TYPE);
	mime = info->mime_type;

	application = gnome_vfs_mime_get_default_application (mime);

	if (!application) {
		if ((g_file_test (file, G_FILE_TEST_IS_EXECUTABLE)) &&
		    (g_ascii_strcasecmp (mime, "application/x-executable") == 0) &&
		     gnome_vfs_uri_is_local (vfs_uri)) {
			g_spawn_command_line_async (file, NULL);
		}
		else {
			message (_("There is no installed viewer capable "
				   "of displaying the document."),
				 baobab.window);
		}
	}
	else {
		GList *uris = NULL;
		gchar *uri;

		uri = gnome_vfs_uri_to_string (vfs_uri, GNOME_VFS_URI_HIDE_NONE);
		uris = g_list_append (uris, uri);
		gnome_vfs_mime_application_launch (application, uris);

		g_list_free (uris);
		g_free (uri);
	}

	gnome_vfs_uri_unref (vfs_uri);
	gnome_vfs_mime_application_free (application);
	gnome_vfs_file_info_unref (info);
}

gchar *
get_trash_path (const gchar *file)
{
	GnomeVFSURI *trash_uri;
	GnomeVFSURI *uri;
	gchar *filename;

	filename = gnome_vfs_escape_path_string (file);
	uri = gnome_vfs_uri_new (file);
	g_free (filename);

	gnome_vfs_find_directory (uri,
				  GNOME_VFS_DIRECTORY_KIND_TRASH,
				  &trash_uri, TRUE, TRUE, 0777);
	gnome_vfs_uri_unref (uri);

	if (trash_uri == NULL) {
		return NULL;
	} else {
		gchar *trash_path;
		trash_path =
		    gnome_vfs_uri_to_string (trash_uri,
					     GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD);
		gnome_vfs_uri_unref (trash_uri);
		return trash_path;
	}
}

gboolean
trash_file (const gchar *filename)
{
	gchar *trash_path = NULL;
	gchar *basename = NULL;
	gchar *destination = NULL;
	GnomeVFSResult result;
	gboolean ret = FALSE;

	trash_path = get_trash_path (filename);

	if (trash_path == NULL) {
		message (_("Cannot find the Trash on this system!"),
			 baobab.window);
		goto out;
	}

	if ((!g_file_test (filename, G_FILE_TEST_EXISTS)) &&
	    (!g_file_test (filename, G_FILE_TEST_IS_SYMLINK))) {
		message (_("The document does not exist."), baobab.window);
		goto out;
	}

	basename = g_path_get_basename (filename);
	destination = g_build_filename (trash_path, basename, NULL);

	result = gnome_vfs_move (filename, destination, TRUE);
	
	if (result != GNOME_VFS_OK) {
		gchar *mess;

		mess = g_strdup_printf (_("Moving <b>%s</b> to trash failed: %s."),
					  g_path_get_basename (filename),
					  gnome_vfs_result_to_string (result));
		message (mess, baobab.window);
		g_free (mess);
		goto out;
	}
	else
		ret = TRUE;

	if (strcmp (destination, filename) == 0) {
		gchar *mess;
		gint response;

		mess = g_strdup_printf (_("Do you want to delete <b>%s</b> permanently?"),
					  g_path_get_basename (filename));
		response = messageyesno (mess, baobab.window);
		g_free (mess);

		if (response == GTK_RESPONSE_YES) {
			if (!g_file_test (filename, G_FILE_TEST_IS_DIR))
				result = gnome_vfs_unlink (filename);
			else
				result = gnome_vfs_remove_directory (filename);

			if (result != GNOME_VFS_OK) {
				mess = g_strdup_printf (_("Deleting <b>%s</b> failed: %s."),
							  g_path_get_basename (filename),
							  gnome_vfs_result_to_string (result));
				message (mess, baobab.window);
				g_free (mess);
				ret = FALSE;
				goto out;
			}

			/* success */
			ret = TRUE;
		}
	}

 out:
	g_free (basename);
	g_free (trash_path);
	g_free (destination);

	return ret;
}

void
contents_changed (void)
{
	baobab_get_filesystem (&g_fs);
	set_label_scan (&g_fs);
	show_label (VIEW_TREE);

	if (messageyesno (_("The content of your home directory has changed.\n"
			    "Do you want to rescan the last tree to update the folder branch details?"),
			  baobab.window) == GTK_RESPONSE_YES) {

		start_proc_on_dir (baobab.last_scan_command->str);
	}
}

GdkPixbuf *
baobab_load_pixbuf (const gchar *filename)
{
	gchar *pathname = NULL;
	GdkPixbuf *pixbuf;
	GError *error = NULL;

	if (!filename || !filename[0])
		return NULL;

	pathname = g_build_filename (BAOBAB_PIX_DIR, filename, NULL);

	if (!pathname) {
		g_warning (_("Couldn't find pixmap file: %s"), filename);
		return NULL;
	}

	pixbuf = gdk_pixbuf_new_from_file (pathname, &error);
	if (!pixbuf) {
		fprintf (stderr, "Failed to load pixbuf file: %s: %s\n",
			 pathname, error->message);
		g_error_free (error);
	}
	g_free (pathname);

	return pixbuf;
}

gboolean
baobab_help_display (GtkWindow   *parent,
		     const gchar *file_name,
		     const gchar *link_id)
{
	GError *error = NULL;
	gboolean ret;

	ret = gnome_help_display (file_name,
				  link_id,
				  &error);

	if (error != NULL) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (parent,
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE, 
						 _("There was an error displaying help."));

		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  error->message);

		g_signal_connect (G_OBJECT (dialog), "response",
				  G_CALLBACK (gtk_widget_destroy), NULL);

		gtk_widget_show (dialog);

		g_error_free (error);
	}

	return ret;
}
