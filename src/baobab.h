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

struct BaobabSearchOpt;

/* Settings */
#define BAOBAB_SETTINGS_TOOLBAR_VISIBLE "toolbar-visible"
#define BAOBAB_SETTINGS_STATUSBAR_VISIBLE "statusbar-visible"
#define BAOBAB_SETTINGS_ACTIVE_CHART "active-chart"
#define BAOBAB_SETTINGS_MONITOR_HOME "monitor-home"
#define BAOBAB_SETTINGS_EXCLUDED_URIS "excluded-uris"
#define DESKTOP_SETTINGS_TOOLBAR_STYLE "toolbar-style"

typedef struct _BaobabApplication BaobabApplication;

struct _BaobabApplication {
	GtkBuilder *main_ui;
	GtkWidget *window;
	GtkWidget *tree_view;
	GtkWidget *chart_frame;
	GtkWidget *rings_chart;
	GtkWidget *treemap_chart;
	GtkWidget *current_chart;
	GtkWidget *chart_type_combo;
	GtkWidget *toolbar;
	GtkWidget *spinner;
	GtkWidget *statusbar;
	GtkTreeStore *model;
	gboolean STOP_SCANNING;
	gboolean CONTENTS_CHANGED_DELAYED;
	GSList *excluded_locations;
	gboolean show_allocated;
	gboolean is_local;

	guint64 fstotal;
	guint64 fsused;
	guint64 fsavail;

	char *selected_path;

	GFile *current_location;

	GVolumeMonitor *monitor_vol;
	GFileMonitor *monitor_home;

	gint model_max_depth;

	GSettings *ui_settings;
	GSettings *prefs_settings;
	GSettings *desktop_settings;
};

/* Application singleton */
BaobabApplication baobab;

struct chan_data {
	guint64 size;
	guint64 alloc_size;
	guint64 tempHLsize;
	guint depth;
	gint elements;
	gchar *display_name;
	gchar *parse_name;
};

void baobab_set_busy (gboolean busy);
void baobab_update_filesystem (void);
void baobab_scan_location (GFile *);
void baobab_scan_home (void);
void baobab_scan_root (void);
void baobab_rescan_current_dir (void);
void baobab_stop_scan (void);
void baobab_fill_model (struct chan_data *);
gboolean baobab_is_excluded_location (GFile *);
void baobab_set_toolbar_visible (gboolean visible);
void baobab_set_statusbar_visible (gboolean visible);
void baobab_set_statusbar (const gchar *);
void baobab_quit (void);

#endif /* __BAOBAB_H_ */
