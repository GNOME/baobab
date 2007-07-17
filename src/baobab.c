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

#include "gedit-spinner.h"

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
	if (busy == TRUE) {
		gedit_spinner_start (GEDIT_SPINNER (baobab.spinner));
 		baobab_ringschart_freeze_updates (baobab.ringschart);
		baobab_ringschart_set_init_depth (baobab.ringschart, 0);
	}
	else {
		gedit_spinner_stop (GEDIT_SPINNER (baobab.spinner));
 		baobab_ringschart_thaw_updates (baobab.ringschart);
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

	if (!baobab_check_dir (dir))
		return;
		
	if (iterstack !=NULL)
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
	iterstack = NULL;
	baobab.CONTENTS_CHANGED_DELAYED = FALSE;
}

void
rescan_current_dir (void)
{
	g_return_if_fail (baobab.last_scan_command != NULL);

	start_proc_on_dir (baobab.last_scan_command->str);
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
	gchar *str;

	cdir = g_string_new ("");

	if (currentdepth == -1) {
		gtk_tree_store_append (baobab.model, &iter, NULL);
		firstiter = iter;
	}
	else if (data->depth == 1) {
		GtkTreePath *path;

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

	str = g_strdup_printf ("<small><i>%s</i></small>", _("Scanning..."));

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (baobab.tree_view), TRUE);
	gtk_tree_store_set (baobab.model, &iter,
			    COL_DIR_NAME, cdir->str,
			    COL_H_FULLPATH, "",
			    COL_H_ELEMENTS, -1, 
 			    COL_H_PERC, -1.0,
			    COL_DIR_SIZE, str,
			    COL_ELEMENTS, str, -1);

	g_string_free (cdir, TRUE);
	g_free(str);

	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}
}

/*
 * set filesystem first row
 */
void
first_row (void)
{
	char *size;
	gdouble perc;
	char *label;

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (baobab.tree_view), FALSE);
	gtk_tree_store_append (baobab.model, &firstiter, NULL);
	size = gnome_vfs_format_file_size_for_display (g_fs.used);
	g_assert(g_fs.total != 0);
	perc = ((gdouble) g_fs.used * 100) / (gdouble) g_fs.total;

	label = g_strdup_printf ("<i>%s</i>", _("Total filesystem usage:"));
	gtk_tree_store_set (baobab.model, &firstiter,
			    COL_DIR_NAME, label,
			    COL_H_FULLPATH, "",
			    COL_H_PERC, perc,
			    COL_DIR_SIZE, size,
			    COL_H_SIZE, g_fs.used,
			    COL_H_ALLOCSIZE, g_fs.used,
			    COL_H_ELEMENTS, -1, -1);

	g_free (size);
	g_free (label);
}

/* fills model during scanning */
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
			 ngettext ("% 5d item", "% 5d items",
				   data->elements), data->elements);

	size = gnome_vfs_format_file_size_for_display (data->size);
	alloc_size = gnome_vfs_format_file_size_for_display (data->alloc_size);

	gtk_tree_store_set (baobab.model, &iter,
			    COL_DIR_NAME, basename->str,
			    COL_H_FULLPATH, data->dir,
			    COL_H_PERC, -1.0, 
			    COL_DIR_SIZE,
			    baobab.show_allocated ? alloc_size : size,
			    COL_H_SIZE, data->size,
			    COL_ELEMENTS, elementi->str,
			    COL_H_ELEMENTS, data->elements,
			    COL_HARDLINK, hardlinks->str,
			    COL_H_HARDLINK, data->tempHLsize,
			    COL_H_ALLOCSIZE, data->alloc_size, -1);

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
	g_return_if_fail (dir != NULL);

	return (baobab.bbExcludedDirs &&
		(g_slist_find_custom (baobab.bbExcludedDirs, dir, list_find) != NULL));
}

void
set_toolbar_visible (gboolean visible)
{
	GtkWidget *menu;

	if (visible)
		gtk_widget_show (baobab.toolbar);
	else
		gtk_widget_hide (baobab.toolbar);

	/* make sure the check menu item is consistent */
	menu = glade_xml_get_widget (baobab.main_xml, "view_tb");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu), visible);
}

void
set_statusbar_visible (gboolean visible)
{
	GtkWidget *menu;

	if (visible)
		gtk_widget_show (baobab.statusbar);
	else
		gtk_widget_hide (baobab.statusbar);

	/* make sure the check menu item is consistent */
	menu = glade_xml_get_widget (baobab.main_xml, "view_sb");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu), visible);
}

void
set_statusbar (const gchar *text)
{
	gtk_statusbar_pop (GTK_STATUSBAR (baobab.statusbar), 1);
	gtk_statusbar_push (GTK_STATUSBAR (baobab.statusbar), 1, text);

	while (gtk_events_pending ())
		gtk_main_iteration ();
}

static void
toolbar_reconfigured_cb (GtkToolItem  *item,
			 GeditSpinner *spinner)
{
	GtkToolbarStyle style;
	GtkIconSize size;

	style = gtk_tool_item_get_toolbar_style (item);

	if (style == GTK_TOOLBAR_BOTH)
	{
		size = GTK_ICON_SIZE_DIALOG;
	}
	else
	{
		size = GTK_ICON_SIZE_LARGE_TOOLBAR;
	}

	gedit_spinner_set_size (spinner, size);
}

static void
baobab_toolbar_style (GConfClient *client,
	      	      guint cnxn_id,
	      	      GConfEntry *entry,
	      	      gpointer user_data)
{
	gchar *toolbar_setting;

	toolbar_setting = baobab_gconf_get_string_with_default (baobab.gconf_client,
								SYSTEM_TOOLBAR_STYLE,
								"both");

	if (!strcmp(toolbar_setting, "icons")) {
		gtk_toolbar_set_style (GTK_TOOLBAR(baobab.toolbar),
				       GTK_TOOLBAR_ICONS);
	}
	else if (!strcmp(toolbar_setting, "both")) {
		gtk_toolbar_set_style (GTK_TOOLBAR(baobab.toolbar),
				       GTK_TOOLBAR_BOTH);
	}
	else if (!strcmp(toolbar_setting, "both-horiz")) {
		gtk_toolbar_set_style (GTK_TOOLBAR(baobab.toolbar),
				       GTK_TOOLBAR_BOTH_HORIZ);
	}
	else if (!strcmp(toolbar_setting, "text")) {
		gtk_toolbar_set_style (GTK_TOOLBAR(baobab.toolbar),
				       GTK_TOOLBAR_TEXT);
	}

	g_free (toolbar_setting);
}

static void
baobab_create_toolbar (void)
{
	GtkWidget *toolbar;
	GtkToolItem *item;
	GtkToolItem *separator;
	gboolean visible;

	toolbar = glade_xml_get_widget (baobab.main_xml, "toolbar1");
	if (toolbar == NULL) {
		g_printerr ("Could not build toolbar\n");
		return;
	}

	baobab.toolbar = toolbar;

	separator = gtk_separator_tool_item_new ();
	gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (separator), FALSE);
	gtk_tool_item_set_expand (GTK_TOOL_ITEM (separator), TRUE);	
	gtk_container_add (GTK_CONTAINER (toolbar), GTK_WIDGET (separator));
	gtk_widget_show (GTK_WIDGET (separator));

	baobab.spinner = gedit_spinner_new ();
	item = gtk_tool_item_new ();		
	gtk_container_add (GTK_CONTAINER (item), baobab.spinner);
  	gtk_container_add (GTK_CONTAINER (toolbar), GTK_WIDGET (item));
	gtk_widget_show (GTK_WIDGET (baobab.spinner));
	gtk_widget_show (GTK_WIDGET (item));

	g_signal_connect (item, "toolbar-reconfigured",
			  G_CALLBACK (toolbar_reconfigured_cb), baobab.spinner);

	baobab_toolbar_style (NULL, 0, NULL, NULL);

	visible = gconf_client_get_bool (baobab.gconf_client,
					 BAOBAB_TOOLBAR_VISIBLE_KEY,
					 NULL);

	set_toolbar_visible (visible);
}

static void
baobab_create_statusbar (void)
{
	gboolean visible;

	baobab.statusbar = glade_xml_get_widget (baobab.main_xml,
						 "statusbar1");
	if (baobab.statusbar == NULL) {
		g_printerr ("Could not build statusbar\n");
		return;
	}

	visible = gconf_client_get_bool (baobab.gconf_client,
					 BAOBAB_STATUSBAR_VISIBLE_KEY,
					 NULL);
	set_statusbar_visible (visible);
}

static void
baobab_init (void)
{
	GnomeVFSResult result;
	GnomeVFSVolumeMonitor *volmonitor;

	/* Load Glade */
	baobab.main_xml = glade_xml_new (BAOBAB_GLADE_FILE,
					 "baobab_window", NULL);
	glade_xml_signal_autoconnect (baobab.main_xml);

	/* Misc */
	baobab.label_scan = NULL;
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
	
	/* Verify if gconf wrongly contains root dir exclusion, and remove it from gconf. */
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

	baobab_create_toolbar ();

	baobab_create_statusbar ();

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
		message (_("Could not initialize GNOME VFS monitoring"),
			 _("Changes to your home folder will not be monitored."),
			 GTK_MESSAGE_WARNING, NULL);
		g_print ("homedir:%s\n",
			 gnome_vfs_result_to_string (result));
	}
}

static void
baobab_shutdown (void)
{
	g_free (baobab.label_scan);
	g_string_free (baobab.last_scan_command, TRUE);

	if (handle_mtab)
		gnome_vfs_monitor_cancel (handle_mtab);
	if (handle_home)
		gnome_vfs_monitor_cancel (handle_home);

	g_free (baobab.selected_path);

	g_slist_foreach (baobab.bbExcludedDirs, (GFunc) g_free, NULL);
	g_slist_free (baobab.bbExcludedDirs);

	if (baobab.gconf_client)
		g_object_unref (baobab.gconf_client);
}

static void
initialize_ringschart (void)
{
        GtkWidget *hpaned_main;
        GtkWidget *ringschart_frame;

        baobab.ringschart = GTK_WIDGET (baobab_ringschart_new ());
        baobab_ringschart_set_model_with_columns (baobab.ringschart,
                                                  GTK_TREE_MODEL (baobab.model),
                                                  COL_DIR_NAME,
                                                  COL_DIR_SIZE,
                                                  COL_H_FULLPATH,
                                                  COL_H_PERC,
                                                  COL_H_ELEMENTS,
                                                  NULL);
        baobab_ringschart_set_init_depth (baobab.ringschart, 1);

        g_signal_connect (baobab.ringschart, "sector_activated",
                          G_CALLBACK (on_rchart_sector_activated), NULL);

        ringschart_frame = gtk_frame_new (NULL);
        gtk_frame_set_label_align (GTK_FRAME (ringschart_frame), 0.0, 0.0);
        gtk_frame_set_shadow_type (GTK_FRAME (ringschart_frame), GTK_SHADOW_IN);

        gtk_container_add (GTK_CONTAINER (ringschart_frame),
                           baobab.ringschart);

        hpaned_main = glade_xml_get_widget (baobab.main_xml, "hpaned_main");
        gtk_paned_pack2 (GTK_PANED (hpaned_main),
                         ringschart_frame, TRUE, TRUE);
        gtk_paned_set_position (GTK_PANED (hpaned_main), 480);

        baobab_ringschart_draw_center (baobab.ringschart, TRUE);

        gtk_widget_show_all (ringschart_frame);
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
		GtkWidget *dialog = gtk_message_dialog_new (NULL,
                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                  GTK_MESSAGE_ERROR,
                                  GTK_BUTTONS_CLOSE,
                                  "Could not detect any mount point.");
                gtk_message_dialog_format_secondary_text
                                                        (GTK_MESSAGE_DIALOG(dialog),
                                                         "Without mount points disk usage cannot be analyzed.");
 		gtk_dialog_run (GTK_DIALOG (dialog));
 		gtk_widget_destroy (dialog);
		goto closing;
	}

	set_label_scan (&g_fs);
	show_label ();

	baobab.window = glade_xml_get_widget (baobab.main_xml, "baobab_window");
	gtk_window_set_position (GTK_WINDOW (baobab.window),
				 GTK_WIN_POS_CENTER);

	baobab.tree_view = create_directory_treeview ();

	set_glade_widget_sens("menurescan",FALSE);

	/* set allocated space checkbox */
	gtk_check_menu_item_set_active ((GtkCheckMenuItem *)
				      glade_xml_get_widget (baobab.
							    main_xml,
							    "ck_allocated"),
				      baobab.show_allocated);

	set_glade_widget_sens("menu_treemap",FALSE);
	gtk_widget_show (baobab.window);

	first_row ();
	set_statusbar (_("Ready"));

	/* The ringschart */
	initialize_ringschart ();

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
