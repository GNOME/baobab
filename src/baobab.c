/*
 * baobab.c
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

#include <pwd.h>
#include <time.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <libgnomeui/libgnomeui.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <gconf/gconf-client.h>
#include <glibtop.h>

#include "baobab.h"
#include "baobab-scan.h"
#include "baobab-treeview.h"
#include "baobab-utils.h"
#include "callbacks.h"
#include "baobab-prefs.h"

static GnomeVFSMonitorHandle *handle_mtab;
static GnomeVFSMonitorHandle *handle_home;

static void check_UTF (GString *);

static void push_iter_in_stack (GtkTreeIter *);
static GtkTreeIter pop_iter_from_stack (void);

static gint currentdepth = 0;
static GtkTreeIter currentiter;
static GtkTreeIter firstiter;
static GQueue *iterstack = NULL;

#define BUSY_IMAGE_PATH		BAOBAB_PIX_DIR "busy.gif"
#define DONE_IMAGE_PATH		BAOBAB_PIX_DIR "done.png"

static GdkPixbufAnimation *busy_image = NULL;
static GdkPixbuf *done_image = NULL;

static gboolean filter_adv_search (struct BaobabSearchRet *);
static void initialize_search_variables (void);
static void baobab_toolbar_style (GConfClient *client,
	      			  guint cnxn_id,
	      			  GConfEntry *entry,
	      			  gpointer user_data);
	      			  
static gboolean
scan_is_local (const gchar *uri_dir)
{
	GnomeVFSFileInfo *info;
	GnomeVFSResult res;
	gboolean ret;

	info = gnome_vfs_file_info_new ();
	res = gnome_vfs_get_file_info (uri_dir,
				       info, GNOME_VFS_FILE_INFO_DEFAULT);

	ret = ((res == GNOME_VFS_OK) &&
	       (info->flags & GNOME_VFS_FILE_FLAGS_LOCAL));

	gnome_vfs_file_info_unref (info);

	return ret;
}

void
set_busy (gboolean busy)
{
	GtkWidget *image;
	GError *err = NULL;

	image = glade_xml_get_widget (baobab.main_xml, "busyimage");

	if (busy) {
		if (busy_image == NULL) {
			busy_image = gdk_pixbuf_animation_new_from_file (BUSY_IMAGE_PATH, &err);
			if (err != NULL) {
				g_warning ("Could not load \"busy\" animation (%s)\n",  err->message);
				g_error_free (err);
				return;
			}
		}

		gtk_image_set_from_animation (GTK_IMAGE (image), busy_image);
	}
	else {
		if (done_image == NULL) {
			done_image = gdk_pixbuf_new_from_file (DONE_IMAGE_PATH, &err);
			if (err != NULL) {
				g_warning ("Could not load \"done\" image (%s)\n",  err->message);
				g_error_free (err);
				return;
			}
		}

		gtk_image_set_from_pixbuf (GTK_IMAGE (image), done_image);
	}
}

/*
 * start scanning on a specific directory
 */
void
start_proc_on_dir (const gchar *dir)
{
	GdkCursor *cursor = NULL;
	GtkWidget *ck_allocated;

	switch_view (VIEW_TREE);

	if (!baobab_check_dir (dir))
		return;

	g_string_assign (baobab.last_scan_command, dir);
	baobab.STOP_SCANNING = FALSE;
	set_busy (TRUE);
	check_menu_sens (TRUE);
	gtk_tree_store_clear (baobab.model);
	currentdepth = -1;	/* flag */
	iterstack = g_queue_new ();

	/* check if the file system is local or remote */
	baobab.is_local = scan_is_local (dir);
	ck_allocated = glade_xml_get_widget (baobab.main_xml, "ck_allocated");
	if (!baobab.is_local) {
		gtk_toggle_button_set_active ((GtkToggleButton *)
					      ck_allocated, FALSE);
		gtk_widget_set_sensitive (ck_allocated, FALSE);
		baobab.show_allocated = FALSE;
	}
	else {
		gtk_widget_set_sensitive (ck_allocated, TRUE);
	}

	getDir (dir);

	/* change the cursor */
	if (baobab.window->window) {
		cursor = gdk_cursor_new (GDK_WATCH);
		gdk_window_set_cursor (baobab.window->window, cursor);
	}

	/* set statusbar, percentage and allocated/normal size */
	set_statusbar (_("Calculating percentage bars..."));
	gtk_tree_model_foreach (GTK_TREE_MODEL (baobab.model),
				show_bars,
				NULL);
	set_busy (FALSE);
	check_menu_sens (FALSE);
	set_statusbar (_("Ready"));

	/* cursor clean up */
	if (baobab.window->window) {
		gdk_window_set_cursor (baobab.window->window, NULL);
		if (cursor)
			gdk_cursor_unref (cursor);
	}

	gtk_tree_view_columns_autosize (GTK_TREE_VIEW (baobab.tree_view));
	baobab.STOP_SCANNING = TRUE;
	g_queue_free (iterstack);
	baobab.CONTENTS_CHANGED_DELAYED = FALSE;
}

void
start_search (const gchar *searchname, const gchar *dir)
{
	GdkCursor *cursor = NULL;

	switch_view (VIEW_SEARCH);
	initialize_search_variables ();
	baobab.STOP_SCANNING = FALSE;
	set_busy (TRUE);
	check_menu_sens (TRUE);

	/* change the cursor */
	if (baobab.window->window) {
		cursor = gdk_cursor_new (GDK_WATCH);
		gdk_window_set_cursor (baobab.window->window, cursor);
	}

	gtk_list_store_clear (baobab.model_search);

	if (dir == NULL)
		searchDir ("file:///", searchname);
	else
		searchDir (dir, searchname);

	/* cursor clean up */
	if (baobab.window->window) {
		gdk_window_set_cursor (baobab.window->window, NULL);
		if (cursor)
			gdk_cursor_unref (cursor);
	}

	set_busy (FALSE);
	check_menu_sens (FALSE);
	set_statusbar (_("Ready"));

	set_label_search (baobab.number_found_files,
			  baobab.size_found_files);
	if (get_NB_page () == VIEW_SEARCH)
		show_label (VIEW_SEARCH);
	gtk_tree_view_columns_autosize (GTK_TREE_VIEW (baobab.tree_search));
	baobab.STOP_SCANNING = TRUE;
	bbSearchOpt.dont_recurse_dir = FALSE;
}

static gchar *
get_last_mod (time_t timeacc)
{
	gchar snum[21];
	struct tm *lt;
	size_t len;

	lt = localtime (&timeacc);
	len = strftime (snum, sizeof (snum), "%Y-%m-%d %R:%S", lt);

	return g_strndup (snum, len);
}

static gchar *
get_owner (uid_t own_id)
{
	struct passwd *pw;

	pw = getpwuid (own_id);

	return g_strdup (pw != NULL ? pw->pw_name : _("Unknown"));
}

static void
prepare_firstcol (GString *text, struct BaobabSearchRet *bbret)
{
	gchar *basename, *path, *last_mod, *owner, *size, *alloc_size;

	check_UTF (text);
	basename = g_path_get_basename (text->str);
	path = strdup (text->str);

	last_mod = get_last_mod (bbret->lastacc);
	owner = get_owner (bbret->owner);

	size = gnome_vfs_format_file_size_for_display (bbret->size);
	alloc_size = gnome_vfs_format_file_size_for_display (bbret->alloc_size);

	g_string_printf (text,
			 "<big><b>%s</b></big>\n"
			 "<small>%s %s</small>\n"
		         "<small>%s %s    %s %s</small>\n"
			 "<small>%s (%s %s) - %s</small>",
			 basename,
			 _("Full path:"), path,
			 _("Last Modification:"), last_mod,
			 _("Owner:"), owner,
			 size, _("Allocated bytes:"), alloc_size,
			 gnome_vfs_mime_get_description (bbret->mime_type));

	g_free (basename);
	g_free (path);
	g_free (last_mod);
	g_free (owner);
	g_free (size);
	g_free (alloc_size);
}

/*
 * fill the search model
 */
void
fill_search_model (struct BaobabSearchRet *bbret)
{
	GtkTreeIter iter;
	GString *testo1;
	GdkPixbuf *picon;
	char *gicon;

	if (!filter_adv_search (bbret))
		return;

	gicon =
	    gnome_icon_lookup_sync (gtk_icon_theme_get_default (), NULL,
				    bbret->fullpath, NULL,
				    GNOME_ICON_LOOKUP_FLAGS_NONE, NULL);
	picon =
	    gtk_icon_theme_load_icon (gtk_icon_theme_get_default (), gicon,
				      36, GTK_ICON_LOOKUP_USE_BUILTIN,
				      NULL);

	testo1 = g_string_new (bbret->fullpath);
	prepare_firstcol (testo1, bbret);

	gtk_list_store_append (baobab.model_search, &iter);
	gtk_list_store_set (baobab.model_search, &iter,
			    COL0_ICON, picon, COL1_STRING, testo1->str,
			    /* COL2_STRING,testo2->str, */
			    COL_FULLPATH, bbret->fullpath,
			    COL_FILETYPE, bbret->mime_type,
			    COL_SIZE, bbret->size,
			    COL_LASTACCESS, bbret->lastacc,
			    COL_OWNER, bbret->owner, -1);

	baobab.number_found_files++;
	baobab.size_found_files += bbret->size;
	g_object_unref (picon);
	g_string_free (testo1, TRUE);
}

void
initialize_search_variables (void)
{
	baobab.number_found_files = 0;
	baobab.size_found_files = 0;
}

static gboolean
filter_adv_search (struct BaobabSearchRet *bbret)
{
	gboolean flag = TRUE;
	GDate *filedate, *today;
	gint days;
	gint maxinterval = 100;

	if ((bbSearchOpt.mod_date == NONE) &&
	     bbSearchOpt.size_limit == NONE) {
		return TRUE;
	}

	switch (bbSearchOpt.mod_date) {

	case LAST_WEEK:
	case LAST_MONTH:
		filedate = g_date_new ();
		today = g_date_new ();
		g_date_set_time (filedate, (GTime) bbret->lastacc);
		g_date_set_time (today, time (NULL));
		days = g_date_days_between (filedate, today);
		if (bbSearchOpt.mod_date == LAST_WEEK) {
			maxinterval = 7;
		}
		if (bbSearchOpt.mod_date == LAST_MONTH) {
			maxinterval = 30;
		}
		if (days > maxinterval) {
			flag = FALSE;
		}

		g_date_free (today);
		g_date_free (filedate);
		break;
	}

	switch (bbSearchOpt.size_limit) {

	case SIZE_SMALL:
		if (bbret->size >= (guint64) 102400)
			flag = FALSE;
		break;

	case SIZE_MEDIUM:
		if (bbret->size >= (guint64) 1048576)
			flag = FALSE;

		break;
	}

	return flag;
}

/*
 * pre-fills model during scanning
 */
static void
prefill_model (struct chan_data *data)
{
	GString *cdir;
	char *basename;
	GtkTreeIter iter, iterparent;
	GtkTreePath *path;

	cdir = g_string_new ("");

	if (currentdepth == -1) {
		gtk_tree_store_append (baobab.model, &iter, NULL);
		firstiter = iter;
	}
	else if (data->depth == 1) {
		gtk_tree_store_append (baobab.model, &iter, &firstiter);
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (baobab.model),
						&firstiter);
		gtk_tree_view_expand_row (GTK_TREE_VIEW (baobab.tree_view),
					  path, FALSE);
		gtk_tree_path_free (path);
	}
	else if (data->depth > currentdepth) {
		gtk_tree_store_append (baobab.model, &iter, &currentiter);
	}
	else if (data->depth == currentdepth) {
		gtk_tree_model_iter_parent ((GtkTreeModel *) baobab.model,
					    &iterparent, &currentiter);
		gtk_tree_store_append (baobab.model, &iter, &iterparent);
	}
	else if (data->depth < currentdepth) {
		GtkTreeIter tempiter;
		gint i;
		iter = currentiter;
		for (i = 0; i <= (currentdepth - data->depth); i++) {
			gtk_tree_model_iter_parent ((GtkTreeModel *)
						    baobab.model,
						    &tempiter, &iter);
			iter = tempiter;
		}
		gtk_tree_store_append (baobab.model, &iter, &tempiter);
	}
	currentdepth = data->depth;
	push_iter_in_stack (&iter);
	currentiter = iter;

	basename = g_filename_display_basename (data->dir);
	g_string_assign (cdir, basename);
	g_free (basename);

	/* check UTF-8 and locale */
	check_UTF (cdir);

	g_string_append (cdir, " &lt;== ");
	g_string_append (cdir, _("scanning..."));
	gtk_tree_store_set (baobab.model, &iter, COL_DIR_NAME, cdir->str,
			    COL_H_FULLPATH, "", COL_H_ELEMENTS, -1, -1);

	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}

	g_string_free (cdir, TRUE);
}

/*
 * set filesystem first row
 */
void
first_row (void)
{
	char *size;
	gfloat perc;
	gchar textperc[10];
	GdkPixbuf *bar;

	gtk_tree_store_append (baobab.model, &firstiter, NULL);
	size = gnome_vfs_format_file_size_for_display (g_fs.used);
	g_assert(g_fs.total != 0);
	perc = ((gfloat) g_fs.used * 100) / (gfloat) g_fs.total;
	g_sprintf (textperc, " %.1f %%", perc);
	bar = set_bar ((g_fs.used * 100) / g_fs.total);

	gtk_tree_store_set (baobab.model, &firstiter,
			    COL_DIR_NAME,
			    _("<i>Total filesystem usage:</i>"),
			    COL_H_FULLPATH, "",
			    COL_BAR, bar,
			    COL_H_PERC, perc,
			    COL_DIR_SIZE, size, COL_PERC, textperc,
			    COL_H_SIZE, g_fs.used,
			    COL_H_ALLOCSIZE, g_fs.used,
			    COL_H_ELEMENTS, -1, -1);

	g_object_unref (bar);
	g_free (size);
}

/* fills model during scanning
 *  model:
 *
 *	0, $dir,					   1, $fullpath,
 *	2, set_bar($perc),				   3, $perc,
 *	4, calc($size),	        			   5, $size,
 *	6, sprintf("% 4s",$elements)." "._('elements'),	   7, $elements,
 *	8, text showing hardlink size,			   9, $HLsize
 */
void
fill_model (struct chan_data *data)
{
	GtkTreeIter iter;
	GString *hardlinks;
	GString *basename;
	GString *elementi;
	char *size;
	char *alloc_size;
	char *basename_cstr;

	if (data->elements == -1) {
		prefill_model (data);
		return;
	}

	basename = g_string_new ("");
	basename_cstr = g_filename_display_basename (data->dir);
	g_string_assign (basename, basename_cstr);
	g_free (basename_cstr);

	check_UTF (basename);
	g_string_prepend (basename, "<b>");
	g_string_append (basename, "</b>");

	iter = pop_iter_from_stack ();

	hardlinks = g_string_new ("");
	if (data->tempHLsize > 0) {
		size = gnome_vfs_format_file_size_for_display (data->tempHLsize);
		g_string_assign (hardlinks, "<i>(");
		g_string_append (hardlinks, _("contains hardlinks for:"));
		g_string_append (hardlinks, " ");
		g_string_append (hardlinks, size);
		g_string_append (hardlinks, ")</i>");
		g_free (size);
	}

	elementi = g_string_new ("");
	g_string_printf (elementi,
			 ngettext ("% 5d object", "% 5d objects",
				   data->elements), data->elements);

	size = gnome_vfs_format_file_size_for_display (data->size);
	alloc_size = gnome_vfs_format_file_size_for_display (data->alloc_size);

	gtk_tree_store_set (baobab.model, &iter,
			    COL_DIR_NAME, basename->str, COL_H_FULLPATH,
			    data->dir, COL_H_PERC, 0.0, COL_DIR_SIZE,
			    baobab.show_allocated ? alloc_size : size,
			    COL_H_SIZE, data->size, COL_ELEMENTS,
			    elementi->str, COL_H_ELEMENTS, data->elements,
			    COL_HARDLINK, hardlinks->str, COL_H_HARDLINK,
			    data->tempHLsize, COL_H_ALLOCSIZE,
			    data->alloc_size, -1);

	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}

	g_string_free (hardlinks, TRUE);
	g_string_free (basename, TRUE);
	g_string_free (elementi, TRUE);
	g_free (size);
	g_free (alloc_size);
}

void
push_iter_in_stack (GtkTreeIter *iter)
{
	g_queue_push_head (iterstack, iter->user_data3);
	g_queue_push_head (iterstack, iter->user_data2);
	g_queue_push_head (iterstack, iter->user_data);
	g_queue_push_head (iterstack, GINT_TO_POINTER (iter->stamp));
}

GtkTreeIter
pop_iter_from_stack (void)
{
	GtkTreeIter iter;

	iter.stamp = GPOINTER_TO_INT (g_queue_pop_head (iterstack));
	iter.user_data = g_queue_pop_head (iterstack);
	iter.user_data2 = g_queue_pop_head (iterstack);
	iter.user_data3 = g_queue_pop_head (iterstack);

	return iter;
}

void
check_UTF (GString *name)
{
	char *str;
	char *escaped_str;

	str = g_filename_to_utf8 (name->str, -1, NULL, NULL, NULL);

	if (!str) {
		str = g_locale_to_utf8 (name->str, -1, NULL, NULL, NULL);

		if (!str) {
			str = g_strjoin ("<i>",
					 _("Invalid UTF-8 characters"),
					 "</i>", NULL);
		}
	}

	g_assert (str);

	escaped_str = g_markup_escape_text (str, strlen (str));

	g_string_assign (name, escaped_str);

	g_free (str);
	g_free (escaped_str);
}

gint
list_find (gconstpointer a, gconstpointer b)
{
	gchar *str_a, *str_b;
	gint ret;

	str_a = gnome_vfs_format_uri_for_display (a);
	str_b = gnome_vfs_format_uri_for_display (b);

	ret = strcmp (str_a, str_b);

	g_free (str_a);
	g_free (str_b);

	return ret;
}

gboolean
is_excluded_dir (const gchar *dir)
{
		
	return (baobab.bbExcludedDirs &&
		(g_slist_find_custom (baobab.bbExcludedDirs, dir, list_find) != NULL));
}

void
set_toolbar_visible (gboolean visible)
{
	GtkWidget *toolbar;
	GtkWidget *menu;

	toolbar = glade_xml_get_widget (baobab.main_xml, "toolbar1");

	if (visible)
		gtk_widget_show (toolbar);
	else
		gtk_widget_hide (toolbar);

	/* make sure the check menu item is consistent */
	menu = glade_xml_get_widget (baobab.main_xml, "view_tb");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu), visible);
}

void
set_statusbar_visible (gboolean visible)
{
	GtkWidget *statusbar;
	GtkWidget *menu;

	statusbar = glade_xml_get_widget (baobab.main_xml, "statusbar1");

	if (visible)
		gtk_widget_show (statusbar);
	else
		gtk_widget_hide (statusbar);

	/* make sure the check menu item is consistent */
	menu = glade_xml_get_widget (baobab.main_xml, "view_sb");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu), visible);
}

static void
baobab_toolbar_style (GConfClient *client,
	      	      guint cnxn_id,
	      	      GConfEntry *entry,
	      	      gpointer user_data)
{
	GtkWidget *toolbar;
	gchar *toolbar_setting;

	toolbar = glade_xml_get_widget(baobab.main_xml, "toolbar1");
	toolbar_setting = gconf_client_get_string (baobab.gconf_client,
						   SYSTEM_TOOLBAR_STYLE,
						   NULL);

	if (!strcmp(toolbar_setting, "icons")) {
		gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
	}
	else if (!strcmp(toolbar_setting, "both")) {
		gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH);
	}
	else if (!strcmp(toolbar_setting, "both-horiz")) {
		gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH_HORIZ);
	}
	else if (!strcmp(toolbar_setting, "text")) {
		gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_TEXT);
	}

	g_free (toolbar_setting);
}

static void
baobab_init (void)
{
	GnomeVFSResult result;
	GnomeVFSVolumeMonitor *volmonitor;
	gboolean visible;

	/* Load Glade */
	baobab.main_xml = glade_xml_new (BAOBAB_GLADE_FILE,
					 "baobab_window", NULL);
	glade_xml_signal_autoconnect (baobab.main_xml);

	/* Misc */
	baobab.label_scan = NULL;
	baobab.label_search = NULL;
	baobab.last_scan_command = g_string_new ("/");
	baobab.CONTENTS_CHANGED_DELAYED = FALSE;
	baobab.STOP_SCANNING = TRUE;
	baobab.show_allocated = TRUE;
	baobab.is_local = TRUE;

	/* GConf */
	baobab.gconf_client = gconf_client_get_default ();
	gconf_client_add_dir (baobab.gconf_client, BAOBAB_KEY_DIR,
			      GCONF_CLIENT_PRELOAD_NONE, NULL);
	gconf_client_notify_add (baobab.gconf_client, PROPS_SCAN_KEY, props_notify,
				 NULL, NULL, NULL);
	gconf_client_notify_add (baobab.gconf_client, SYSTEM_TOOLBAR_STYLE, baobab_toolbar_style,
				 NULL, NULL, NULL);				 
	baobab.bbExcludedDirs = gconf_client_get_list (baobab.gconf_client, PROPS_SCAN_KEY,
						       GCONF_VALUE_STRING, NULL);
	
	/*	Verify if gconf wrongly contains root dir exclusion, and remove it from gconf. */
	if (is_excluded_dir("/")) {
		baobab.bbExcludedDirs = g_slist_delete_link (baobab.bbExcludedDirs, 
						g_slist_find_custom(baobab.bbExcludedDirs, 
							"/", list_find));
		gconf_client_set_list (baobab.gconf_client, PROPS_SCAN_KEY,
						GCONF_VALUE_STRING, 
						baobab.bbExcludedDirs, NULL);
	}
	
	baobab.bbEnableHomeMonitor = gconf_client_get_bool (baobab.gconf_client,
							    PROPS_ENABLE_HOME_MONITOR,
							    NULL);

	/* toolbar & statusbar visibility */
	visible = gconf_client_get_bool (baobab.gconf_client,
					 BAOBAB_TOOLBAR_VISIBLE_KEY,
					 NULL);
	set_toolbar_visible (visible);
	visible = gconf_client_get_bool (baobab.gconf_client,
					 BAOBAB_STATUSBAR_VISIBLE_KEY,
					 NULL);
	set_statusbar_visible (visible);

	/* set the toolbar style */
	baobab_toolbar_style(NULL, 0, NULL, NULL);
	
	/* initialize bbSearchOpt variables */
	bbSearchOpt.filename = g_string_new ("");
	bbSearchOpt.dir = g_string_new (g_get_home_dir ());
	bbSearchOpt.mod_date = NONE;
	bbSearchOpt.size_limit = NONE;
	bbSearchOpt.exact = TRUE;
	bbSearchOpt.search_whole = FALSE;
	bbSearchOpt.dont_recurse_dir = FALSE;

	/* start VFS monitoring */
	handle_home = NULL;

	volmonitor = gnome_vfs_get_volume_monitor ();
	g_signal_connect (volmonitor, "volume_mounted",
			  G_CALLBACK (volume_changed), NULL);
	g_signal_connect (volmonitor, "volume_unmounted",
			  G_CALLBACK (volume_changed), NULL);

	result = gnome_vfs_monitor_add (&handle_home,
					g_get_home_dir (),
					GNOME_VFS_MONITOR_DIRECTORY,
					contents_changed_cb, NULL);

	if (result != GNOME_VFS_OK) {
		message (_("Cannot initialize GNOME VFS monitoring\n"
			   "Some real-time auto-detect function will not be available!"),
			 NULL);
		g_print ("homedir:%s\n",
			 gnome_vfs_result_to_string (result));
	}
}

static void
baobab_shutdown (void)
{
	g_free (baobab.label_scan);
	g_free (baobab.label_search);
	g_string_free (baobab.last_scan_command, TRUE);
	g_string_free (bbSearchOpt.filename, TRUE);
	g_string_free (bbSearchOpt.dir, TRUE);

	if (handle_mtab)
		gnome_vfs_monitor_cancel (handle_mtab);
	if (handle_home)
		gnome_vfs_monitor_cancel (handle_home);

	g_free (baobab.selected_path);

	g_slist_foreach (baobab.bbExcludedDirs, (GFunc) g_free, NULL);
	g_slist_free (baobab.bbExcludedDirs);

	if (baobab.gconf_client)
		g_object_unref (baobab.gconf_client);

	if (done_image)
		g_object_unref (done_image);
	if (busy_image)
		g_object_unref (busy_image);
}

int
main (int argc, char *argv[])
{
	GnomeProgram *program;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	program = gnome_program_init ("baobab", VERSION,
				      LIBGNOMEUI_MODULE,
				      argc, argv,
				      GNOME_PARAM_APP_DATADIR,
				      BAOBAB_DATA_DIR, NULL);

	g_set_application_name ("Baobab");

	g_assert (gnome_vfs_init ());
	gnome_authentication_manager_init ();
	glibtop_init ();

	gtk_window_set_default_icon_name ("baobab");

	baobab_init ();

	baobab_get_filesystem (&g_fs);
	if (g_fs.total == 0) {
		g_print("No mount points detected! Aborting...\n");
		goto closing;
		}
	
	set_label_scan (&g_fs);

	baobab.window = glade_xml_get_widget (baobab.main_xml, "baobab_window");
	gtk_window_set_position (GTK_WINDOW (baobab.window),
				 GTK_WIN_POS_CENTER);

	/* set global pixbufs */
	baobab.yellow_bar = baobab_load_pixbuf ("yellow.png");
	baobab.red_bar = baobab_load_pixbuf ("red.png");
	baobab.green_bar = baobab_load_pixbuf ("green.png");

	baobab.tree_view = create_directory_treeview ();
	baobab.tree_search = create_filesearch_treeview ();
	set_label_search (0, 0);

	switch_view (VIEW_SEARCH);
	switch_view (VIEW_TREE);
	show_label (VIEW_TREE);
	set_busy (FALSE);

	/* set allocated space checkbox */
	gtk_toggle_button_set_active ((GtkToggleButton *)
				      glade_xml_get_widget (baobab.
							    main_xml,
							    "ck_allocated"),
				      baobab.show_allocated);

	set_glade_widget_sens("menu_treemap",FALSE);
	gtk_widget_show (baobab.window);

	first_row ();
	set_statusbar (_("Ready"));

	/* commandline */
	if (argc > 1) {
		gchar *uri_shell;
		uri_shell = gnome_vfs_make_uri_from_shell_arg (argv[1]);
		start_proc_on_dir (uri_shell);
		g_free (uri_shell);
	}

	gtk_main ();

 closing:
	baobab_shutdown ();

	glibtop_close ();
	gnome_vfs_shutdown ();

	g_object_unref (program);

	return 0;
}
