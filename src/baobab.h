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
#include <glade/glade-xml.h>
#include <gconf/gconf-client.h>

typedef struct _baobab_application baobab_application;
typedef struct _baobab_fs baobab_fs;
struct BaobabSearchOpt;

#define BAOBAB_GLADE_FILE BAOBAB_PIX_DIR "baobab.glade"

#define VIEW_TREE 0
#define VIEW_SEARCH 1

#define BAOBAB_KEY_DIR "/apps/baobab"
#define BAOBAB_TOOLBAR_VISIBLE_KEY	BAOBAB_KEY_DIR "/ui/toolbar_visible"
#define BAOBAB_STATUSBAR_VISIBLE_KEY	BAOBAB_KEY_DIR "/ui/statusbar_visible"
#define PROPS_SCAN_KEY			BAOBAB_KEY_DIR "/properties/noscan"
#define PROPS_ENABLE_HOME_MONITOR	BAOBAB_KEY_DIR "/properties/enable_home_monitor"
#define SYSTEM_TOOLBAR_STYLE		"/desktop/gnome/interface/toolbar_style"


struct _baobab_application {
	GladeXML *main_xml;
  	GtkWidget *window;
  	GtkWidget *tree_view;
	GtkTreeStore *model;
	gboolean STOP_SCANNING;
	GdkPixbuf *green_bar,*yellow_bar,*red_bar;
	gboolean CONTENTS_CHANGED_DELAYED;
	GSList *bbExcludedDirs;
	gboolean bbEnableHomeMonitor;
	gchar *label_scan;
	GString *last_scan_command;
	gboolean show_allocated;
	gboolean is_local;

	char *selected_path;

	GConfClient *gconf_client;
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
	gchar *dir;
};
	

/* globals */
baobab_application baobab;
baobab_fs g_fs;

void set_busy (gboolean busy);
void start_proc_on_dir (const gchar *);
void fill_model (struct chan_data *);
void first_row (void);
gint list_find (gconstpointer a, gconstpointer b);
gboolean is_excluded_dir (const gchar *);
void set_toolbar_visible (gboolean visible);
void set_statusbar_visible (gboolean visible);

#endif /* __BAOBAB_H_ */
