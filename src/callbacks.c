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
#include <gconf/gconf-client.h>
#include <gio/gio.h>

#include "baobab.h"
#include "baobab-treeview.h"
#include "baobab-utils.h"
#include "callbacks.h"
#include "baobab-prefs.h"
#include "baobab-remote-connect-dialog.h"
#include "baobab-chart.h"

void
on_menuscanhome_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	baobab_scan_home ();
}

void
on_menuallfs_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	baobab_scan_root ();
}

void
on_menuscandir_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	dir_select (FALSE, baobab.window);
}

void
on_esci1_activate (GtkObject *menuitem, gpointer user_data)
{
	baobab_stop_scan ();
	gtk_main_quit ();
}

void
on_about_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	const gchar * const authors[] = {
		"Fabio Marzocca <thesaltydog@gmail.com>",
		"Paolo Borelli <pborelli@katamail.com>",
		"Benoît Dejean <benoit@placenet.org>",
		"Igalia (rings-chart widget) <www.igalia.com>",
		NULL
	};
	
	const gchar *license =
    	"This program is free software; you can redistribute it and/or "
    	"modify it under the terms of the GNU General Public License as "
    	"published by the Free Software Foundation; either version 2 of "
    	"the License, or (at your option) any later version.\n"
    	"\n"
    	"This program is distributed in the hope that it will be useful, "
    	"but WITHOUT ANY WARRANTY; without even the implied warranty of "
    	"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU "
    	"General Public License for more details.\n"
    	"\n"
    	"You should have received a copy of the GNU General Public License "
    	"along with this program; if not, write to the Free Software "
    	"Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA "
    	"02111-1307, USA.\n";


	static const gchar copyright[] = "Fabio Marzocca <thesaltydog@gmail.com> \xc2\xa9 2005-2008";

	gtk_show_about_dialog (NULL,
			       "name", _("Baobab"),
			       "comments", _("A graphical tool to analyze "
					     "disk usage."),
			       "version", VERSION,
			       "copyright", copyright,
			       "logo-icon-name", "baobab",
			       "license", license,
			       "authors", authors, 
			       "translator-credits",
			       _("translator-credits"), 
			       "wrap-license", TRUE,
			        NULL);
}

void
on_menu_expand_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	gtk_tree_view_expand_all (GTK_TREE_VIEW (baobab.tree_view));
}

void
on_menu_collapse_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	gtk_tree_view_collapse_all (GTK_TREE_VIEW (baobab.tree_view));
}

void
on_menu_stop_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	baobab_stop_scan ();
}

void
on_menu_rescan_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	baobab_rescan_current_dir ();
}

void
on_tbscandir_clicked (GtkToolButton *toolbutton, gpointer user_data)
{
	dir_select (FALSE, baobab.window);
}

void
on_tbscanhome_clicked (GtkToolButton *toolbutton, gpointer user_data)
{
	baobab_scan_home ();
}

void
on_tbscanall_clicked (GtkToolButton *toolbutton, gpointer user_data)
{
	baobab_scan_root ();
}

void
on_tb_scan_remote_clicked (GtkToolButton *toolbutton, gpointer user_data)
{
	GtkWidget *dlg;
	
	dlg = baobab_remote_connect_dialog_new (GTK_WINDOW (baobab.window),
						NULL);
	gtk_dialog_run (GTK_DIALOG (dlg));

	gtk_widget_destroy (dlg);
}

void
on_menu_scan_rem_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	on_tb_scan_remote_clicked (NULL, NULL);
}

void
on_tbstop_clicked (GtkToolButton *toolbutton, gpointer user_data)
{
	baobab_stop_scan ();
}

void
on_tbrescan_clicked (GtkToolButton *toolbutton, gpointer user_data)
{
	baobab_rescan_current_dir ();
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
	GFile *file;

	g_assert (!dummy);
	g_assert (baobab.selected_path);

	file = g_file_parse_name (baobab.selected_path);

	if (!g_file_query_exists (file, NULL)) {
		message (_("The document does not exist."), "",
		GTK_MESSAGE_INFO, baobab.window);
		g_object_unref (file);
		return;
	}

	open_file_with_application (file);
	g_object_unref (file);
}

void
trash_dir_cb (GtkMenuItem *pmenu, gpointer dummy)
{
	GFile *file;

	g_assert (!dummy);
	g_assert (baobab.selected_path);

	file = g_file_parse_name (baobab.selected_path);

	if (trash_file (file)) {
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

	g_object_unref (file);
}

void
volume_changed (GVolumeMonitor *volume_monitor,
                GVolume        *volume,
                gpointer        user_data)
{
	/* filesystem has changed (mounted or unmounted device) */
	baobab_get_filesystem (&g_fs);
	set_label_scan (&g_fs);
	show_label ();
}

void
contents_changed_cb (GFileMonitor      *file_monitor,
              	     GFile             *child,
              	     GFile             *other_file,
              	     GFileMonitorEvent  event_type,
              	     gpointer           user_data)

{
	gchar *excluding;

	if (!baobab.bbEnableHomeMonitor)
		return;

	if (baobab.CONTENTS_CHANGED_DELAYED)
		return;

	excluding = g_file_get_basename (child);
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
on_ck_allocated_activate (GtkCheckMenuItem *checkmenuitem,
			  gpointer user_data)
{
	if (!baobab.is_local)
		return;

	baobab.show_allocated = gtk_check_menu_item_get_active (checkmenuitem);

	baobab_treeview_show_allocated_size (baobab.tree_view,
					     baobab.show_allocated);

	baobab_set_busy (TRUE);
	set_statusbar (_("Calculating percentage bars..."));
	gtk_tree_model_foreach (GTK_TREE_MODEL (baobab.model),
				show_bars, NULL);
	baobab_set_busy (FALSE);
	set_statusbar (_("Ready"));
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
on_helpcontents_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	baobab_help_display (GTK_WINDOW (baobab.window), "baobab.xml", NULL);
}

void 
scan_folder_cb (GtkMenuItem *pmenu, gpointer dummy)
{
	GFile	*file;
	
	g_assert (!dummy);
	g_assert (baobab.selected_path);

	file = g_file_parse_name (baobab.selected_path);

	if (!g_file_query_exists (file, NULL)) {
		message (_("The folder does not exist."), "", GTK_MESSAGE_INFO, baobab.window);
	}

	baobab_scan_location (file);
	g_object_unref (file);
}

void
on_tv_selection_changed (GtkTreeSelection *selection, gpointer user_data)
{
	GtkTreeIter iter;
        
	if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		GtkTreePath *path;
                
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (baobab.model), &iter);
                
		baobab_chart_set_root (baobab.rings_chart, path);
		baobab_chart_set_root (baobab.treemap_chart, path);
		
		gtk_tree_path_free (path);
	}
}

void
on_chart_item_activated (BaobabChart *chart, GtkTreeIter *iter)
{
	GtkTreePath *path;

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (baobab.model), iter);

	if (!gtk_tree_view_row_expanded (GTK_TREE_VIEW (baobab.tree_view), path))
                gtk_tree_view_expand_to_path (GTK_TREE_VIEW (baobab.tree_view), path);

	gtk_tree_view_set_cursor (GTK_TREE_VIEW (baobab.tree_view),
				  path, NULL, FALSE);
	gtk_tree_path_free (path);
}

gboolean
on_chart_button_release (BaobabChart *chart, GdkEventButton *event,
                          gpointer data)
{
  ContextMenu *menu;
  
  if (baobab_chart_is_frozen (baobab.current_chart))
    return FALSE;

  menu = baobab.chart_menu;
  
  if (event->button== 3) /* right button */
    {
      GtkTreePath *root_path = NULL;

      root_path = baobab_chart_get_root (baobab.current_chart);

      gtk_widget_set_sensitive (menu->up_item,
                                ((root_path != NULL) &&
                                 (gtk_tree_path_get_depth (root_path) > 1)));
      gtk_widget_set_sensitive (menu->zoom_in_item,
                                (baobab_chart_get_max_depth (baobab.current_chart) > 1));
      gtk_widget_set_sensitive (menu->zoom_out_item,
                               (baobab_chart_get_max_depth (baobab.current_chart) 
                                < BAOBAB_CHART_MAX_DEPTH));

      /* show the menu */
      gtk_menu_popup (GTK_MENU (menu->widget), NULL, NULL, NULL, NULL,
                      event->button, event->time);
      
      gtk_tree_path_free (root_path);
    }
 
  return FALSE;
}

void
on_move_upwards_cb (GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
  baobab_chart_move_up_root (baobab.current_chart);
}

void
on_zoom_in_cb (GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
  baobab_chart_zoom_in (baobab.current_chart);
}

void
on_zoom_out_cb (GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
  baobab_chart_zoom_out (baobab.current_chart);
}

void
on_chart_snapshot_cb (GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
  baobab_chart_save_snapshot (baobab.current_chart);
}

void
on_chart_type_change (GtkWidget *combo, gpointer user_data)
{
  GtkWidget *chart;
  GtkWidget *frame;

  guint active;

  active = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));

  if (active == 0)
    chart = baobab.rings_chart;
  else
    if (active == 1)
      chart = baobab.treemap_chart;

  frame = gtk_widget_get_parent (baobab.current_chart);

  baobab_chart_freeze_updates (baobab.current_chart);
  baobab_chart_thaw_updates (chart);

  g_object_ref_sink (baobab.current_chart);
  gtk_container_remove (GTK_CONTAINER (frame), baobab.current_chart);
  gtk_container_add (GTK_CONTAINER (frame), chart);

  baobab.current_chart = chart;
}

