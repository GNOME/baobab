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
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>
#include <libgnomevfs/gnome-vfs.h>
#include <glibtop/mountlist.h>
#include <glibtop/fsusage.h>
#include "baobab.h"
#include "baobab-utils.h"
#include "baobab-prefs.h"


static gboolean props_changed;

static GtkTreeView *tree_props;
static GtkListStore *model_props;
static GtkListStore *create_props_model (void);
static GtkWidget *create_tree_props (GladeXML *);
static void fill_props_model (GtkWidget *);
static void check_toggled (GtkCellRendererToggle * cell,
			   gchar * path_str, gpointer data);

static void read_gconf (void);
static void save_gconf (void);
static gboolean set_gconf_list (GtkTreeModel * model, GtkTreePath * path,
				GtkTreeIter * iter, gpointer data);

static gboolean set_model_checks (GtkTreeModel * model, GtkTreePath * path,
				  GtkTreeIter * iter, gpointer data);

static void enable_home_cb (GtkToggleButton * togglebutton,
			    gpointer user_data);


void
props_notify (GConfClient *client,
	      guint cnxn_id,
	      GConfEntry *entry,
	      gpointer user_data)
{
	g_slist_foreach (baobab.bbExcludedDirs, (GFunc) g_free, NULL);
	g_slist_free (baobab.bbExcludedDirs);

	baobab.bbExcludedDirs =
	    gconf_client_get_list (client, PROPS_SCAN_KEY,
				   GCONF_VALUE_STRING, NULL);

	baobab_get_filesystem (&g_fs);
	set_label_scan (&g_fs);
	show_label ();
	gtk_tree_store_clear (baobab.model);
	first_row ();
}

static void
filechooser_response_cb (GtkDialog *dialog,
                         gint       response_id,
                         gpointer   user_data)
{
	switch (response_id) {
		case GTK_RESPONSE_HELP:
			baobab_help_display (GTK_WINDOW (baobab.window), 
			                     "baobab.xml", "baobab-preferences");
			break;
		case GTK_RESPONSE_CLOSE:
			if (props_changed) { 
				save_gconf (); 
			}
		default:
			gtk_widget_destroy (GTK_WIDGET (dialog));
			break;
	}
}

void
create_props (void)
{
	GtkWidget *dlg, *check_enablehome;
	GladeXML *dlg_xml;

	props_changed = FALSE;

	/* Glade stuff */
	dlg_xml = glade_xml_new (BAOBAB_GLADE_FILE,
				 "dialog_scan_props", NULL);
	glade_xml_signal_autoconnect (dlg_xml);
	dlg = glade_xml_get_widget (dlg_xml, "dialog_scan_props");

	gtk_window_set_transient_for (GTK_WINDOW (dlg),
				      GTK_WINDOW (baobab.window));

	tree_props = (GtkTreeView *) create_tree_props (dlg_xml);
	fill_props_model (dlg);
	read_gconf ();

	check_enablehome = glade_xml_get_widget (dlg_xml, "check_enable_home");
	gtk_toggle_button_set_active ((GtkToggleButton *) check_enablehome,
				      baobab.bbEnableHomeMonitor);

	g_signal_connect_after ((GtkToggleButton *) check_enablehome,
				"toggled", G_CALLBACK (enable_home_cb),
				NULL);

	g_signal_connect (dlg, "response",
		    	  G_CALLBACK (filechooser_response_cb),
		    	  NULL);

  	gtk_widget_show_all (dlg);
	g_object_unref (dlg_xml);
}

GtkListStore *
create_props_model (void)
{
	GtkListStore *mdl;

	mdl = gtk_list_store_new (TOT_COLUMNS,
				  G_TYPE_BOOLEAN,	/* checkbox */
				  G_TYPE_STRING,	/* device */
				  G_TYPE_STRING,	/* mount point */
				  G_TYPE_STRING,	/* fs type */
				  G_TYPE_STRING,	/* fs size */
				  G_TYPE_STRING		/* fs used */
				  );

	return mdl;
}

GtkWidget *
create_tree_props (GladeXML *dlg_xml)
{
	GtkCellRenderer *cell;
	GtkTreeViewColumn *col;
	GtkWidget *tvw;

	tvw = glade_xml_get_widget (dlg_xml, "tree_view_props");

	/* checkbox column */
	cell = gtk_cell_renderer_toggle_new ();
	g_signal_connect (cell, "toggled",
			  G_CALLBACK (check_toggled), NULL);

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
							COL_MOUNT, "text",
							COL_MOUNT, NULL);
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
	
	model_props = create_props_model ();
	gtk_tree_view_set_model (GTK_TREE_VIEW (tvw),
				 GTK_TREE_MODEL (model_props));
	g_object_unref (model_props);

	return tvw;
}

void
check_toggled (GtkCellRendererToggle *cell,
	       gchar *path_str,
	       gpointer data)
{
	GtkTreeIter iter;
	GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
	gboolean toggle;
	gchar *mountpoint;

	/* get toggled iter */
	gtk_tree_model_get_iter ((GtkTreeModel *) model_props, &iter,
				 path);
	gtk_tree_model_get ((GtkTreeModel *) model_props, &iter, COL_CHECK,
			    &toggle, COL_MOUNT, &mountpoint, -1);

	/* check if root dir */
	if (strcmp ("/", mountpoint) == 0)
		goto clean_up;

	/* set new value */
	props_changed = TRUE;
	toggle ^= 1;
	gtk_list_store_set (GTK_LIST_STORE (model_props), &iter, COL_CHECK,
			    toggle, -1);

 clean_up:
	g_free (mountpoint);
	gtk_tree_path_free (path);
}

void
fill_props_model (GtkWidget *dlg)
{
	GtkTreeIter iter;
	guint lo;
	glibtop_mountlist mountlist;
	glibtop_mountentry *mountentry, *mountentry_tofree;
	guint64 fstotal, fsavail;
	
	mountentry_tofree = glibtop_get_mountlist (&mountlist, 0);

	for (lo = 0, mountentry = mountentry_tofree; lo < mountlist.number;
	     lo++, mountentry++) {
	     
	     	glibtop_fsusage fsusage;
		gchar * total, *avail;
		glibtop_get_fsusage (&fsusage, mountentry->mountdir);
		fstotal = fsusage.blocks * fsusage.block_size;
		fsavail = fsusage.bfree * fsusage.block_size;
		total = gnome_vfs_format_file_size_for_display(fstotal);
		avail = gnome_vfs_format_file_size_for_display(fsavail);
		gtk_list_store_append (model_props, &iter);
			gtk_list_store_set (model_props, &iter,
					    COL_CHECK, TRUE,
					    COL_DEVICE, mountentry->devname, 
					    COL_MOUNT, mountentry->mountdir, 
					    COL_TYPE, mountentry->type,
					    COL_FS_SIZE, total,
					    COL_FS_AVAIL, avail,
					    -1);
		g_free(total);
		g_free(avail);
	}

	g_free (mountentry_tofree);
}

void
read_gconf (void)
{
	GSList *l;

	l = gconf_client_get_list (baobab.gconf_client, PROPS_SCAN_KEY,
				   GCONF_VALUE_STRING, NULL);

	if (!l)
		return;

	gtk_tree_model_foreach (GTK_TREE_MODEL (model_props),
				set_model_checks, l);

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);
}

void
save_gconf (void)
{
	GSList *l = NULL;

	l = g_slist_append (l, "START");
	gtk_tree_model_foreach (GTK_TREE_MODEL (model_props),
				set_gconf_list, l);
	l = g_slist_remove (l, "START");
	gconf_client_set_list (baobab.gconf_client,
			       PROPS_SCAN_KEY, GCONF_VALUE_STRING,
			       l, NULL);

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);
}

gboolean
set_gconf_list (GtkTreeModel *model,
		GtkTreePath *path,
		GtkTreeIter *iter,
		gpointer list)
{
	gchar *mount;
	gboolean check;

	gtk_tree_model_get (model, iter, COL_MOUNT, &mount, COL_CHECK,
			    &check, -1);

	/* add only un-checked mount points */
	if (!check) {
		list = g_slist_last (list);
		list = g_slist_append (list, mount);
	}

	/* g_free(mount); //freed in save_gconf() */
	return FALSE;
}

gboolean
set_model_checks (GtkTreeModel *model,
		  GtkTreePath *path,
		  GtkTreeIter *iter,
		  gpointer list)
{
	gchar *mount;

	gtk_tree_model_get (model, iter, COL_MOUNT, &mount, -1);

	if (g_slist_find_custom (list, mount, list_find) != NULL)
		gtk_list_store_set ((GtkListStore *) model, iter,
				    COL_CHECK, FALSE, -1);

	g_free (mount);

	return FALSE;
}

void
enable_home_cb (GtkToggleButton *togglebutton, gpointer user_data)
{
	baobab.bbEnableHomeMonitor = gtk_toggle_button_get_active (togglebutton);

	gconf_client_set_bool (baobab.gconf_client, PROPS_ENABLE_HOME_MONITOR,
			       baobab.bbEnableHomeMonitor, NULL);

}
