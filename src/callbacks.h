/*
 * callbacks.h
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

#ifndef __BAOBAB_CALLBACKS_H__
#define __BAOBAB_CALLBACKS_H__

#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>


void on_about_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_menuallfs_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_menuscandir_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_tbscandir_clicked (GtkToolButton *toolbutton, gpointer user_data);
void on_sort_alfa_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_by_sort_size_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_tbstop_clicked (GtkToolButton *toolbutton, gpointer user_data);
void on_tbsortalfa_clicked (GtkToolButton *toolbutton, gpointer user_data);
void on_tbsortnum_clicked (GtkToolButton *toolbutton, gpointer user_data);
void on_tbscanall_clicked (GtkToolButton *toolbutton, gpointer user_data);
void on_tb_search_clicked (GtkToolButton *toolbutton, gpointer user_data);
void on_btn_oksearch_clicked (GtkButton *button, gpointer user_data);
void on_radio_allfs_clicked (GtkButton *button, gpointer user_data);
void on_radio_dir_clicked (GtkButton *button, gpointer user_data);
void on_btn_select_search_clicked (GtkButton *button, gpointer user_data);
void on_search_for_a_file_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_notebook1_switch_page (GtkNotebook *notebook,
			       GtkNotebookPage *page,
			       guint page_num,
			       gpointer user_data);
void on_esci1_activate (GtkObject *object, gpointer user_data);
gboolean on_delete_activate (GtkWidget *widget, GdkEvent *event, gpointer user_data);
void open_file_cb (GtkMenuItem *pmenu, gpointer dummy);
void trash_file_cb (GtkMenuItem *pmenu, gpointer dummy);
void scan_folder_cb (GtkMenuItem *pmenu, gpointer dummy);
void graph_map_cb (GtkMenuItem *pmenu, gchar * path_to_string);
void trash_dir_cb (GtkMenuItem *pmenu, gpointer dummy);
void list_all_cb (GtkMenuItem *pmenu, gpointer dummy);
void contents_changed_cb (GnomeVFSMonitorHandle *handle,
			  const gchar *monitor_uri,
			  const gchar *info_uri,
			  GnomeVFSMonitorEventType event_type,
			  gpointer user_data);
void on_pref_menu (GtkMenuItem *menuitem, gpointer user_data);
void on_by_type1_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_by_date1_activate (GtkMenuItem *menuitem, gpointer user_data);
void volume_changed (GnomeVFSVolumeMonitor *volume_monitor, GnomeVFSVolume *volume);
void on_ck_allocated_toggled (GtkToggleButton *togglebutton, gpointer user_data);
void on_graph_close_btn_clicked (GtkButton * button, gpointer user_data);
void on_graph_zoom_in_clicked (GtkToolButton *toolbutton, gpointer user_data);
void on_graph_zoom_out_clicked (GtkToolButton *toolbutton, gpointer user_data);
void on_graph_zoom_100_clicked (GtkToolButton *toolbutton, gpointer user_data);
void on_tb_scan_remote_clicked (GtkToolButton *toolbutton, gpointer user_data);
void on_menu_scan_rem_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_view_tb_activate (GtkCheckMenuItem *checkmenuitem, gpointer user_data); 
void on_view_sb_activate (GtkCheckMenuItem *checkmenuitem, gpointer user_data); 
void on_menu_treemap_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_helpcontents_activate (GtkMenuItem *menuitem, gpointer user_data);

#endif /* __BAOBAB_CALLBACKS_H__ */
