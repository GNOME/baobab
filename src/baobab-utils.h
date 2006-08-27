/*
 * baobab-utils.h
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

#ifndef __BAOBAB_UTILS_H__
#define __BAOBAB_UTILS_H__

#include "baobab.h"

void baobab_get_filesystem (baobab_fs *fs);
void set_statusbar (const gchar *);
gchar* dir_select (gboolean, GtkWidget *);
void on_toggled (GtkToggleButton *, gpointer);
GdkPixbuf *set_bar (gfloat );
void check_menu_sens (gboolean);
void stop_scan (void);
gboolean show_bars (GtkTreeModel *model,
		    GtkTreePath *path,
		    GtkTreeIter *iter,
		    gpointer data);
void message (gchar *, GtkWidget *);
gint messageyesno (gchar *messaggio, GtkWidget *parent);
gboolean baobab_check_dir (const gchar *);
void popupmenu_list (GtkTreePath *path, GdkEventButton *event, gboolean is_trash);
void popupmenu_list_search (GtkTreePath *path, GdkEventButton *event, gboolean is_trash);
void open_nautilus (GtkMenuItem *, gpointer );
void dialog_search (void);
void switch_view (gint);
gint get_NB_page (void);
void set_label_scan (baobab_fs *);
void show_label (gint view);
void set_label_search (gint , guint64 );
void open_file_with_application (gchar *file);
gchar * get_trash_path (const gchar *file);
gboolean trash_file (const gchar *filename);
void contents_changed (void);
void set_glade_widget_sens (const gchar *name, gboolean sens);
gchar *baobab_gconf_get_string_with_default (GConfClient *client, const gchar *key, const gchar *def);
gboolean baobab_help_display (GtkWindow *parent, const gchar *file_name, const gchar *link_id);

#endif /* __BAOBAB_UTILS_H__ */
