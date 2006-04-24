/*
 * baobab-treeview.c
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
#include <string.h>

#include "baobab.h"
#include "baobab-treeview.h"
#include "baobab-utils.h"
#include "callbacks.h"

static GtkTreeStore *
create_model (void)
{
	GtkTreeStore *mdl = gtk_tree_store_new (NUM_TREE_COLUMNS,
						G_TYPE_STRING,	/* COL_DIR_NAME */
						G_TYPE_STRING,	/* COL_H_FULLPATH */
						GDK_TYPE_PIXBUF,/* COL_BAR */
						G_TYPE_FLOAT,	/* COL_H_PERC */
						G_TYPE_STRING,	/* COL_DIR_SIZE */
						G_TYPE_UINT64,	/* COL_H_SIZE */
						G_TYPE_STRING,	/* COL_PERC */
						G_TYPE_UINT64,	/* COL_H_ALLOCSIZE */
						G_TYPE_STRING,	/* COL_ELEMENTS */
						G_TYPE_INT,	/* COL_H_ELEMENTS */
						G_TYPE_STRING,	/* COL_HARDLINK */
						G_TYPE_UINT64	/* COL_H_HARDLINK */
						);

	/* Defaults to sort-by-size */
	gtk_tree_sortable_set_sort_column_id ((GtkTreeSortable *) mdl,
					      COL_H_SIZE,
					      GTK_SORT_DESCENDING);

	return mdl;
}

static GtkListStore *
create_search_model (void)
{
	GtkListStore *mdl;
	GtkTreeIter iter;

	mdl = gtk_list_store_new (NUM_COLUMNS,
				  GDK_TYPE_PIXBUF,	/* icon */
				  G_TYPE_STRING,
				  G_TYPE_STRING,
				  G_TYPE_STRING,	/* fullpath */
				  G_TYPE_LONG,		/* last access */
				  G_TYPE_DOUBLE,	/* size */
				  G_TYPE_STRING,	/* filetype */
				  G_TYPE_UINT		/* owner id (gushort) */
				  );

	gtk_list_store_append (mdl, &iter);
	gtk_list_store_set (mdl, &iter,
			    COL1_STRING, " ", COL_FULLPATH, "", -1);
	gtk_list_store_append (mdl, &iter);
	gtk_list_store_set (mdl, &iter,
			    COL1_STRING,
			    _("<i>Use the Edit->Find menu item "
			      "or the search toolbar button.</i>"),
			    COL_FULLPATH, "", -1);

	/* Defaults to sort-by-size */
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (mdl),
					      COL_SIZE,
					      GTK_SORT_DESCENDING);

	return mdl;
}

static void
on_tv_row_expanded (GtkTreeView *treeview,
		    GtkTreeIter *arg1,
		    GtkTreePath *arg2,
		    gpointer data)
{
	gtk_tree_view_columns_autosize (treeview);
}

static void
on_tv_cur_changed (GtkTreeView *treeview, gpointer data)
{

	GtkTreeIter iter;
	gchar *text = NULL;

	gtk_tree_selection_get_selected (gtk_tree_view_get_selection (treeview), NULL, &iter);

	if (get_NB_page () == VIEW_TREE) {
		if (gtk_tree_store_iter_is_valid (baobab.model, &iter)) {
			gtk_tree_model_get (GTK_TREE_MODEL (baobab.model), &iter,
				    COL_H_FULLPATH, &text, -1);
		}
	}
	else if (get_NB_page () == VIEW_SEARCH) {
		if (gtk_list_store_iter_is_valid (baobab.model_search, &iter)) {
			gtk_tree_model_get (GTK_TREE_MODEL (baobab.model_search),
					    &iter, COL_FULLPATH, &text, -1);
		}
	}

	set_glade_widget_sens("menu_treemap",FALSE);
	if (text) {
		gchar *msg;

		/* make sure it is utf8 */
		msg = g_filename_display_name (text);

		set_statusbar (msg);
		if (get_NB_page () == VIEW_TREE && strcmp (text, "") != 0 )
			set_glade_widget_sens("menu_treemap",TRUE);

		g_free (msg);
		g_free (text);
	}
}

static gboolean
on_tv_button_press (GtkWidget *widget,
		    GdkEventButton *event,
		    gpointer data)
{
	if (get_NB_page () == VIEW_TREE) {
		if (baobab.CONTENTS_CHANGED_DELAYED) {
			baobab.CONTENTS_CHANGED_DELAYED = FALSE;
			if (baobab.STOP_SCANNING) {
				contents_changed ();
			}
		}
	}

	GtkTreePath *path;
	GtkTreeIter iter;

	gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget),
					       event->x, event->y,
					       &path, NULL, NULL, NULL);
	if (!path) 
		return TRUE;		

	/* check if a valid and scanned folder has been selected */
	if (get_NB_page () == VIEW_TREE) {
		if (baobab.selected_path) {
			g_free (baobab.selected_path);
			baobab.selected_path = NULL;
		}

		gtk_tree_model_get_iter (GTK_TREE_MODEL (baobab.model), &iter,
					 path);
		gtk_tree_model_get (GTK_TREE_MODEL (baobab.model), &iter,
				    COL_H_FULLPATH, &baobab.selected_path, -1);

		if (strcmp (baobab.selected_path, "") == 0) {
			set_glade_widget_sens("menu_treemap",FALSE);
			gtk_tree_path_free (path);
			return FALSE;
		}
	}
	
	if (get_NB_page () == VIEW_SEARCH)
		set_glade_widget_sens("menu_treemap",FALSE);
	else
		set_glade_widget_sens("menu_treemap",TRUE);


	/* right-click */
	if (event->button == 3) {

		if (get_NB_page () == VIEW_TREE)
			popupmenu_list (path, event);
		if (get_NB_page () == VIEW_SEARCH)
			popupmenu_list_search (path, event);

		return FALSE;
	}

	gtk_tree_path_free (path);

	return FALSE;
}

GtkWidget *
create_directory_treeview (void)
{
	GtkCellRenderer *cell;
	GtkTreeViewColumn *col;
	GtkWidget *scrolled;

	GtkWidget *tvw = glade_xml_get_widget (baobab.main_xml, "treeview1");

	g_signal_connect (tvw, "row-expanded",
			  G_CALLBACK (on_tv_row_expanded), NULL);
	g_signal_connect (tvw, "cursor-changed",
			  G_CALLBACK (on_tv_cur_changed), NULL);
	g_signal_connect (tvw, "button-press-event",
			  G_CALLBACK (on_tv_button_press), NULL);

	/* dir name column */
	cell = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (NULL, cell, "markup",
							COL_DIR_NAME, "text",
							COL_DIR_NAME, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tvw), col);

	/* percentage bar column */
	cell = gtk_cell_renderer_pixbuf_new ();
	col = gtk_tree_view_column_new_with_attributes (NULL, cell, "pixbuf",
							COL_BAR, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tvw), col);

	/* directory size column */
	cell = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (NULL, cell, "markup",
							COL_DIR_SIZE, "text",
							COL_DIR_SIZE, NULL);
	g_object_set (G_OBJECT (cell), "xalign", (gfloat) 1.0, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tvw), col);

	/* percentage column */
	cell = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (NULL, cell, "markup",
							COL_PERC, "text",
							COL_PERC, NULL);
	g_object_set (G_OBJECT (cell), "xalign", (gfloat) 1.0, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tvw), col);

	/* objects column */
	cell = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (NULL, cell, "markup",
							COL_ELEMENTS, "text",
							COL_ELEMENTS, NULL);
	g_object_set (G_OBJECT (cell), "xalign", (gfloat) 1.0, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tvw), col);

	/* hardlink column */
	cell = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (NULL, cell, "markup",
							COL_HARDLINK, "text",
						 	COL_HARDLINK, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tvw), col);

	gtk_tree_view_collapse_all (GTK_TREE_VIEW (tvw));
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tvw), FALSE);
	scrolled = glade_xml_get_widget (baobab.main_xml, "scrolledwindow1");
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);

	baobab.model = create_model ();

	gtk_tree_view_set_model (GTK_TREE_VIEW (tvw),
				 GTK_TREE_MODEL (baobab.model));
	g_object_unref (baobab.model);

	return tvw;
}

GtkWidget *
create_filesearch_treeview (void)
{
	GtkWidget *tvw;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *col;

	tvw = glade_xml_get_widget (baobab.main_xml, "tree_search");

	g_signal_connect (tvw, "cursor-changed",
			  G_CALLBACK (on_tv_cur_changed), NULL);
	g_signal_connect (tvw, "button-press-event",
			  G_CALLBACK (on_tv_button_press), NULL);

	/* icons column */
	cell = gtk_cell_renderer_pixbuf_new ();
	g_object_set (cell, "stock-size", GTK_ICON_SIZE_LARGE_TOOLBAR,
		      NULL);
	col = gtk_tree_view_column_new_with_attributes (NULL, cell, "pixbuf",
							COL0_ICON, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tvw), col);

	/* First text column */
	cell = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (NULL, cell, "markup",
							COL1_STRING, "text",
							COL1_STRING, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tvw), col);

	/* second text column */
	cell = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (NULL, cell, "markup",
							COL2_STRING, "text",
							COL2_STRING, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tvw), col);

	baobab.model_search = create_search_model ();
	gtk_tree_view_set_model (GTK_TREE_VIEW (tvw),
				 GTK_TREE_MODEL (baobab.model_search));
	g_object_unref (baobab.model_search);

	return tvw;
}
