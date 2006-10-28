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
filechooser_cb (GtkWidget * chooser,
                 gint response,
                 gpointer data)
{
	if (response != GTK_RESPONSE_OK) {
		gtk_widget_hide (chooser);
	}
	else {
		char *filename;

		filename = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (chooser));
		gtk_widget_hide (chooser);
		start_proc_on_dir (filename);
		g_free (filename);
	}
	
}


/*
 * GtkFileChooser to select a directory to scan
 */
gchar *
dir_select (gboolean SEARCH, GtkWidget *parent)
{
	static GtkWidget *file_chooser = NULL;
	GtkWidget *toggle;

	if (file_chooser == NULL) {
		file_chooser = gtk_file_chooser_dialog_new (_("Select Folder"),
					      GTK_WINDOW (parent),
					      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
					      GTK_STOCK_CANCEL,
					      GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OPEN,
					      GTK_RESPONSE_OK, NULL);

		gtk_file_chooser_set_show_hidden (GTK_FILE_CHOOSER (file_chooser), FALSE);
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (file_chooser),
					     g_get_home_dir ());
		/* add extra widget */
		toggle = gtk_check_button_new_with_mnemonic (_("_Show hidden folders"));
		gtk_widget_show (toggle);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), FALSE);
		g_signal_connect ((gpointer) toggle, "toggled",
				  G_CALLBACK (on_toggled), file_chooser);
		gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (file_chooser),
						   toggle);

		g_signal_connect (G_OBJECT (file_chooser), "response",
				  G_CALLBACK (filechooser_cb), NULL);

		gtk_window_set_modal (GTK_WINDOW (file_chooser), TRUE);
		gtk_window_set_position (GTK_WINDOW (file_chooser), GTK_WIN_POS_CENTER_ON_PARENT);
	}
	
	gtk_widget_show (GTK_WIDGET (file_chooser));

	return NULL;
}

void
on_toggled (GtkToggleButton *togglebutton, gpointer dialog)
{
	gtk_file_chooser_set_show_hidden (GTK_FILE_CHOOSER (dialog),
					  !gtk_file_chooser_get_show_hidden
					  (GTK_FILE_CHOOSER (dialog)));
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
		set_glade_widget_sens ("expand_all", TRUE);
		set_glade_widget_sens ("collapse_all", TRUE);		
	}

	set_glade_widget_sens ("tbscanhome", !scanning);
	set_glade_widget_sens ("tbscanall", !scanning);
	set_glade_widget_sens ("tbscandir", !scanning);
	set_glade_widget_sens ("menuscanhome", !scanning);
	set_glade_widget_sens ("menuallfs", !scanning);
	set_glade_widget_sens ("menuscandir", !scanning);
	set_glade_widget_sens ("tbstop", scanning);
	set_glade_widget_sens ("tbrescan", !scanning);
	set_glade_widget_sens ("menustop", scanning);
	set_glade_widget_sens ("menurescan", !scanning);
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
	gchar textperc[10];
	char *sizecstr = NULL;

	if (baobab.show_allocated)
		size_col = (gint) COL_H_ALLOCSIZE;
	else
		size_col = (gint) COL_H_SIZE;

	if (gtk_tree_model_iter_parent (mdl, &parent, iter)) {
		gtk_tree_model_get (mdl, iter, COL_H_ELEMENTS,
				    &readelements, -1);
		if (readelements == -1)
		  {
		  	gtk_tree_store_set (GTK_TREE_STORE (mdl), iter,
				    COL_DIR_SIZE, "--",
				    COL_ELEMENTS, "--", -1);				    
			return FALSE;
		  }

 		gtk_tree_model_get (mdl, &parent, COL_H_ELEMENTS,
 				    &readelements, -1);
                
                gtk_tree_model_get (mdl, iter, size_col, &size, -1);
                sizecstr = gnome_vfs_format_file_size_for_display (size);

 		if (readelements == -1)
                  {
                    gtk_tree_store_set (GTK_TREE_STORE (mdl), iter,
                                        COL_DIR_SIZE, sizecstr, -1);
                    
                    g_free (sizecstr);
                    return FALSE;
                  }

		gtk_tree_model_get (mdl, &parent, size_col, &refsize, -1);
                perc = (refsize != 0) ? ((gfloat) size * 100) / (gfloat) refsize : 0.0;
		g_sprintf (textperc, " %.1f %%", perc);

		gtk_tree_store_set (GTK_TREE_STORE (mdl), iter,
				    COL_DIR_SIZE, sizecstr,
				    COL_H_PERC, perc,
				    COL_PERC, textperc, -1);

		g_free (sizecstr);
	} else {
		gtk_tree_model_get (mdl, iter, COL_H_ELEMENTS,
				    &readelements, -1);
		if (readelements != -1) {
			gtk_tree_model_get (mdl, iter, size_col, &size,
					    -1);
			sizecstr = gnome_vfs_format_file_size_for_display (size);

			gtk_tree_store_set (GTK_TREE_STORE (mdl), iter,
					    COL_H_PERC, 100.0,
					    COL_PERC, "100 %",
					    COL_DIR_SIZE, sizecstr, -1);

			g_free (sizecstr);
		}
		else {
			gtk_tree_store_set (GTK_TREE_STORE (mdl), iter,
		  		 	    COL_DIR_SIZE, "--",
					    COL_ELEMENTS, "--", -1);
		}
	}

	return FALSE;
}

void
message (gchar *primary_msg, gchar *secondary_msg, GtkMessageType type, GtkWidget *parent)
{
	GtkWidget *dialog;
	dialog = gtk_message_dialog_new (GTK_WINDOW (parent),
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 type,
					 GTK_BUTTONS_OK, "%s", primary_msg);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
	                                          secondary_msg);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

gint
messageyesno (gchar *primary_msg, gchar *secondary_msg, GtkMessageType type, gchar *ok_button, GtkWidget *parent)
{
	GtkWidget *dialog;
	GtkWidget *button;
	gint response;

	dialog = gtk_message_dialog_new (GTK_WINDOW (parent),
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 type,
					 GTK_BUTTONS_CANCEL,
					 "%s", primary_msg);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
	                                          secondary_msg);
						  
	button = gtk_button_new_with_mnemonic (ok_button);
	GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
	gtk_widget_show (button);
	
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
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
		error_msg = g_strdup_printf (_("\"%s\" is not a valid folder"),
					     dirname);

	if (info->type != GNOME_VFS_FILE_TYPE_DIRECTORY)
		error_msg = g_strdup_printf (_("\"%s\" is not a valid folder"),
					     dirname);

	if (error_msg) {
		message (error_msg, _("Could not analyze disk usage."), 
		         GTK_MESSAGE_ERROR, baobab.window);
		g_free (error_msg);
		ret = FALSE;
	}

	gnome_vfs_file_info_unref (info);

	return ret;
}

void
popupmenu_list (GtkTreePath *path, GdkEventButton *event, gboolean is_trash)
{
	GtkWidget *pmenu, *open, *trash, *sep, *graph_map, *remove;
	gchar *path_to_string;
	GtkWidget *image;

	/* path_to_string is freed in callback function */
	path_to_string = gtk_tree_path_to_string (path);

	pmenu = gtk_menu_new ();

	image = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_MENU);
	open = gtk_image_menu_item_new_with_mnemonic(_("_Open Folder"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (open), image);

	graph_map = gtk_image_menu_item_new_with_mnemonic (_("_Graphical Usage Map"));
	image = gtk_image_new_from_stock ("gtk-select-color", GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (graph_map), image);

	sep = gtk_separator_menu_item_new ();

	g_signal_connect (open, "activate",
			  G_CALLBACK (open_file_cb), NULL);
	g_signal_connect (graph_map, "activate",
			  G_CALLBACK (graph_map_cb), path_to_string);

	gtk_container_add (GTK_CONTAINER (pmenu), open);
	gtk_container_add (GTK_CONTAINER (pmenu), graph_map);
	
	if (baobab.is_local) {
		gtk_container_add (GTK_CONTAINER (pmenu), sep);

		if (is_trash) {
			remove = gtk_image_menu_item_new_with_mnemonic(_("_Remove from Trash"));
			image = gtk_image_new_from_stock ("gtk-undelete", GTK_ICON_SIZE_MENU);
			gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (remove), image);

			g_signal_connect (remove, "activate",
					  G_CALLBACK (trash_dir_cb), NULL);

			gtk_container_add (GTK_CONTAINER (pmenu), remove);
		}
		else {
			trash = gtk_image_menu_item_new_with_mnemonic(_("Mo_ve to Trash"));
			image = gtk_image_new_from_stock ("gtk-delete", GTK_ICON_SIZE_MENU);
			gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (trash), image);

			g_signal_connect (trash, "activate",
					  G_CALLBACK (trash_dir_cb), NULL);

			gtk_container_add (GTK_CONTAINER (pmenu), trash);
		}
	}

	gtk_widget_show_all (pmenu);
	gtk_menu_popup (GTK_MENU (pmenu), NULL, NULL, NULL, NULL,
			event->button, event->time);
}

void
set_label_scan (baobab_fs *fs)
{
	gchar *markup;
	gchar *total;
	gchar *used;
	gchar *available;

	g_free (baobab.label_scan);

	total = gnome_vfs_format_file_size_for_display (fs->total);
	used = gnome_vfs_format_file_size_for_display (fs->used);
	available = gnome_vfs_format_file_size_for_display (fs->avail);

	/* Translators: these are labels for disk space */
	markup = g_markup_printf_escaped  ("<small>%s <b>%s</b> (%s %s %s %s )</small>",
					   _("Total filesystem capacity:"), total,
					   _("used:"), used,
					   _("available:"), available);

	baobab.label_scan = markup;

	g_free (total);
	g_free (used);
	g_free (available);
}

void
show_label (void)
{
	GtkWidget *label;

	label = glade_xml_get_widget (baobab.main_xml, "label1");

	gtk_label_set_markup (GTK_LABEL (label),
			      baobab.label_scan);

}

void
open_file_with_application (gchar *file)
{
	GnomeVFSMimeApplication *application;
	gchar *mime;
	gchar *primary;
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
			primary = g_strdup_printf (_("Could not open folder \"%s\""), 
			                           g_path_get_basename (file));
			message (primary, 
			         _("There is no installed viewer capable "
				   "of displaying the folder."),
				 GTK_MESSAGE_ERROR,
				 baobab.window);
			g_free (primary);
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
	gchar *str = NULL;
	GnomeVFSResult result;
	gboolean ret = FALSE;

	trash_path = get_trash_path (filename);

	if (trash_path == NULL) {
		str = g_strdup_printf (_("The folder \"%s\" was not moved to the trash."),
		                       g_path_get_basename (filename));
		message (_("Could not find a Trash folder on this system"), 
		         str, GTK_MESSAGE_ERROR, baobab.window);
		g_free (str);
		goto out;
	}

	if ((!g_file_test (filename, G_FILE_TEST_EXISTS)) &&
	    (!g_file_test (filename, G_FILE_TEST_IS_SYMLINK))) {
	    	str = g_strdup_printf (_("Could not move \"%s\" to the Trash"), g_path_get_basename (filename));
		message (str, _("The folder does not exist."), GTK_MESSAGE_ERROR, baobab.window);
		g_free (str);
		goto out;
	}

	basename = g_path_get_basename (filename);
	destination = g_build_filename (trash_path, basename, NULL);

	result = gnome_vfs_move (filename, destination, TRUE);
	
	if (result != GNOME_VFS_OK) {
		gchar *mess;

		str = g_strdup_printf (_("Could not move \"%s\" to the Trash"), g_path_get_basename (filename));
		mess = g_strdup_printf (_("Details: %s"),
					  gnome_vfs_result_to_string (result));
		message (str, mess, GTK_MESSAGE_ERROR, baobab.window);
		g_free (str);
		g_free (mess);
		goto out;
	}
	else
		ret = TRUE;

	if (strcmp (destination, filename) == 0) {
		gchar *mess;
		gint response;

		mess = g_strdup_printf (_("Delete the folder \"%s\" permanently?"),
					  g_path_get_basename (filename));
		response = messageyesno (mess, 
		                         _("Could not move the folder to the trash."), GTK_MESSAGE_WARNING,
					 _("_Delete Folder"), 
					 baobab.window);
		g_free (mess);

		if (response == GTK_RESPONSE_OK) {
			if (!g_file_test (filename, G_FILE_TEST_IS_DIR))
				result = gnome_vfs_unlink (filename);
			else
				result = gnome_vfs_remove_directory (filename);

			if (result != GNOME_VFS_OK) {
				mess = g_strdup_printf (_("Deleting \"%s\" failed: %s."),
							  g_path_get_basename (filename),
							  gnome_vfs_result_to_string (result));
				str = g_strdup_printf (_("Could not delete the folder \"%s\""), g_path_get_basename (filename));							  
				message (str, mess, GTK_MESSAGE_ERROR, baobab.window);
				g_free (mess);
				g_free (str);
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
	show_label ();

	if (messageyesno (_("Rescan your home folder?"), 
			  _("The content of your home folder has changed. Select rescan to update the disk usage details."),
			  GTK_MESSAGE_QUESTION, _("_Rescan"), baobab.window) == GTK_RESPONSE_OK) {

		start_proc_on_dir (baobab.last_scan_command->str);
	}
}

gchar *
baobab_gconf_get_string_with_default (GConfClient *client,
				      const gchar *key,
				      const gchar *def)
{
	gchar *val;

	val = gconf_client_get_string (client, key, NULL);
	return val ? val : g_strdup (def);
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
