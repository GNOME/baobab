/*
 * baobab.h
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
 

#ifndef __BAOBAB_H__
#define __BAOBAB_H__


#include <time.h>
#include <sys/types.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <gconf/gconf-client.h>

typedef struct _baobab_application baobab_application;
typedef struct _baobab_fs baobab_fs;
struct BaobabSearchOpt;

#define BAOBAB_UI_FILE PKGDATADIR "/baobab-main-window.ui"
#define BAOBAB_DIALOG_SCAN_UI_FILE PKGDATADIR "/baobab-dialog-scan-props.ui"

#define BAOBAB_KEY_DIR "/apps/baobab"
#define BAOBAB_TOOLBAR_VISIBLE_KEY	BAOBAB_KEY_DIR "/ui/toolbar_visible"
#define BAOBAB_STATUSBAR_VISIBLE_KEY	BAOBAB_KEY_DIR "/ui/statusbar_visible"
#define BAOBAB_SUBFLSTIPS_VISIBLE_KEY   BAOBAB_KEY_DIR "/ui/baobab_subfoldertips_visible"
#define PROPS_SCAN_KEY			BAOBAB_KEY_DIR "/properties/skip_scan_uri_list"
#define PROPS_ENABLE_HOME_MONITOR	BAOBAB_KEY_DIR "/properties/enable_home_monitor"
#define SYSTEM_TOOLBAR_STYLE		"/desktop/gnome/interface/toolbar_style"

typedef struct _ContextMenu ContextMenu;

struct _ContextMenu {
	GtkWidget* widget;
	GtkWidget* up_item;
	GtkWidget* zoom_in_item;
	GtkWidget* zoom_out_item;
	GtkWidget* subfolders_item;
	GtkWidget* snapshot_item;
	GtkWidget* set_root_item;
};

struct _baobab_application {
	GtkBuilder *main_ui;
  	GtkWidget *window;
  	GtkWidget *tree_view;
	GtkWidget *rings_chart;
	GtkWidget *treemap_chart;
	GtkWidget *current_chart;
	GtkWidget *chart_type_combo;
	ContextMenu *chart_menu;
	GtkWidget *toolbar;
	GtkWidget *spinner;
	GtkWidget *statusbar;
	GtkTreeStore *model;
	gboolean STOP_SCANNING;
	gboolean CONTENTS_CHANGED_DELAYED;
	GSList *excluded_locations;
	gboolean monitor_home;
	gchar *label_scan;
	gboolean show_allocated;
	gboolean is_local;

	char *selected_path;

	GConfClient *gconf_client;
	gint model_max_depth;
};

struct _baobab_fs {
	guint64 total;
	guint64 used;
	guint64 avail;
};

struct chan_data {
	guint64 size;
	guint64 alloc_size;
	guint64 tempHLsize;
	guint depth;
	gint elements;
	gchar *display_name;
	gchar *parse_name;
};

/* globals */
gboolean g_noactivescans;
baobab_application baobab;
baobab_fs g_fs;

void baobab_set_busy (gboolean busy);
void baobab_scan_location (GFile *);
void baobab_scan_home (void);
void baobab_scan_root (void);
void baobab_rescan_current_dir (void);
void baobab_stop_scan (void);
void fill_model (struct chan_data *);
void first_row (void);
gboolean baobab_is_excluded_location (GFile *);
void baobab_set_excluded_locations (GSList *);
void set_toolbar_visible (gboolean visible);
void set_statusbar_visible (gboolean visible);
void set_statusbar (const gchar *);

#endif /* __BAOBAB_H_ */
