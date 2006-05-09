/*
 * callbacks.c
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <gconf/gconf-client.h>

#include "baobab.h"
#include "baobab-graphwin.h"
#include "baobab-treeview.h"
#include "baobab-utils.h"
#include "callbacks.h"
#include "baobab-prefs.h"
#include "baobab-remote-connect-dialog.h"

void
on_menuallfs_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	start_proc_on_dir ("file:///");
}

void
on_menuscandir_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	dir_select (FALSE, baobab.window);
}

void
on_esci1_activate (GtkObject *menuitem, gpointer user_data)
{
	baobab.STOP_SCANNING = TRUE;
	gtk_main_quit ();
}

void
on_about_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	const gchar * const authors[] = {
		"Fabio Marzocca <thesaltydog@gmail.com>",
		"Paolo Borelli <pborelli@katamail.com>",
		"Benoît Dejean <benoit@placenet.org>",
		NULL
	};

	static const gchar copyright[] = "Fabio Marzocca <thesaltydog@gmail.com> \xc2\xa9 2005-2006";

	gtk_show_about_dialog (NULL,
			       "name", _("Baobab"),
			       "comments", _("A graphical tool to analyse "
					     "disk usage."),
			       "version", VERSION,
			       "copyright", copyright,
			       "logo-icon-name", "baobab",
			       "license", "GPL",
			       "authors", authors, "website",
			       "http://www.gnome.org/projects/baobab",
			       "translator-credits",
			       _("translator-credits"), NULL);
}

void
on_tbscandir_clicked (GtkToolButton *toolbutton, gpointer user_data)
{
	dir_select (FALSE, baobab.window);
}

void
on_sort_alfa_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	if (get_NB_page () == VIEW_TREE) {
		gtk_tree_sortable_set_sort_column_id ((GtkTreeSortable *)
						      baobab.model,
						      COL_DIR_NAME,
						      GTK_SORT_ASCENDING);
	}
	if (get_NB_page () == VIEW_SEARCH) {
		gtk_tree_sortable_set_sort_column_id ((GtkTreeSortable *)
						      baobab.model_search,
						      COL_FULLPATH,
						      GTK_SORT_ASCENDING);
	}
}

void
on_by_sort_size_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	if (get_NB_page () == VIEW_TREE) {
		gtk_tree_sortable_set_sort_column_id ((GtkTreeSortable *)
						      baobab.model,
						      COL_H_SIZE,
						      GTK_SORT_DESCENDING);
	}
	if (get_NB_page () == VIEW_SEARCH) {
		gtk_tree_sortable_set_sort_column_id ((GtkTreeSortable *)
						      baobab.model_search,
						      COL_SIZE,
						      GTK_SORT_DESCENDING);
	}
}

void
on_tbstop_clicked (GtkToolButton *toolbutton, gpointer user_data)
{
	baobab.STOP_SCANNING = TRUE;
	stop_scan ();
}

void
on_tbsortalfa_clicked (GtkToolButton *toolbutton, gpointer user_data)
{
	on_sort_alfa_activate (NULL, NULL);
}

void
on_tbsortnum_clicked (GtkToolButton *toolbutton, gpointer user_data)
{
	on_by_sort_size_activate (NULL, NULL);
}

void
on_tbscanall_clicked (GtkToolButton *toolbutton, gpointer user_data)
{
	start_proc_on_dir ("file:///");
}

void
on_tb_scan_remote_clicked (GtkToolButton *toolbutton, gpointer user_data)
{
	gint response;
	GtkWidget *dlg;

	dlg =
	    baobab_remote_connect_dialog_new (GTK_WINDOW (baobab.window),
					      NULL);
	response = gtk_dialog_run (GTK_DIALOG (dlg));

	gtk_widget_destroy (dlg);

	while (gtk_events_pending ())
		gtk_main_iteration ();

	if (response == GTK_RESPONSE_OK)
		start_proc_on_dir (baobab.last_scan_command->str);
}

void
on_menu_scan_rem_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	on_tb_scan_remote_clicked (NULL, NULL);
}

void
on_tb_search_clicked (GtkToolButton *toolbutton, gpointer user_data)
{
	dialog_search ();
}

void
on_radio_allfs_clicked (GtkButton *button, gpointer user_data)
{
	GladeXML *dlg_xml = glade_get_widget_tree ((GtkWidget *) button);
	gtk_widget_set_sensitive (glade_xml_get_widget (dlg_xml, "entry2"),
				  FALSE);
	gtk_widget_set_sensitive (glade_xml_get_widget
				  (dlg_xml, "btn_select_search"), FALSE);
}

void
on_radio_dir_clicked (GtkButton *button, gpointer user_data)
{
	GladeXML *dlg_xml = glade_get_widget_tree ((GtkWidget *) button);
	gtk_widget_set_sensitive (glade_xml_get_widget (dlg_xml, "entry2"),
				  TRUE);
	gtk_widget_set_sensitive (glade_xml_get_widget
				  (dlg_xml, "btn_select_search"), TRUE);
}

void
on_btn_select_search_clicked (GtkButton *button, gpointer user_data)
{
	GtkWidget *dlg = gtk_widget_get_toplevel ((GtkWidget *) button);
	GladeXML *dlg_xml = glade_get_widget_tree ((GtkWidget *) button);
	GtkWidget *entry_dir = glade_xml_get_widget (dlg_xml, "entry2");
	gchar *dirname = dir_select (TRUE, dlg);
	if (dirname) {
		gchar *string_to_display =
		    gnome_vfs_format_uri_for_display (dirname);
		gtk_entry_set_text (GTK_ENTRY (entry_dir),
				    string_to_display);
		g_free (dirname);
		g_free (string_to_display);
	}
}

void
on_search_for_a_file_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	dialog_search ();
}

void
on_notebook1_switch_page (GtkNotebook *notebook,
			  GtkNotebookPage *page,
			  guint page_num, gpointer user_data)
{
	if (page_num == VIEW_SEARCH) {
		set_glade_widget_sens("by_type1",TRUE);
		set_glade_widget_sens("by_date1",TRUE);
		set_glade_widget_sens("ck_allocated",FALSE);
		set_glade_widget_sens("menu_treemap",FALSE);
	}

	if (page_num == VIEW_TREE) {
		set_glade_widget_sens("by_type1",FALSE);
		set_glade_widget_sens("by_date1",FALSE);
		if (baobab.is_local)
			set_glade_widget_sens("ck_allocated",TRUE);
		if (baobab.CONTENTS_CHANGED_DELAYED) {
			baobab.CONTENTS_CHANGED_DELAYED = FALSE;
			if (baobab.STOP_SCANNING) {
				contents_changed ();

			}
		}
	}

	show_label (page_num);
}

gboolean
on_delete_activate (GtkWidget *widget,
		    GdkEvent *event, gpointer user_data)
{
	on_esci1_activate (NULL, NULL);
	return TRUE;
}

void
open_file_cb (GtkMenuItem *pmenu, gpointer dummy)
{
	g_assert (!dummy);
	g_assert (baobab.selected_path);

	GnomeVFSURI *vfs_uri;

	vfs_uri = gnome_vfs_uri_new (baobab.selected_path);

	if (!gnome_vfs_uri_exists (vfs_uri)) {
		message (_("The document does not exist."), baobab.window);
	}
	gnome_vfs_uri_unref (vfs_uri);
	open_file_with_application (baobab.selected_path);
}

void
trash_file_cb (GtkMenuItem *pmenu, gpointer dummy)
{
	g_assert (!dummy);
	g_assert (baobab.selected_path);

	if (trash_file (baobab.selected_path)) {
		GtkTreeIter iter;
		guint64 filesize;
		GtkTreeSelection *selection;

		selection =
		    gtk_tree_view_get_selection ((GtkTreeView *) baobab.
						 tree_search);
		gtk_tree_selection_get_selected (selection, NULL, &iter);
		gtk_tree_model_get ((GtkTreeModel *) baobab.model_search,
				    &iter, COL_SIZE, &filesize, -1);
		gtk_list_store_remove (GTK_LIST_STORE
				       (baobab.model_search), &iter);
		set_label_search (baobab.number_found_files - 1,
				  baobab.size_found_files - filesize);
		show_label (VIEW_SEARCH);
		if (baobab.bbEnableHomeMonitor)
			baobab.CONTENTS_CHANGED_DELAYED = TRUE;
	}
}

void
graph_map_cb (GtkMenuItem *pmenu, gchar *path_to_string)
{
	baobab_graphwin_create (GTK_TREE_MODEL (baobab.model),
				path_to_string,
				BAOBAB_GLADE_FILE,
				COL_H_FULLPATH,
				baobab.is_local ? COL_H_ALLOCSIZE : COL_H_SIZE,
				-1);
	g_free (path_to_string);
}

void
trash_dir_cb (GtkMenuItem *pmenu, gpointer dummy)
{
	g_assert (!dummy);
	g_assert (baobab.selected_path);

	if (trash_file (baobab.selected_path)) {
		GtkTreeIter iter;
		guint64 filesize;
		GtkTreeSelection *selection;

		selection =
		    gtk_tree_view_get_selection ((GtkTreeView *) baobab.
						 tree_view);
		gtk_tree_selection_get_selected (selection, NULL, &iter);
		gtk_tree_model_get ((GtkTreeModel *) baobab.model, &iter,
				    5, &filesize, -1);
		gtk_tree_store_remove (GTK_TREE_STORE (baobab.model),
				       &iter);
		if (baobab.bbEnableHomeMonitor)
			contents_changed ();
	}
}

void
list_all_cb (GtkMenuItem *pmenu, gpointer dummy)
{
	g_assert (!dummy);
	g_assert (baobab.selected_path);

	bbSearchOpt.mod_date = NONE;
	bbSearchOpt.size_limit = NONE;
	bbSearchOpt.dont_recurse_dir = TRUE;
	start_search ("*", baobab.selected_path);
}

void
volume_changed (GnomeVFSVolumeMonitor *volume_monitor,
		GnomeVFSVolume *volume)
{
	/* filesystem has changed (mounted or unmounted device) */
	baobab_get_filesystem (&g_fs);
	set_label_scan (&g_fs);
	if (get_NB_page () == VIEW_TREE)
		show_label (VIEW_TREE);
}

void
contents_changed_cb (GnomeVFSMonitorHandle *handle,
		     const gchar *monitor_uri,
		     const gchar *info_uri,
		     GnomeVFSMonitorEventType event_type,
		     gpointer user_data)
{
	gchar *excluding;

	if (!baobab.bbEnableHomeMonitor)
		return;

	if (baobab.CONTENTS_CHANGED_DELAYED)
		return;

	excluding = g_path_get_basename (info_uri);
	if (strcmp (excluding, ".recently-used") == 0   ||
	    strcmp (excluding, ".gnome2_private") == 0  ||
	    strcmp (excluding, ".xsession-errors") == 0 ||
	    strcmp (excluding, ".bash_history") == 0    ||
	    strcmp (excluding, ".gconfd") == 0) {
		g_free (excluding);
		return;
	}
	g_free (excluding);

	baobab.CONTENTS_CHANGED_DELAYED = TRUE;
}

void
on_pref_menu (GtkMenuItem *menuitem, gpointer user_data)
{
	create_props ();
}

void
on_by_type1_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	if (get_NB_page () == VIEW_SEARCH) {
		gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (baobab.model_search),
						      COL_FILETYPE,
						      GTK_SORT_ASCENDING);
	}
}

void
on_by_date1_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	if (get_NB_page () == VIEW_SEARCH) {
		gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (baobab.model_search),
						      COL_LASTACCESS,
						      GTK_SORT_DESCENDING);
	}
}

void
on_ck_allocated_toggled (GtkToggleButton *togglebutton,
			 gpointer user_data)
{
	GdkCursor *cursor = NULL;

	if (!baobab.is_local)
		return;

	baobab.show_allocated = gtk_toggle_button_get_active (togglebutton);

	/* change the cursor */
	if (baobab.window->window) {
		cursor = gdk_cursor_new (GDK_WATCH);
		gdk_window_set_cursor (baobab.window->window, cursor);
	}

	set_busy (TRUE);
	set_statusbar (_("Calculating percentage bars..."));
	gtk_tree_model_foreach (GTK_TREE_MODEL (baobab.model),
				show_bars, NULL);
	set_busy (FALSE);
	set_statusbar (_("Ready"));

	/* cursor clean up */
	if (baobab.window->window) {
		gdk_window_set_cursor (baobab.window->window, NULL);
		if (cursor)
			gdk_cursor_unref (cursor);
	}
}

void
on_view_tb_activate (GtkCheckMenuItem *checkmenuitem,
                     gpointer          user_data) 
{
	gboolean visible;

	visible = gtk_check_menu_item_get_active (checkmenuitem);
	set_toolbar_visible (visible);

	gconf_client_set_bool (baobab.gconf_client,
			       BAOBAB_TOOLBAR_VISIBLE_KEY,
			       visible,
			       NULL);
}

void
on_view_sb_activate (GtkCheckMenuItem *checkmenuitem,
                     gpointer          user_data) 
{
	gboolean visible;

	visible = gtk_check_menu_item_get_active (checkmenuitem);
	set_statusbar_visible (visible);

	gconf_client_set_bool (baobab.gconf_client,
			       BAOBAB_STATUSBAR_VISIBLE_KEY,
			       visible,
			       NULL);
}

void
on_menu_treemap_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	gchar *path_to_string;
	
	if (!gtk_tree_selection_get_selected (gtk_tree_view_get_selection 
							(GTK_TREE_VIEW(baobab.tree_view)), 
					      NULL, 
					      &iter))		 
		return;

	path = gtk_tree_model_get_path(GTK_TREE_MODEL(baobab.model), &iter);
	
	/* path_to_string is freed in graph_map_cb function */
	path_to_string = gtk_tree_path_to_string (path);

	gtk_tree_path_free(path);
	graph_map_cb(NULL, path_to_string);
}

void
on_helpcontents_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	baobab_help_display (GTK_WINDOW (baobab.window), "baobab.xml", NULL);
}

void 
scan_folder_cb (GtkMenuItem *pmenu, gpointer dummy)
{
	g_assert (!dummy);
	g_assert (baobab.selected_path);

	GnomeVFSURI *vfs_uri;

	vfs_uri = gnome_vfs_uri_new (baobab.selected_path);

	if (!gnome_vfs_uri_exists (vfs_uri)) {
		message (_("The folder does not exist."), baobab.window);
	}
	gnome_vfs_uri_unref (vfs_uri);
	
	start_proc_on_dir (baobab.selected_path);
}
