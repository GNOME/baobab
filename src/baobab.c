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

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gconf/gconf-client.h>
#include <glibtop.h>

#include "baobab.h"
#include "baobab-scan.h"
#include "baobab-treeview.h"
#include "baobab-utils.h"
#include "callbacks.h"
#include "baobab-prefs.h"

#include "gedit-spinner.h"

#include "baobab-treemap.h"
#include "baobab-ringschart.h"

static GVolumeMonitor 	 *monitor_vol;
static GFileMonitor	 *monitor_home;

static void push_iter_in_stack (GtkTreeIter *);
static GtkTreeIter pop_iter_from_stack (void);

static gint currentdepth = 0;
static GtkTreeIter currentiter;
static GtkTreeIter firstiter;
static GQueue *iterstack = NULL;

static GFile *current_location = NULL;

#define BUSY_IMAGE_PATH		BAOBAB_PIX_DIR "busy.gif"
#define DONE_IMAGE_PATH		BAOBAB_PIX_DIR "done.png"


static gboolean
scan_is_local (GFile	*file)
{
	gchar *uri_scheme;
	gboolean ret = FALSE;

	uri_scheme = g_file_get_uri_scheme (file);
	if (g_ascii_strcasecmp(uri_scheme,"file") == 0)
		ret = TRUE;

	g_free (uri_scheme);

	return ret;
}

void
baobab_set_busy (gboolean busy)
{
	static GdkCursor *busy_cursor = NULL;
	GdkCursor *cursor = NULL;

	if (busy == TRUE) {
		if (!busy_cursor) {
			busy_cursor = gdk_cursor_new (GDK_WATCH);
		}
		cursor = busy_cursor;

		gedit_spinner_start (GEDIT_SPINNER (baobab.spinner));

		baobab_chart_freeze_updates (baobab.rings_chart);
		baobab_chart_set_summary_mode (baobab.rings_chart, FALSE);

		baobab_chart_freeze_updates (baobab.treemap_chart);
		baobab_chart_set_summary_mode (baobab.treemap_chart, FALSE);

		gtk_widget_set_sensitive (baobab.chart_type_combo, FALSE);
	}
	else {
		gedit_spinner_stop (GEDIT_SPINNER (baobab.spinner));

		baobab_chart_thaw_updates (baobab.rings_chart);
		baobab_chart_thaw_updates (baobab.treemap_chart);

		gtk_widget_set_sensitive (baobab.chart_type_combo, TRUE);
	}

	/* change the cursor */
	if (baobab.window->window) {
		gdk_window_set_cursor (baobab.window->window, cursor);
	}
}

/* menu & toolbar sensitivity */
static void
check_menu_sens (gboolean scanning)
{
	if (scanning) {

		while (gtk_events_pending ())
			gtk_main_iteration ();

		set_statusbar (_("Scanning..."));
		set_glade_widget_sens ("expand_all", TRUE);
		set_glade_widget_sens ("collapse_all", TRUE);
	}

	set_glade_widget_sens ("tbscanhome", !scanning);
	set_glade_widget_sens ("tbscanall", !scanning);
	set_glade_widget_sens ("tbscandir", !scanning);
	set_glade_widget_sens ("menuscanhome", !scanning);
	set_glade_widget_sens ("menuallfs", !scanning);
	set_glade_widget_sens ("menuscandir", !scanning);
	set_glade_widget_sens ("tbstop", scanning);
	set_glade_widget_sens ("tbrescan", !scanning && current_location != NULL);
	set_glade_widget_sens ("menustop", scanning);
	set_glade_widget_sens ("menurescan", !scanning && current_location != NULL);
	set_glade_widget_sens ("preferenze1", !scanning);
	set_glade_widget_sens ("menu_scan_rem", !scanning);
	set_glade_widget_sens ("tb_scan_remote", !scanning);
	set_glade_widget_sens ("ck_allocated",
			       !scanning &&
			       baobab.is_local && !g_noactivescans);
}

void
baobab_scan_location (GFile *file)
{
	GtkWidget *ck_allocated;

	if (!baobab_check_dir (file))
		return;

	if (iterstack !=NULL)
		return;

	if (current_location)
		g_object_unref (current_location);
	current_location = g_object_ref (file);

	g_noactivescans = FALSE; 
	baobab.STOP_SCANNING = FALSE;
	baobab_set_busy (TRUE);
	check_menu_sens (TRUE);
	gtk_tree_store_clear (baobab.model);
	currentdepth = -1;	/* flag */
	iterstack = g_queue_new ();

	/* check if the file system is local or remote */
	baobab.is_local = scan_is_local (file);
	ck_allocated = glade_xml_get_widget (baobab.main_xml, "ck_allocated");
	if (!baobab.is_local) {
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (ck_allocated),
						FALSE);
		gtk_widget_set_sensitive (ck_allocated, FALSE);
		baobab.show_allocated = FALSE;
	}
	else {
		gtk_widget_set_sensitive (ck_allocated, TRUE);
	}

	baobab_scan_execute (file);

	/* set statusbar, percentage and allocated/normal size */
	set_statusbar (_("Calculating percentage bars..."));
	gtk_tree_model_foreach (GTK_TREE_MODEL (baobab.model),
				show_bars,
				NULL);

	baobab_chart_set_max_depth (baobab.rings_chart, baobab.model_max_depth);
	baobab_chart_set_max_depth (baobab.treemap_chart, baobab.model_max_depth);

	baobab_set_busy (FALSE);
	check_menu_sens (FALSE);
	set_statusbar (_("Ready"));

	gtk_tree_view_columns_autosize (GTK_TREE_VIEW (baobab.tree_view));
	baobab.STOP_SCANNING = TRUE;
	g_queue_free (iterstack);
	iterstack = NULL;
	baobab.CONTENTS_CHANGED_DELAYED = FALSE;
}

void
baobab_scan_home (void)
{
	GFile *file;

	file = g_file_new_for_path (g_get_home_dir ());
	baobab_scan_location (file);
	g_object_unref (file);
}

void
baobab_scan_root (void)
{
	GFile *file;

	file = g_file_new_for_uri ("file:///");
	baobab_scan_location (file);
	g_object_unref (file);
}

void
baobab_rescan_current_dir (void)
{
	g_return_if_fail (current_location != NULL);

	baobab_get_filesystem (&g_fs);
	set_label_scan (&g_fs);
	show_label ();

	g_object_ref (current_location);
	baobab_scan_location (current_location);
	g_object_unref (current_location);
}

void
baobab_stop_scan (void)
{
	baobab.STOP_SCANNING = TRUE;

	set_statusbar (_("Calculating percentage bars..."));
	gtk_tree_model_foreach (GTK_TREE_MODEL (baobab.model),
				show_bars, NULL);
	gtk_tree_view_columns_autosize (GTK_TREE_VIEW (baobab.tree_view));
}

/*
 * pre-fills model during scanning
 */
static void
prefill_model (struct chan_data *data)
{
	GtkTreeIter iter, iterparent;
	char *name;
	char *str;

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

	/* in case filenames contains gmarkup */
	name = g_markup_escape_text (data->display_name, -1);

	str = g_strdup_printf ("<small><i>%s</i></small>", _("Scanning..."));

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (baobab.tree_view), TRUE);
	gtk_tree_store_set (baobab.model, &iter,
			    COL_DIR_NAME, name,
			    COL_H_PARSENAME, "",
			    COL_H_ELEMENTS, -1, 
 			    COL_H_PERC, -1.0,
			    COL_DIR_SIZE, str,
			    COL_ELEMENTS, str, -1);

	g_free (name);
	g_free (str);

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
	size = g_format_size_for_display (g_fs.used);
	if (g_fs.total == 0 && g_fs.used == 0) {
		perc = 100.0;
	} else {
		g_assert (g_fs.total != 0);
		perc = ((gdouble) g_fs.used * 100) / (gdouble) g_fs.total;
	}

	label = g_strdup_printf ("<i>%s</i>", _("Total filesystem usage:"));
	gtk_tree_store_set (baobab.model, &firstiter,
			    COL_DIR_NAME, label,
			    COL_H_PARSENAME, "",
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
	GString *elements;
	char *name;
	char *size;
	char *alloc_size;

	if (data->elements == -1) {
		prefill_model (data);
		return;
	}

	iter = pop_iter_from_stack ();

	/* in case filenames contains gmarkup */
	name = g_markup_escape_text (data->display_name, -1);

	hardlinks = g_string_new ("");
	if (data->tempHLsize > 0) {
		size = g_format_size_for_display (data->tempHLsize);
		g_string_assign (hardlinks, "<i>(");
		g_string_append (hardlinks, _("contains hardlinks for:"));
		g_string_append (hardlinks, " ");
		g_string_append (hardlinks, size);
		g_string_append (hardlinks, ")</i>");
		g_free (size);
	}

	elements = g_string_new ("");
	g_string_printf (elements,
			 ngettext ("% 5d item", "% 5d items",
				   data->elements), data->elements);

	size = g_format_size_for_display (data->size);
	alloc_size = g_format_size_for_display (data->alloc_size);

	gtk_tree_store_set (baobab.model, &iter,
			    COL_DIR_NAME, name,
			    COL_H_PARSENAME, data->parse_name,
			    COL_H_PERC, -1.0,
			    COL_DIR_SIZE,
			    baobab.show_allocated ? alloc_size : size,
			    COL_H_SIZE, data->size,
			    COL_ELEMENTS, elements->str,
			    COL_H_ELEMENTS, data->elements,
			    COL_HARDLINK, hardlinks->str,
			    COL_H_HARDLINK, data->tempHLsize,
			    COL_H_ALLOCSIZE, data->alloc_size, -1);

	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}

	g_string_free (hardlinks, TRUE);
	g_string_free (elements, TRUE);
	g_free (name);
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
baobab_set_excluded_locations (GSList *excluded_uris)
{
	GSList *l;

	g_slist_foreach (baobab.excluded_locations, (GFunc) g_object_unref, NULL);
	g_slist_free (baobab.excluded_locations);
	baobab.excluded_locations = NULL;
	for (l = excluded_uris; l != NULL; l = l->next) {
		baobab.excluded_locations = g_slist_prepend (baobab.excluded_locations,
						g_file_new_for_uri (l->data));
	}
}

gboolean
baobab_is_excluded_location (GFile *file)
{
	gboolean ret = FALSE;
	GSList *l;

	g_return_val_if_fail (file != NULL, FALSE);

	for (l = baobab.excluded_locations; l != NULL; l = l->next) {	
		if (g_file_equal (l->data, file)) {
			ret = TRUE;
			break;
		}
	}

	return ret;
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
	toolbar_reconfigured_cb (item, GEDIT_SPINNER (baobab.spinner));
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
baobab_subfolderstips_toggled (GConfClient *client,
			       guint cnxn_id,
			       GConfEntry *entry,
			       gpointer user_data)
{
	baobab_ringschart_set_subfoldertips_enabled (baobab.rings_chart,
						     gconf_client_get_bool (baobab.gconf_client,
									    BAOBAB_SUBFLSTIPS_VISIBLE_KEY,
									    NULL));
}

static void
store_excluded_locations (void)
{
	GSList *l;
	GSList *uri_list = NULL;

	for (l = baobab.excluded_locations; l != NULL; l = l->next) {
		GSList *uri_list = NULL;

		uri_list = g_slist_prepend (uri_list, g_file_get_uri(l->data));
	}

	gconf_client_set_list (baobab.gconf_client,
			       PROPS_SCAN_KEY,
			       GCONF_VALUE_STRING, 
			       uri_list,
			       NULL);

	g_slist_foreach (uri_list, (GFunc) g_free, NULL);
	g_slist_free (uri_list);
}

static void
sanity_check_excluded_locations (void)
{
	GFile *root;
	GSList *l;

	/* Verify if gconf wrongly contains root dir exclusion, and remove it from gconf. */
	root = g_file_new_for_uri ("file:///");

	for (l = baobab.excluded_locations; l != NULL; l = l->next) {
		if (g_file_equal (l->data, root)) {
			baobab.excluded_locations = g_slist_delete_link (baobab.excluded_locations, l);
			store_excluded_locations ();
			break;			
		}
	}

	g_object_unref (root);
}

static void
baobab_init (void)
{
	GSList *uri_list;
	GFile *file;
	GError *error = NULL;
	monitor_home = NULL;

	/* Load Glade */
	baobab.main_xml = glade_xml_new (BAOBAB_GLADE_FILE,
					 "baobab_window", NULL);
	glade_xml_signal_autoconnect (baobab.main_xml);

	/* Misc */
	baobab.label_scan = NULL;
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
	gconf_client_notify_add (baobab.gconf_client, BAOBAB_SUBFLSTIPS_VISIBLE_KEY, baobab_subfolderstips_toggled,
				 NULL, NULL, NULL);

	uri_list = gconf_client_get_list (baobab.gconf_client,
						      PROPS_SCAN_KEY,
						      GCONF_VALUE_STRING,
						      NULL);

	baobab_set_excluded_locations (uri_list);

	g_slist_foreach (uri_list, (GFunc) g_free, NULL);
	g_slist_free (uri_list);

	sanity_check_excluded_locations ();

	baobab.bbEnableHomeMonitor = gconf_client_get_bool (baobab.gconf_client,
							    PROPS_ENABLE_HOME_MONITOR,
							    NULL);

	baobab_create_toolbar ();

	baobab_create_statusbar ();

	/* start monitoring */
	monitor_vol = g_volume_monitor_get ();
	g_signal_connect (monitor_vol, "volume_changed",
			  G_CALLBACK (volume_changed), NULL);

	file = g_file_new_for_path (g_get_home_dir ());
	monitor_home = g_file_monitor_directory (file, 0, NULL, &error);
	g_object_unref (file);

	if (!monitor_home) {
		message (_("Could not initialize monitoring"),
			 _("Changes to your home folder will not be monitored."),
			 GTK_MESSAGE_WARNING, NULL);
		g_print ("homedir:%s\n", error->message);
		g_error_free (error);
	}
	else {
		g_signal_connect (monitor_home,
				  "changed",
				  G_CALLBACK (contents_changed_cb),
				  NULL);
	}
}

static void
baobab_shutdown (void)
{
	g_free (baobab.label_scan);

	if (current_location)
		g_object_unref (current_location);

	if (monitor_vol)
		g_object_unref (monitor_vol);
	if (monitor_home)
		g_file_monitor_cancel (monitor_home);

	g_free (baobab.selected_path);

	g_slist_foreach (baobab.excluded_locations, (GFunc) g_object_unref, NULL);
	g_slist_free (baobab.excluded_locations);

	if (baobab.gconf_client)
		g_object_unref (baobab.gconf_client);
}

static void
create_context_menu (void)
{
	ContextMenu *menu = NULL;

	baobab.chart_menu = g_new0 (ContextMenu, 1);
	menu = baobab.chart_menu;

	menu->widget = gtk_menu_new ();
		
	menu->up_item = gtk_image_menu_item_new_with_label (_("Move to parent folder")); 
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu->up_item), 
				       gtk_image_new_from_stock(GTK_STOCK_GO_UP, GTK_ICON_SIZE_MENU));
		
	menu->zoom_in_item = gtk_image_menu_item_new_with_label (_("Zoom in")) ;
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu->zoom_in_item), 
				       gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_MENU));
        
	menu->zoom_out_item = gtk_image_menu_item_new_with_label (_("Zoom out"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu->zoom_out_item), 
				       gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU));       
		
	menu->snapshot_item = gtk_image_menu_item_new_with_label (_("Save snapshot"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu->snapshot_item),
									gtk_image_new_from_file (BAOBAB_PIX_DIR "shot.png"));

	gtk_menu_shell_append (GTK_MENU_SHELL (menu->widget),
			       menu->up_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu->widget),
			       gtk_separator_menu_item_new ());
	gtk_menu_shell_append (GTK_MENU_SHELL (menu->widget),
			       menu->zoom_in_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu->widget),
			       menu->zoom_out_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu->widget),
			       gtk_separator_menu_item_new ());
	gtk_menu_shell_append (GTK_MENU_SHELL (menu->widget),
			       menu->snapshot_item);

	/* connect signals */
	g_signal_connect (menu->up_item, "activate",
			  G_CALLBACK (on_move_upwards_cb), NULL);
	g_signal_connect (menu->zoom_in_item, "activate",
			  G_CALLBACK (on_zoom_in_cb), NULL);
	g_signal_connect (menu->zoom_out_item, "activate",
			  G_CALLBACK (on_zoom_out_cb), NULL);
	g_signal_connect (menu->snapshot_item, "activate",
					  G_CALLBACK (on_chart_snapshot_cb), NULL);

	gtk_widget_show_all (menu->widget);
}

static void
initialize_ringschart (void)
{
	GtkWidget *hpaned_main;
	GtkWidget *chart_frame;
	GtkWidget *hbox1;

	chart_frame = gtk_frame_new (NULL);
	gtk_frame_set_label_align (GTK_FRAME (chart_frame), 0.0, 0.0);
	gtk_frame_set_shadow_type (GTK_FRAME (chart_frame), GTK_SHADOW_IN);

	hpaned_main = glade_xml_get_widget (baobab.main_xml, "hpaned_main");
	gtk_paned_pack2 (GTK_PANED (hpaned_main),
			 chart_frame, TRUE, TRUE);
	gtk_paned_set_position (GTK_PANED (hpaned_main), 480);

	baobab.chart_type_combo = gtk_combo_box_new_text ();
	gtk_combo_box_append_text (GTK_COMBO_BOX (baobab.chart_type_combo), 
				   _("View as Rings Chart"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (baobab.chart_type_combo), 
				   _("View as Treemap Chart"));
	gtk_combo_box_set_active (GTK_COMBO_BOX (baobab.chart_type_combo), 0);
	gtk_widget_show (baobab.chart_type_combo);
	g_signal_connect (baobab.chart_type_combo,
			  "changed",
			  G_CALLBACK (on_chart_type_change), NULL);

	hbox1 = glade_xml_get_widget (baobab.main_xml, "hbox1");
	gtk_container_add (GTK_CONTAINER (hbox1), baobab.chart_type_combo);
	gtk_box_set_spacing (GTK_BOX (hbox1), 50);
	gtk_box_set_child_packing (GTK_BOX (hbox1),
				   baobab.chart_type_combo,
				   FALSE,
				   TRUE,
				   0, GTK_PACK_END);

	/* Baobab's Treemap Chart */
	baobab.treemap_chart = baobab_treemap_new ();
	baobab_chart_set_model_with_columns (baobab.treemap_chart,
					     GTK_TREE_MODEL (baobab.model),
					     COL_DIR_NAME,
					     COL_DIR_SIZE,
					     COL_H_PARSENAME,
					     COL_H_PERC,
					     COL_H_ELEMENTS,
					     NULL);
	baobab_chart_set_max_depth (baobab.treemap_chart, 1);
	g_signal_connect (baobab.treemap_chart, "item_activated",
					G_CALLBACK (on_chart_item_activated), NULL);
	g_signal_connect (baobab.treemap_chart, "button-release-event", 
					G_CALLBACK (on_chart_button_release), NULL);
	gtk_widget_show (baobab.treemap_chart);
	/* Ends Baobab's Treemap Chart */

	/* Baobab's Rings Chart */
	baobab.rings_chart = (GtkWidget *) baobab_ringschart_new ();
	baobab_chart_set_model_with_columns (baobab.rings_chart,
					     GTK_TREE_MODEL (baobab.model),
					     COL_DIR_NAME,
					     COL_DIR_SIZE,
					     COL_H_PARSENAME,
					     COL_H_PERC,
					     COL_H_ELEMENTS,
					     NULL);
	baobab_ringschart_set_subfoldertips_enabled (baobab.rings_chart,
						     gconf_client_get_bool (baobab.gconf_client,
									    BAOBAB_SUBFLSTIPS_VISIBLE_KEY,
									    NULL));
	baobab_chart_set_max_depth (baobab.rings_chart, 1);
	g_signal_connect (baobab.rings_chart, "item_activated",
					G_CALLBACK (on_chart_item_activated), NULL);
	g_signal_connect (baobab.rings_chart, "button-release-event", 
					G_CALLBACK (on_chart_button_release), NULL);
	gtk_widget_show (baobab.rings_chart);
	/* Ends Baobab's Treemap Chart */

	baobab.current_chart = baobab.rings_chart;

	g_object_ref_sink (baobab.treemap_chart);
	baobab_chart_freeze_updates (baobab.treemap_chart);

	gtk_container_add (GTK_CONTAINER (chart_frame),
			   baobab.current_chart);
	gtk_widget_show_all (chart_frame);

	create_context_menu ();
}

static gboolean
start_proc_on_command_line (GFile *file)
{
	baobab_scan_location (file);

	return FALSE;
}

int
main (int argc, char *argv[])
{
	GOptionContext *context;
	GError *error = NULL;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	context = g_option_context_new (NULL);
	g_option_context_set_ignore_unknown_options (context, FALSE);
	g_option_context_set_help_enabled (context, TRUE);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));

	g_option_context_parse (context, &argc, &argv, &error);

	if (error) {
		g_critical ("Unable to parse option: %s", error->message);
		g_error_free (error);
		g_option_context_free (context);

		exit (1);
	}
	g_option_context_free (context);

	g_set_application_name ("Baobab");

	glibtop_init ();

	gtk_window_set_default_icon_name ("baobab");

	baobab_init ();

	g_noactivescans = TRUE;
	check_menu_sens (FALSE);
	baobab_get_filesystem (&g_fs);
	if (g_fs.total == 0) {
		GtkWidget *dialog = gtk_message_dialog_new (NULL,
                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                  GTK_MESSAGE_ERROR,
                                  GTK_BUTTONS_CLOSE,
                                  _("Could not detect any mount point."));
                gtk_message_dialog_format_secondary_text
                                                        (GTK_MESSAGE_DIALOG(dialog),
                                                         _("Without mount points disk usage cannot be analyzed."));
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

	set_glade_widget_sens ("menurescan",FALSE);

	/* set allocated space checkbox */
	gtk_check_menu_item_set_active ((GtkCheckMenuItem *)
				      glade_xml_get_widget (baobab.
							    main_xml,
							    "ck_allocated"),
				      baobab.show_allocated);

	gtk_widget_show (baobab.window);

	first_row ();
	set_statusbar (_("Ready"));

	/* The ringschart */
	initialize_ringschart ();

	/* commandline */
	if (argc > 1) {
		GFile *file;

		file = g_file_new_for_commandline_arg (argv[1]);

		/* start processing the dir specified on the
		 * command line as soon as we enter the main loop */
		g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
		                 (GSourceFunc) start_proc_on_command_line,
		                 file,
		                 (GDestroyNotify) g_object_unref);
	}

	gtk_main ();

 closing:
	baobab_shutdown ();

	glibtop_close ();

	return 0;
}
