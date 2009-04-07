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
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glibtop/mountlist.h>
#include <glibtop/fsusage.h>

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
		GFile	*file;
		glibtop_fsusage fsusage;
		
		file = g_file_new_for_path(mountentries[i].mountdir);	

		if (baobab_is_excluded_location (file)){
			g_object_unref(file);
			continue;
			}

		glibtop_get_fsusage (&fsusage, mountentries[i].mountdir);

		/*  v.1.1.1 changed bavail with bfree) */
		fs->total += fsusage.blocks * fsusage.block_size;
		fs->avail += fsusage.bfree * fsusage.block_size;
		fs->used += (fsusage.blocks - fsusage.bfree) * fsusage.block_size;
		g_object_unref(file);

	}

	g_free (mountentries);
}

void
filechooser_cb (GtkWidget *chooser,
                gint response,
                gpointer data)
{
	if (response == GTK_RESPONSE_OK) {
		gchar *filename;
		GFile	*file;

		filename = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (chooser));
		gtk_widget_hide (chooser);

		file = g_file_new_for_uri (filename);
		baobab_scan_location (file);
		g_free (filename);
		g_object_unref (file);
	}
	else {
		gtk_widget_hide (chooser);
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

		g_signal_connect (file_chooser, "response",
				  G_CALLBACK (filechooser_cb), NULL);
		g_signal_connect (file_chooser, "destroy",
				  G_CALLBACK (gtk_widget_destroyed), &file_chooser);

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
set_ui_action_sens (const gchar *name, gboolean sens)
{
	GtkAction *a;

	a = GTK_ACTION (gtk_builder_get_object (baobab.main_ui, name));
	gtk_action_set_sensitive (a, sens);
}

void
set_ui_widget_sens (const gchar *name, gboolean sens)
{
	GtkWidget *w;

	w = GTK_WIDGET (gtk_builder_get_object (baobab.main_ui, name));
	gtk_widget_set_sensitive (w, sens);
}

gboolean
show_bars (GtkTreeModel *mdl,
	   GtkTreePath *path,
	   GtkTreeIter *iter,
	   gpointer data)
{
	GtkTreeIter parent;
	gdouble perc;
	gint readelements, size_col;
	guint64 refsize, size;
	char *sizecstr = NULL;

	if (baobab.show_allocated)
		size_col = (gint) COL_H_ALLOCSIZE;
	else
		size_col = (gint) COL_H_SIZE;

	if (gtk_tree_model_iter_parent (mdl, &parent, iter)) {
		gtk_tree_model_get (mdl, iter, COL_H_ELEMENTS,
				    &readelements, -1);

		if (readelements == -1) {
			gtk_tree_store_set (GTK_TREE_STORE (mdl), iter,
					    COL_DIR_SIZE, "--",
					    COL_ELEMENTS, "--", -1);				    
			return FALSE;
		}

 		gtk_tree_model_get (mdl, &parent, COL_H_ELEMENTS,
 				    &readelements, -1);
                
                gtk_tree_model_get (mdl, iter, size_col, &size, -1);
                sizecstr = g_format_size_for_display (size);

 		if (readelements == -1) {
			gtk_tree_store_set (GTK_TREE_STORE (mdl), iter,
				            COL_DIR_SIZE, sizecstr, -1);

			g_free (sizecstr);
			return FALSE;
		}

		gtk_tree_model_get (mdl, &parent, size_col, &refsize, -1);
		perc = (refsize != 0) ? ((gdouble) size * 100) / (gdouble) refsize : 0.0;

		gtk_tree_store_set (GTK_TREE_STORE (mdl), iter,
				    COL_DIR_SIZE, sizecstr,
				    COL_H_PERC, perc, -1);

		g_free (sizecstr);
	} else {
		gtk_tree_model_get (mdl, iter, COL_H_ELEMENTS,
				    &readelements, -1);

		if (readelements != -1) {
			gtk_tree_model_get (mdl, iter, size_col, &size,
					    -1);
			sizecstr = g_format_size_for_display (size);

			gtk_tree_store_set (GTK_TREE_STORE (mdl), iter,
					    COL_H_PERC, 100.0,
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
baobab_check_dir (GFile	*file)
{
	GFileInfo *info;
	GError *error = NULL;
	gboolean ret = TRUE;

	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_TYPE,
				  G_FILE_QUERY_INFO_NONE,
				  NULL,
				  &error);

	if (!info) {
		message("", error->message, GTK_MESSAGE_INFO, baobab.window);
		g_error_free (error);

		return FALSE;
	}	

	if ((g_file_info_get_file_type (info) != G_FILE_TYPE_DIRECTORY) ||
		is_virtual_filesystem(file)) {
		
		char *error_msg = NULL;
		gchar *name = NULL;

	    	name = g_file_get_parse_name (file);
		error_msg = g_strdup_printf (_("\"%s\" is not a valid folder"),
					     name);

		message (error_msg, _("Could not analyze disk usage."), 
		         GTK_MESSAGE_ERROR, baobab.window);

		g_free (error_msg);
		g_free (name);
		ret = FALSE;
	}

	g_object_unref(info);

	return ret;
}

static void
add_popupmenu_item (GtkMenu *pmenu, const gchar *label, const gchar *stock, GCallback item_cb)
{
	GtkWidget *item;
	GtkWidget *image;

	item = gtk_image_menu_item_new_with_mnemonic (label);
	image = gtk_image_new_from_stock (stock, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

	g_signal_connect (item, "activate", item_cb, NULL);

	gtk_container_add (GTK_CONTAINER (pmenu), item);
}

void
popupmenu_list (GtkTreePath *path, GdkEventButton *event, gboolean can_trash)
{
	GtkWidget *pmenu, *open, *trash, *remove;
	gchar *path_to_string;
	GtkWidget *image;

	/* path_to_string is freed in callback function */
	path_to_string = gtk_tree_path_to_string (path);

	pmenu = gtk_menu_new ();

	add_popupmenu_item (GTK_MENU (pmenu),
			    _("_Open Folder"),
			    "gtk-open",
			    G_CALLBACK (open_file_cb));

	if (baobab.is_local && can_trash) {
		add_popupmenu_item (GTK_MENU (pmenu),
				    _("Mo_ve to Trash"),
				    "gtk-delete",
				    G_CALLBACK (trash_dir_cb));
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

	total = g_format_size_for_display (fs->total);
	used = g_format_size_for_display (fs->used);
	available = g_format_size_for_display (fs->avail);

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

	label = GTK_WIDGET (gtk_builder_get_object (baobab.main_ui, "label1"));

	gtk_label_set_markup (GTK_LABEL (label),
			      baobab.label_scan);
}

void
open_file_with_application (GFile *file)
{
	GAppInfo *application;
	gchar *primary;
	GFileInfo *info;
	gchar *uri_scheme;
	const char *content;
	gboolean local = FALSE;

	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				  G_FILE_QUERY_INFO_NONE,
				  NULL,
				  NULL);
	if (!info) return;
	
	uri_scheme = g_file_get_uri_scheme (file);
	if (g_ascii_strcasecmp(uri_scheme,"file") == 0)  local = TRUE;
	
	content = g_file_info_get_content_type (info);
	application = g_app_info_get_default_for_type (content, TRUE);

	if (!application) {
			primary = g_strdup_printf (_("Could not open folder \"%s\""), 
			                           g_file_get_basename (file));
			message (primary, 
			         _("There is no installed viewer capable "
				   "of displaying the folder."),
				 GTK_MESSAGE_ERROR,
				 baobab.window);
			g_free (primary);
	}
	else {
		GList *uris = NULL;
		gchar *uri;

		uri = g_file_get_uri (file);
		uris = g_list_append (uris, uri);
		g_app_info_launch_uris (application, uris, NULL, NULL);

		g_list_free (uris);
		g_free (uri);
	}

	g_free (uri_scheme);

	if (application)
		g_object_unref (application);

	g_object_unref (info);
}

gboolean
can_trash_file (GFile *file)
{
	GFileInfo *info;
	gboolean can_trash = FALSE;

	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH,
				  G_FILE_QUERY_INFO_NONE,
				  NULL,
				  NULL);

	if (info) {
		if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH)) {
			can_trash = g_file_info_get_attribute_boolean (info,
								       G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH);
		}

		g_object_unref (info);
	}

	return can_trash;
}

gboolean
trash_file (GFile *file)
{
	GError *error = NULL;

	if (!g_file_trash (file, NULL, &error)) {
		GFileInfo *info;
		char *str = NULL;
		char *mess;

		info = g_file_query_info (file,
					  G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
					  G_FILE_QUERY_INFO_NONE,
					  NULL,
					  NULL);

		if (info) {
			const char *displayname = g_file_info_get_display_name (info);
			if (displayname)
				str = g_strdup_printf (_("Could not move \"%s\" to the Trash"),
						      displayname);

			g_object_unref (info);
		}

		/* fallback */
		if (str == NULL)
			str = g_strdup (_("Could not move file to the Trash"));

		mess = g_strdup_printf (_("Details: %s"), error->message);
		message (str, mess, GTK_MESSAGE_ERROR, baobab.window);
		g_free (str);
		g_free (mess);
		g_error_free (error);

		return FALSE;		
	}
	
	return TRUE;
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
		baobab_rescan_current_dir ();
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
	char *uri;
	gboolean ret;

	uri = (link_id) ? 
		g_strdup_printf ("ghelp:%s#%s", file_name, link_id) :
		g_strdup_printf ("ghelp:%s", file_name);

	ret = gtk_show_uri (gtk_window_get_screen (parent),
			    uri, gtk_get_current_event_time (), &error);
	g_free (uri);

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

gboolean
is_virtual_filesystem (GFile *file)
{
	gboolean ret = FALSE;
	char *path;
	
	path = g_file_get_path (file);

	/* FIXME: we need a better way to check virtual FS */
	if (path != NULL) {
		if ((strcmp (path, "/proc") == 0) ||
		    (strcmp (path, "/sys") == 0))
	 		ret = TRUE;
	}

	g_free (path);

	return ret;
}
