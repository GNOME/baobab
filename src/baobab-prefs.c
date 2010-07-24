/*
 * baobab-prefs.c
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

#include <string.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>
#include <glibtop/mountlist.h>
#include <glibtop/fsusage.h>
#include "baobab.h"
#include "baobab-utils.h"
#include "baobab-prefs.h"

enum
{
	COL_CHECK,
	COL_DEVICE,
	COL_MOUNT_D,
	COL_MOUNT,
	COL_TYPE,
	COL_FS_SIZE,
	COL_FS_AVAIL,
	TOT_COLUMNS
};

static gboolean
add_excluded_item (GtkTreeModel  *model,
		   GtkTreePath   *path,
		   GtkTreeIter   *iter,
		   GSList       **list)
{
	GSList *l;
	gchar *mount;
	gboolean check;

	l = *list;

	gtk_tree_model_get (model,
			    iter,
			    COL_MOUNT, &mount,
			    COL_CHECK, &check,
			    -1);

	if (!check) {
		l = g_slist_prepend (l, mount);
	}

	*list = l;

	return FALSE;
}

static GSList *
get_excluded_locations (GtkTreeModel *model)
{
	GSList *l = NULL;

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) add_excluded_item,
				&l);

	return g_slist_reverse (l);
}

static void
save_gconf (GtkTreeModel *model)
{
	GSList *l;

	l = get_excluded_locations (model);

	gconf_client_set_list (baobab.gconf_client,
			       PROPS_SCAN_KEY, GCONF_VALUE_STRING,
			       l, NULL);

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);
}

static void
enable_home_cb (GtkToggleButton *togglebutton, gpointer user_data)
{
	baobab.bbEnableHomeMonitor = gtk_toggle_button_get_active (togglebutton);

	gconf_client_set_bool (baobab.gconf_client, PROPS_ENABLE_HOME_MONITOR,
			       baobab.bbEnableHomeMonitor, NULL);

}

static void
filechooser_response_cb (GtkDialog *dialog,
                         gint response_id,
                         GtkTreeModel *model)
{
	switch (response_id) {
		case GTK_RESPONSE_HELP:
			baobab_help_display (GTK_WINDOW (baobab.window), 
			                     "baobab.xml", "baobab-preferences");
			break;
		case GTK_RESPONSE_CLOSE:
			save_gconf (model); 
		default:
			gtk_widget_destroy (GTK_WIDGET (dialog));
			break;
	}
}

static void
check_toggled (GtkCellRendererToggle *cell,
	       gchar *path_str,
	       GtkTreeModel *model)
{
	GtkTreeIter iter;
	GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
	gboolean toggle;
	gchar *mountpoint;

	/* get toggled iter */
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model,
			    &iter,
			    COL_CHECK, &toggle,
			    COL_MOUNT_D, &mountpoint,
			    -1);

	/* check if root dir */
	if (strcmp ("/", mountpoint) == 0)
		goto clean_up;

	/* set new value */
	gtk_list_store_set (GTK_LIST_STORE (model),
			    &iter,
			    COL_CHECK, !toggle,
			    -1);

 clean_up:
	g_free (mountpoint);
	gtk_tree_path_free (path);
}

static void
create_tree_props (GtkBuilder *builder, GtkTreeModel *model)
{
	GtkCellRenderer *cell;
	GtkTreeViewColumn *col;
	GtkWidget *tvw;

	tvw = GTK_WIDGET (gtk_builder_get_object (builder , "tree_view_props"));

	/* checkbox column */
	cell = gtk_cell_renderer_toggle_new ();
	g_signal_connect (cell, "toggled",
			  G_CALLBACK (check_toggled), model);

	col = gtk_tree_view_column_new_with_attributes (_("Scan"), cell,
							"active", COL_CHECK,
							NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tvw), col);

	/* First text column */
	cell = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (_("Device"), cell,
							"markup", COL_DEVICE,
							"text", COL_DEVICE,
							NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tvw), col);

	/* second text column */
	cell = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (_("Mount Point"),
							cell, "markup",
							COL_MOUNT_D, "text",
							COL_MOUNT_D, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tvw), col);

	/* third text column */
	cell = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (_("Filesystem Type"),
							cell, "markup",
							COL_TYPE, "text",
							COL_TYPE, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tvw), col);

	/* fourth text column */
	cell = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (_("Total Size"),
							cell, "markup",
							COL_FS_SIZE, "text",
							COL_FS_SIZE, NULL);
	g_object_set (G_OBJECT (cell), "xalign", (gfloat) 1.0, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tvw), col);

	/* fifth text column */
	cell = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (_("Available"),
							cell, "markup",
							COL_FS_AVAIL, "text",
							COL_FS_AVAIL, NULL);
	g_object_set (G_OBJECT (cell), "xalign", (gfloat) 1.0, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tvw), col);

	gtk_tree_view_set_model (GTK_TREE_VIEW (tvw), model);
	g_object_unref (model);
}

static void
fill_props_model (GtkListStore *store)
{
	GtkTreeIter iter;
	guint lo;
	glibtop_mountlist mountlist;
	glibtop_mountentry *mountentry, *mountentry_tofree;
	guint64 fstotal, fsavail;

	mountentry_tofree = glibtop_get_mountlist (&mountlist, 0);

	for (lo = 0, mountentry = mountentry_tofree;
	     lo < mountlist.number;
	     lo++, mountentry++) {
		glibtop_fsusage fsusage;
		gchar * total, *avail;
		GFile *file;
		gchar *uri;
		gboolean excluded;

		struct stat buf;
		if (g_stat (mountentry->devname,&buf) == -1)
			continue;

		glibtop_get_fsusage (&fsusage, mountentry->mountdir);
		fstotal = fsusage.blocks * fsusage.block_size;
		fsavail = fsusage.bfree * fsusage.block_size;
		total = g_format_size_for_display(fstotal);
		avail = g_format_size_for_display(fsavail);
		file = g_file_new_for_path (mountentry->mountdir);
		uri = g_file_get_uri (file);
		excluded = baobab_is_excluded_location (file);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COL_CHECK, !excluded,
				    COL_DEVICE, mountentry->devname,
				    COL_MOUNT_D, mountentry->mountdir,
				    COL_MOUNT, uri,
				    COL_TYPE, mountentry->type,
				    COL_FS_SIZE, total,
				    COL_FS_AVAIL, avail,
				    -1);
		g_free(total);
		g_free(avail);
		g_free(uri);
		g_object_unref(file);
	}

	g_free (mountentry_tofree);
}

void
create_props (void)
{
	GtkBuilder *builder;
	GtkWidget *dlg;
	GtkWidget *check_enablehome;
	GtkListStore *model;
	GError *error = NULL;

	builder = gtk_builder_new ();
	gtk_builder_add_from_file (builder, BAOBAB_DIALOG_SCAN_UI_FILE, &error);

	if (error) {
		g_critical ("Can't load user interface file for the scan properties dialog: %s",
			    error->message);
		g_object_unref (builder);
		g_error_free (error);

		return;
	}

	dlg = GTK_WIDGET (gtk_builder_get_object (builder, "dialog_scan_props"));

	gtk_window_set_transient_for (GTK_WINDOW (dlg),
				      GTK_WINDOW (baobab.window));

	model = gtk_list_store_new (TOT_COLUMNS,
				    G_TYPE_BOOLEAN,	/* checkbox */
				    G_TYPE_STRING,	/* device */
				    G_TYPE_STRING,	/*mount point display */
				    G_TYPE_STRING,	/* mount point uri */
				    G_TYPE_STRING,	/* fs type */
				    G_TYPE_STRING,	/* fs size */
				    G_TYPE_STRING	/* fs avail */
				    );

	create_tree_props (builder, GTK_TREE_MODEL (model));
	fill_props_model (model);

	check_enablehome = GTK_WIDGET (gtk_builder_get_object (builder, "check_enable_home"));
	gtk_toggle_button_set_active ((GtkToggleButton *) check_enablehome,
				      baobab.bbEnableHomeMonitor);

	g_signal_connect_after ((GtkToggleButton *) check_enablehome,
				"toggled", G_CALLBACK (enable_home_cb),
				NULL);

	g_signal_connect (dlg, "response",
		    	  G_CALLBACK (filechooser_response_cb),
		    	  model);

	gtk_widget_show_all (dlg);
}

