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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <glibtop/mountlist.h>
#include <glibtop/fsusage.h>
#include <gtk/gtk.h>
#include "baobab.h"
#include "baobab-scan.h"
#include "baobab-treeview.h"
#include "baobab-utils.h"
#include "callbacks.h"
#include "baobab-prefs.h"
#include "baobab-treemap.h"
#include "baobab-ringschart.h"

#define BAOBAB_UI_FILE PKGDATADIR "/baobab-main-window.ui"

static gint currentdepth = 0;
static GtkTreeIter currentiter;
static GtkTreeIter firstiter;
static GQueue *iterstack = NULL;

enum {
	DND_TARGET_URI_LIST
};

static GtkTargetEntry dnd_target_list[] = {
	{ "text/uri-list", 0, DND_TARGET_URI_LIST },
};

static gboolean
scan_is_local (GFile *file)
{
	gchar *uri_scheme;
	gboolean ret;

	uri_scheme = g_file_get_uri_scheme (file);
	ret = (g_ascii_strcasecmp(uri_scheme, "file") == 0);
	g_free (uri_scheme);

	return ret;
}

void
baobab_set_busy (gboolean busy)
{
	static GdkCursor *busy_cursor = NULL;
	GdkCursor *cursor = NULL;
	GdkWindow *window;

	if (busy == TRUE) {
		if (!busy_cursor) {
			busy_cursor = gdk_cursor_new (GDK_WATCH);
		}
		cursor = busy_cursor;

		gtk_widget_show (baobab.spinner);
		gtk_spinner_start (GTK_SPINNER (baobab.spinner));

		baobab_chart_freeze_updates (baobab.rings_chart);

		baobab_chart_freeze_updates (baobab.treemap_chart);

		gtk_widget_set_sensitive (baobab.chart_type_combo, FALSE);
	}
	else {
		gtk_widget_hide (baobab.spinner);
		gtk_spinner_stop (GTK_SPINNER (baobab.spinner));

		baobab_chart_thaw_updates (baobab.rings_chart);
		baobab_chart_thaw_updates (baobab.treemap_chart);

		gtk_widget_set_sensitive (baobab.chart_type_combo, TRUE);
	}

	/* change the cursor */
	window = gtk_widget_get_window (baobab.window);
	if (window) {
		gdk_window_set_cursor (window, cursor);
	}
}

static void
set_drop_target (GtkWidget *target, gboolean active) {
	if (active) {
		gtk_drag_dest_set (GTK_WIDGET (target),
				   GTK_DEST_DEFAULT_DROP | GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT,
				   dnd_target_list,
				   G_N_ELEMENTS (dnd_target_list),
				   GDK_ACTION_COPY);
	} else {
		gtk_drag_dest_unset (target);
	}
}

/* menu & toolbar sensitivity */
static void
check_menu_sens (gboolean scanning)
{
	gboolean has_current_location;

	if (scanning) {
		while (gtk_events_pending ())
			gtk_main_iteration ();

		baobab_set_statusbar (_("Scanning..."));
		set_ui_action_sens ("expand_all", TRUE);
		set_ui_action_sens ("collapse_all", TRUE);
	}

	has_current_location = baobab.current_location != NULL;

	set_ui_action_sens ("menuscanhome", !scanning);
	set_ui_action_sens ("menuallfs", !scanning);
	set_ui_action_sens ("menuscandir", !scanning);
	set_ui_action_sens ("menustop", scanning);
	set_ui_action_sens ("menurescan", !scanning && has_current_location);
	set_ui_action_sens ("preferenze1", !scanning);
	set_ui_action_sens ("menu_scan_rem", !scanning);
	set_ui_action_sens ("ck_allocated", !scanning && baobab.is_local);

	set_ui_widget_sens ("tbscanhome", !scanning);
	set_ui_widget_sens ("tbscanall", !scanning);
	set_ui_widget_sens ("tbscandir", !scanning);
	set_ui_widget_sens ("tbstop", scanning);
	set_ui_widget_sens ("tbrescan", !scanning && has_current_location);
	set_ui_widget_sens ("tb_scan_remote", !scanning);
}

static void
check_drop_targets (gboolean scanning)
{
	set_drop_target (baobab.rings_chart, !scanning);
	set_drop_target (baobab.treemap_chart, !scanning);
}

static void
update_scan_label (void)
{
	gchar *markup;
	gchar *total;
	gchar *used;
	gchar *available;
	GtkWidget *label;

	total = g_format_size (baobab.fstotal);
	used = g_format_size (baobab.fsused);
	available = g_format_size (baobab.fsavail);

	/* Translators: these are labels for disk space */
	markup = g_markup_printf_escaped  ("<small>%s <b>%s</b> (%s %s %s %s )</small>",
					   _("Total filesystem capacity:"), total,
					   _("used:"), used,
					   _("available:"), available);

	g_free (total);
	g_free (used);
	g_free (available);

	label = GTK_WIDGET (gtk_builder_get_object (baobab.main_ui, "label1"));

	gtk_label_set_markup (GTK_LABEL (label), markup);
}

static void
get_filesystem_usage (void)
{
	size_t i;
	glibtop_mountlist mountlist;
	glibtop_mountentry *mountentries;

	mountentries = glibtop_get_mountlist (&mountlist, FALSE);

	baobab.fstotal = 0;
	baobab.fsavail = 0;
	baobab.fsused = 0;

	for (i = 0; i < mountlist.number; ++i) {
		GFile *file;
		glibtop_fsusage fsusage;

		file = g_file_new_for_path (mountentries[i].mountdir);

		if (!baobab_is_excluded_location (file)) {
			glibtop_get_fsusage (&fsusage, mountentries[i].mountdir);

			baobab.fstotal += fsusage.blocks * fsusage.block_size;
			baobab.fsavail += fsusage.bfree * fsusage.block_size;
			baobab.fsused += (fsusage.blocks - fsusage.bfree) * fsusage.block_size;
		}

		g_object_unref (file);
	}

	g_free (mountentries);
}

void
baobab_update_filesystem (void)
{
	get_filesystem_usage ();
	update_scan_label ();
}

void
baobab_scan_location (GFile *file)
{
	GtkToggleAction *ck_allocated;

	if (!baobab_check_dir (file))
		return;

	if (iterstack !=NULL)
		return;

	if (baobab.current_location)
		g_object_unref (baobab.current_location);
	baobab.current_location = g_object_ref (file);

	baobab.STOP_SCANNING = FALSE;
	baobab_set_busy (TRUE);
	check_menu_sens (TRUE);
	check_drop_targets (TRUE);
	gtk_tree_store_clear (baobab.model);
	currentdepth = -1;	/* flag */
	iterstack = g_queue_new ();

	/* check if the file system is local or remote */
	baobab.is_local = scan_is_local (file);
	ck_allocated = GTK_TOGGLE_ACTION (gtk_builder_get_object (baobab.main_ui, "ck_allocated"));
	if (!baobab.is_local) {
		gtk_toggle_action_set_active (ck_allocated, FALSE);
		gtk_action_set_sensitive (GTK_ACTION (ck_allocated), FALSE);
		baobab.show_allocated = FALSE;
	}
	else {
		gtk_action_set_sensitive (GTK_ACTION (ck_allocated), TRUE);
	}

	baobab_scan_execute (file);

	/* set statusbar, percentage and allocated/normal size */
	baobab_set_statusbar (_("Calculating percentage bars..."));
	gtk_tree_model_foreach (GTK_TREE_MODEL (baobab.model),
				show_bars,
				NULL);

	baobab_chart_set_max_depth (baobab.rings_chart, baobab.model_max_depth);
	baobab_chart_set_max_depth (baobab.treemap_chart, baobab.model_max_depth);

	baobab_set_busy (FALSE);
	check_menu_sens (FALSE);
	check_drop_targets (FALSE);
	baobab_set_statusbar (_("Ready"));

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
	g_return_if_fail (baobab.current_location != NULL);

	baobab_update_filesystem ();

	g_object_ref (baobab.current_location);
	baobab_scan_location (baobab.current_location);
	g_object_unref (baobab.current_location);
}

void
baobab_stop_scan (void)
{
	baobab.STOP_SCANNING = TRUE;

	baobab_set_statusbar (_("Calculating percentage bars..."));
	gtk_tree_model_foreach (GTK_TREE_MODEL (baobab.model),
				show_bars, NULL);
	gtk_tree_view_columns_autosize (GTK_TREE_VIEW (baobab.tree_view));
}

static void
push_iter_on_stack (GtkTreeIter *iter)
{
	g_queue_push_head (iterstack, iter->user_data3);
	g_queue_push_head (iterstack, iter->user_data2);
	g_queue_push_head (iterstack, iter->user_data);
	g_queue_push_head (iterstack, GINT_TO_POINTER (iter->stamp));
}

static GtkTreeIter
pop_iter_from_stack (void)
{
	GtkTreeIter iter;

	iter.stamp = GPOINTER_TO_INT (g_queue_pop_head (iterstack));
	iter.user_data = g_queue_pop_head (iterstack);
	iter.user_data2 = g_queue_pop_head (iterstack);
	iter.user_data3 = g_queue_pop_head (iterstack);

	return iter;
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
	push_iter_on_stack (&iter);
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

static void
first_row (void)
{
	char *size;
	gdouble perc;
	char *label;

	GtkTreeIter root_iter;

	gchar *capacity_label, *capacity_size;

	gtk_tree_store_append (baobab.model, &root_iter, NULL);
	capacity_size = g_format_size (baobab.fstotal);

	capacity_label = g_strdup (_("Total filesystem capacity"));
	gtk_tree_store_set (baobab.model, &root_iter,
			    COL_DIR_NAME, capacity_label,
			    COL_H_PARSENAME, "",
			    COL_H_PERC, 100.0,
			    COL_DIR_SIZE, capacity_size,
			    COL_H_SIZE, baobab.fstotal,
			    COL_H_ALLOCSIZE, baobab.fstotal,
			    COL_H_ELEMENTS, -1, -1);
	g_free (capacity_label);
	g_free (capacity_size);

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (baobab.tree_view), FALSE);
	gtk_tree_store_append (baobab.model, &firstiter, &root_iter);
	size = g_format_size (baobab.fsused);
	if (baobab.fstotal == 0 && baobab.fsused == 0) {
		perc = 100.0;
	} else {
		g_assert (baobab.fstotal != 0);
		perc = ((gdouble) baobab.fsused * 100) / (gdouble) baobab.fstotal;
	}

	label = g_strdup (_("Total filesystem usage"));
	gtk_tree_store_set (baobab.model, &firstiter,
			    COL_DIR_NAME, label,
			    COL_H_PARSENAME, "",
			    COL_H_PERC, perc,
			    COL_DIR_SIZE, size,
			    COL_H_SIZE, baobab.fsused,
			    COL_H_ALLOCSIZE, baobab.fsused,
			    COL_H_ELEMENTS, -1, -1);

	g_free (size);
	g_free (label);

	gtk_tree_view_expand_all (GTK_TREE_VIEW (baobab.tree_view));
}

/* fills model during scanning */
void
baobab_fill_model (struct chan_data *data)
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
		size = g_format_size (data->tempHLsize);
		g_string_assign (hardlinks, "<i>(");
		g_string_append (hardlinks, _("contains hardlinks for:"));
		g_string_append (hardlinks, " ");
		g_string_append (hardlinks, size);
		g_string_append (hardlinks, ")</i>");
		g_free (size);
	}

	elements = g_string_new ("");
	g_string_printf (elements,
			 ngettext ("%5d item", "%5d items",
				   data->elements), data->elements);

	size = g_format_size (data->size);
	alloc_size = g_format_size (data->alloc_size);

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

static void
volume_changed (GVolumeMonitor *volume_monitor,
                GVolume        *volume,
                gpointer        user_data)
{
	/* filesystem has changed (mounted or unmounted device) */
	baobab_update_filesystem ();
}

static void
home_contents_changed (GFileMonitor      *file_monitor,
		       GFile             *child,
		       GFile             *other_file,
		       GFileMonitorEvent  event_type,
		       gpointer           user_data)

{
	gchar *excluding;

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

static void
monitor_volume (void)
{
	baobab.monitor_vol = g_volume_monitor_get ();

	g_signal_connect (baobab.monitor_vol, "volume_changed",
			  G_CALLBACK (volume_changed), NULL);
}

static void
monitor_home (gboolean enable)
{
	if (enable && baobab.monitor_home == NULL) {
		GFile *file;
		GError *error = NULL;

		file = g_file_new_for_path (g_get_home_dir ());
		baobab.monitor_home = g_file_monitor_directory (file, 0, NULL, &error);
		g_object_unref (file);

		if (!baobab.monitor_home) {
			message (_("Could not initialize monitoring"),
				 _("Changes to your home folder will not be monitored."),
				 GTK_MESSAGE_WARNING, NULL);
			g_print ("homedir:%s\n", error->message);
			g_error_free (error);
		}
		else {
			g_signal_connect (baobab.monitor_home,
					  "changed",
					  G_CALLBACK (home_contents_changed),
					  NULL);
		}
	}
	else if (!enable && baobab.monitor_home != NULL) {
		g_file_monitor_cancel (baobab.monitor_home);
		g_object_unref (baobab.monitor_home);
		baobab.monitor_home = NULL;
	}
}

void
baobab_set_statusbar (const gchar *text)
{
	gtk_statusbar_pop (GTK_STATUSBAR (baobab.statusbar), 1);
	gtk_statusbar_push (GTK_STATUSBAR (baobab.statusbar), 1, text);

	while (gtk_events_pending ())
		gtk_main_iteration ();
}

static void
toolbar_reconfigured_cb (GtkToolItem *item,
			 GtkWidget   *spinner)
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

	gtk_widget_set_size_request (spinner, size, size);
}

static void
baobab_create_toolbar (void)
{
	GtkWidget *toolbar;
	GtkToolItem *item;
	GtkToolItem *separator;
	gboolean visible;

	toolbar = GTK_WIDGET (gtk_builder_get_object (baobab.main_ui, "toolbar1"));
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

	baobab.spinner = gtk_spinner_new ();
	item = gtk_tool_item_new ();
	gtk_container_add (GTK_CONTAINER (item), baobab.spinner);
	gtk_container_add (GTK_CONTAINER (toolbar), GTK_WIDGET (item));
	gtk_widget_show (GTK_WIDGET (item));

	g_signal_connect (item, "toolbar-reconfigured",
			  G_CALLBACK (toolbar_reconfigured_cb), baobab.spinner);
	toolbar_reconfigured_cb (item, baobab.spinner);

	g_settings_bind (baobab.ui_settings,
			 BAOBAB_SETTINGS_TOOLBAR_VISIBLE,
			 baobab.toolbar, "visible",
			 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (baobab.ui_settings,
			 BAOBAB_SETTINGS_TOOLBAR_VISIBLE,
			 GTK_TOGGLE_ACTION (gtk_builder_get_object (baobab.main_ui, "view_tb")), "active",
			 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (baobab.desktop_settings,
			 DESKTOP_SETTINGS_TOOLBAR_STYLE,
			 baobab.toolbar, "toolbar-style",
			 G_SETTINGS_BIND_GET);
}

static void
baobab_create_statusbar (void)
{
	gboolean visible;

	baobab.statusbar = GTK_WIDGET (gtk_builder_get_object (baobab.main_ui,
							       "statusbar1"));
	if (baobab.statusbar == NULL) {
		g_printerr ("Could not build statusbar\n");
		return;
	}

	g_settings_bind (baobab.ui_settings,
			 BAOBAB_SETTINGS_STATUSBAR_VISIBLE,
			 baobab.statusbar, "visible",
			 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (baobab.ui_settings,
			 BAOBAB_SETTINGS_STATUSBAR_VISIBLE,
			 GTK_TOGGLE_ACTION (gtk_builder_get_object (baobab.main_ui, "view_sb")), "active",
			 G_SETTINGS_BIND_DEFAULT);
}

static void
baobab_set_excluded_locations (gchar **uris)
{
	GSList *l;
	gint i;

	g_slist_foreach (baobab.excluded_locations, (GFunc) g_object_unref, NULL);
	g_slist_free (baobab.excluded_locations);
	baobab.excluded_locations = NULL;
	for (i = 0; uris[i] != NULL; ++i) {
		baobab.excluded_locations = g_slist_prepend (baobab.excluded_locations,
						g_file_new_for_uri (uris[i]));
	}
}

static void
store_excluded_locations (void)
{
	GSList *l;
	GPtrArray *uris;
	GSList *uri_list = NULL;

	uris = g_ptr_array_new ();

	for (l = baobab.excluded_locations; l != NULL; l = l->next) {
		g_ptr_array_add (uris, g_file_get_uri (l->data));
	}

	g_ptr_array_add (uris, NULL);

	g_settings_set_strv (baobab.prefs_settings,
			     BAOBAB_SETTINGS_EXCLUDED_URIS,
			     (const gchar *const *) uris->pdata);

	g_ptr_array_free (uris, TRUE);
}

static void
sanity_check_excluded_locations (void)
{
	GFile *root;
	GSList *l;

	/* Make sure the root dir is not excluded */
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
excluded_uris_changed (GSettings   *settings,
		       const gchar *key,
		       gpointer     user_data)
{
	gchar **uris;

	uris = g_settings_get_strv (settings, key);
	baobab_set_excluded_locations (uris);
	g_strfreev (uris);

	baobab_update_filesystem ();

	gtk_tree_store_clear (baobab.model);
	first_row ();
}

static void
baobab_setup_excluded_locations (void)
{
	gchar **uris;

	uris = g_settings_get_strv (baobab.prefs_settings,
				    BAOBAB_SETTINGS_EXCLUDED_URIS);
	baobab_set_excluded_locations (uris);
	g_strfreev (uris);

	sanity_check_excluded_locations ();

	g_signal_connect (baobab.prefs_settings,
			 "changed::" BAOBAB_SETTINGS_EXCLUDED_URIS,
			  G_CALLBACK (excluded_uris_changed),
			  NULL);
}

static void
baobab_settings_monitor_home_changed (GSettings *settings,
				      const gchar *key,
				      gpointer user_data)
{
	gboolean enable;

	enable = g_settings_get_boolean (settings, key);

	monitor_home (enable);
}

static void
baobab_setup_monitors (void)
{
	gboolean enable;

	monitor_volume ();

	enable = g_settings_get_boolean (baobab.prefs_settings,
					 BAOBAB_SETTINGS_MONITOR_HOME);

	g_signal_connect (baobab.prefs_settings,
			  "changed::" BAOBAB_SETTINGS_MONITOR_HOME,
			  G_CALLBACK (baobab_settings_monitor_home_changed),
			  NULL);

	monitor_home (enable);
}

static void
baobab_init (void)
{
	GError *error = NULL;

	/* init filesystem usage */
	get_filesystem_usage ();

	/* Load the UI */
	baobab.main_ui = gtk_builder_new ();
	gtk_builder_add_from_file (baobab.main_ui, BAOBAB_UI_FILE, &error);

	if (error) {
		g_object_unref (baobab.main_ui);
		g_critical ("Unable to load the user interface file: %s", error->message);
		g_error_free (error);
		exit (1);
	}

	gtk_builder_connect_signals (baobab.main_ui, NULL);

	/* Settings */
	baobab.ui_settings = g_settings_new ("org.gnome.baobab.ui");
	baobab.prefs_settings = g_settings_new ("org.gnome.baobab.preferences");
	baobab.desktop_settings = g_settings_new ("org.gnome.desktop.interface");

	/* Misc */
	baobab.CONTENTS_CHANGED_DELAYED = FALSE;
	baobab.STOP_SCANNING = TRUE;
	baobab.show_allocated = TRUE;
	baobab.is_local = TRUE;

	baobab_setup_excluded_locations ();

	baobab_create_toolbar ();
	baobab_create_statusbar ();

	baobab_setup_monitors ();
}

static void
baobab_shutdown (void)
{
	if (baobab.current_location) {
		g_object_unref (baobab.current_location);
	}

	if (baobab.monitor_vol) {
		g_object_unref (baobab.monitor_vol);
	}

	if (baobab.monitor_home) {
		g_file_monitor_cancel (baobab.monitor_home);
		g_object_unref (baobab.monitor_home);
	}

	g_free (baobab.selected_path);

	g_slist_foreach (baobab.excluded_locations, (GFunc) g_object_unref, NULL);
	g_slist_free (baobab.excluded_locations);

	if (baobab.ui_settings) {
		g_object_unref (baobab.ui_settings);
	}

	if (baobab.prefs_settings) {
		g_object_unref (baobab.prefs_settings);
	}

	if (baobab.desktop_settings) {
		g_object_unref (baobab.desktop_settings);
	}

	g_settings_sync ();
}

static BaobabChartMenu *
create_context_menu (void)
{
	BaobabChartMenu *menu = NULL;

	baobab.chart_menu = g_new0 (BaobabChartMenu, 1);
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

	menu->snapshot_item = gtk_image_menu_item_new_with_label (_("Save screenshot"));
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

	return menu;
}

static void
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

static gboolean
on_chart_button_release (BaobabChart *chart,
			 GdkEventButton *event,
			 gpointer data)
{
	if (baobab_chart_is_frozen (baobab.current_chart))
		return FALSE;

	if (event->button== 3) /* right button */
	{
		GtkTreePath *root_path;
		BaobabChartMenu *menu;

		root_path = baobab_chart_get_root (baobab.current_chart);

		menu = baobab.chart_menu;
		gtk_widget_set_sensitive (menu->up_item,
					  ((root_path != NULL) &&
					  (gtk_tree_path_get_depth (root_path) > 1)));
		gtk_widget_set_sensitive (menu->zoom_in_item,
					  baobab_chart_can_zoom_in (baobab.current_chart));
		gtk_widget_set_sensitive (menu->zoom_out_item,
					  baobab_chart_can_zoom_out (baobab.current_chart));

		/* show the menu */
		gtk_menu_popup (GTK_MENU (menu->widget),
				NULL, NULL, NULL, NULL,
				event->button, event->time);

		gtk_tree_path_free (root_path);
	}

	return FALSE;
}

static void
drag_data_received_handl (GtkWidget *widget,
			 GdkDragContext *context,
			 gint x,
			 gint y,
			 GtkSelectionData *selection_data,
			 guint target_type,
			 guint time,
			 gpointer data)
{
	GFile *gf = NULL;

	/* set "gf" if we got some valid data */
	if ((selection_data != NULL) &&
	    (gtk_selection_data_get_length (selection_data) >= 0) &&
	    (target_type == DND_TARGET_URI_LIST)) {
		gchar **uri_list;
		uri_list = g_uri_list_extract_uris ((const gchar *) gtk_selection_data_get_data (selection_data));
		/* check list is 1 item long */
		if (uri_list != NULL && uri_list[0] != NULL && uri_list[1] == NULL) {
			gf = g_file_new_for_uri (uri_list[0]);
		}
		g_strfreev (uri_list);
	}

	/* success if "gf" has been set */
	if (gf != NULL) {
		/* finish drop before beginning scan, as the drag-drop can
		   probably time out */
		gtk_drag_finish (context, TRUE, FALSE, time);
		baobab_scan_location (gf);
		g_object_unref (gf);
	} else {
		gtk_drag_finish (context, FALSE, FALSE, time);
	}
}

static void
set_active_chart (GtkWidget *chart)
{
	if (baobab.current_chart != chart) {
		if (baobab.current_chart) {
			baobab_chart_freeze_updates (baobab.current_chart);

			g_object_ref (baobab.current_chart);
			gtk_container_remove (GTK_CONTAINER (baobab.chart_frame),
					      baobab.current_chart);
		}

		gtk_container_add (GTK_CONTAINER (baobab.chart_frame), chart);
		g_object_unref (chart);

		baobab_chart_thaw_updates (chart);

		baobab.current_chart = chart;

		gtk_widget_show_all (baobab.chart_frame);

		g_settings_set_string (baobab.ui_settings,
				       BAOBAB_SETTINGS_ACTIVE_CHART,
				       baobab.current_chart == baobab.rings_chart ? "rings" : "treemap");
	}
}

static void
on_chart_type_change (GtkWidget *combo, gpointer user_data)
{
	GtkWidget *chart;
	guint active;

	active = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));

	switch (active) {
	case 0:
		chart = baobab.rings_chart;
		break;
	case 1:
		chart = baobab.treemap_chart;
		break;
	default:
		g_return_if_reached ();
	}

	set_active_chart (chart);
}

static void
initialize_charts (void)
{
	GtkWidget *hpaned_main;
	GtkWidget *hbox1;
	char *saved_chart;

	baobab.chart_frame = gtk_frame_new (NULL);
	gtk_frame_set_label_align (GTK_FRAME (baobab.chart_frame), 0.0, 0.0);
	gtk_frame_set_shadow_type (GTK_FRAME (baobab.chart_frame), GTK_SHADOW_IN);

	hpaned_main = GTK_WIDGET (gtk_builder_get_object (baobab.main_ui, "hpaned_main"));
	gtk_paned_pack2 (GTK_PANED (hpaned_main),
			 baobab.chart_frame, TRUE, TRUE);
	gtk_paned_set_position (GTK_PANED (hpaned_main), 480);

	baobab.chart_type_combo = gtk_combo_box_text_new ();
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (baobab.chart_type_combo),
					_("View as Rings Chart"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (baobab.chart_type_combo),
					_("View as Treemap Chart"));
	gtk_widget_show (baobab.chart_type_combo);
	g_signal_connect (baobab.chart_type_combo,
			  "changed",
			  G_CALLBACK (on_chart_type_change), NULL);

	hbox1 = GTK_WIDGET (gtk_builder_get_object (baobab.main_ui, "hbox1"));
	gtk_container_add (GTK_CONTAINER (hbox1), baobab.chart_type_combo);
	gtk_box_set_spacing (GTK_BOX (hbox1), 50);
	gtk_box_set_child_packing (GTK_BOX (hbox1),
				   baobab.chart_type_combo,
				   FALSE,
				   TRUE,
				   0, GTK_PACK_END);

	baobab.chart_menu = create_context_menu ();

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
	g_signal_connect (baobab.treemap_chart, "drag-data-received",
			  G_CALLBACK (drag_data_received_handl), NULL);
	gtk_widget_show (baobab.treemap_chart);
	g_object_ref_sink (baobab.treemap_chart);
	baobab_chart_freeze_updates (baobab.treemap_chart);

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

	baobab_chart_set_max_depth (baobab.rings_chart, 1);
	g_signal_connect (baobab.rings_chart, "item_activated",
			  G_CALLBACK (on_chart_item_activated), NULL);
	g_signal_connect (baobab.rings_chart, "button-release-event",
			  G_CALLBACK (on_chart_button_release), NULL);
	g_signal_connect (baobab.rings_chart, "drag-data-received",
			  G_CALLBACK (drag_data_received_handl), NULL);
	gtk_widget_show (baobab.rings_chart);
	g_object_ref_sink (baobab.rings_chart);
	baobab_chart_freeze_updates (baobab.rings_chart);

	saved_chart = g_settings_get_string (baobab.ui_settings,
					     BAOBAB_SETTINGS_ACTIVE_CHART);

	if (0 == g_ascii_strcasecmp (saved_chart, "treemap")) {
		set_active_chart (baobab.treemap_chart);
		gtk_combo_box_set_active (GTK_COMBO_BOX (baobab.chart_type_combo), 1);
	} else {
		set_active_chart (baobab.rings_chart);
		gtk_combo_box_set_active (GTK_COMBO_BOX (baobab.chart_type_combo), 0);
	}

	check_drop_targets (FALSE);
}

void
baobab_quit ()
{
	baobab_stop_scan ();
	gtk_main_quit ();
}

static gboolean
start_proc_on_command_line (GFile *file)
{
	baobab_scan_location (file);

	return FALSE;
}

static gboolean
show_version (const gchar *option_name,
              const gchar *value,
              gpointer data,
              GError **error)
{
	g_print("%s %s\n", g_get_application_name (), VERSION);
	exit (0);
	return TRUE; /* It's just good form */
}

int
main (int argc, char *argv[])
{
	gchar **directories = NULL;
	const GOptionEntry options[] = {
		{"version", 'V', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, show_version, N_("Show version"), NULL},
		{G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &directories, NULL, N_("[DIRECTORY]")},
		{NULL}
	};
	GOptionContext *context;
	GError *error = NULL;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_set_application_name ("Baobab");

	context = g_option_context_new (NULL);
	g_option_context_set_ignore_unknown_options (context, FALSE);
	g_option_context_set_help_enabled (context, TRUE);
	g_option_context_add_main_entries(context, options, GETTEXT_PACKAGE);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));

	g_option_context_parse (context, &argc, &argv, &error);

	if (error) {
		g_critical ("Unable to parse option: %s", error->message);
		g_error_free (error);
		g_option_context_free (context);

		exit (1);
	}
	g_option_context_free (context);

	if (directories && directories[0] && directories[1]) {
		g_critical (_("Too many arguments. Only one directory can be specified."));
		exit (1);
	}

	glibtop_init ();

	gtk_window_set_default_icon_name ("baobab");

	baobab_init ();

	if (baobab.fstotal == 0) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (NULL,
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CLOSE,
				_("Could not detect any mount point."));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG(dialog),
				_("Without mount points disk usage cannot be analyzed."));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		goto closing;
	}

	check_menu_sens (FALSE);
	update_scan_label ();

	baobab.window = GTK_WIDGET (gtk_builder_get_object (baobab.main_ui, "baobab_window"));
	gtk_window_set_position (GTK_WINDOW (baobab.window),
				 GTK_WIN_POS_CENTER);

	baobab.tree_view = create_directory_treeview ();

	set_ui_action_sens ("menurescan", FALSE);

	/* set allocated space checkbox */
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (gtk_builder_get_object (baobab.main_ui,
							 "ck_allocated")),
				      baobab.show_allocated);

	gtk_widget_show (baobab.window);

	first_row ();
	baobab_set_statusbar (_("Ready"));

	initialize_charts ();

	/* commandline */
	if (directories && directories[0]) {
		GFile *file;

		file = g_file_new_for_commandline_arg (directories[0]);

		/* start processing the dir specified on the
		 * command line as soon as we enter the main loop */
		g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
		                 (GSourceFunc) start_proc_on_command_line,
		                 file,
		                 (GDestroyNotify) g_object_unref);
	}
	g_strfreev (directories);

	gtk_main ();

 closing:
	baobab_shutdown ();

	glibtop_close ();

	return 0;
}

/* ex:set ts=8 noet: */
