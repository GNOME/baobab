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
#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include <string.h>

#include "baobab.h"
#include "baobab-treeview.h"
#include "baobab-cell-renderer-progress.h"
#include "baobab-utils.h"
#include "callbacks.h"

static GtkTreeStore *
create_model (void)
{
	GtkTreeStore *mdl = gtk_tree_store_new (NUM_TREE_COLUMNS,
						G_TYPE_STRING,	/* COL_DIR_NAME */
						G_TYPE_STRING,	/* COL_H_PARSENAME */
						G_TYPE_DOUBLE,	/* COL_H_PERC */
						G_TYPE_STRING,	/* COL_DIR_SIZE */
						G_TYPE_UINT64,	/* COL_H_SIZE */
						G_TYPE_UINT64,	/* COL_H_ALLOCSIZE */
						G_TYPE_STRING,	/* COL_ELEMENTS */
						G_TYPE_INT,	/* COL_H_ELEMENTS */
						G_TYPE_STRING,	/* COL_HARDLINK */
						G_TYPE_UINT64	/* COL_H_HARDLINK */
						);

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
	gchar *parsename = NULL;

	gtk_tree_selection_get_selected (gtk_tree_view_get_selection (treeview), NULL, &iter);

	if (gtk_tree_store_iter_is_valid (baobab.model, &iter)) {
		gtk_tree_model_get (GTK_TREE_MODEL (baobab.model), &iter,
				    COL_H_PARSENAME, &parsename, -1);
	}
}

static void
contents_changed (void)
{
	if (messageyesno (_("Rescan your home folder?"), 
			  _("The content of your home folder has changed. Select rescan to update the disk usage details."),
			  GTK_MESSAGE_QUESTION, _("_Rescan"), baobab.window) == GTK_RESPONSE_OK) {
		baobab_rescan_current_dir ();
	}
	else {
		/* Just update the total */
		baobab_update_filesystem ();
	}
}

static gboolean
on_tv_button_press (GtkWidget *widget,
		    GdkEventButton *event,
		    gpointer data)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	GFile *file;

	gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget),
				       event->x, event->y,
				       &path, NULL, NULL, NULL);
	if (!path)
		return TRUE;		

	/* get the selected path */
	g_free (baobab.selected_path);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (baobab.model), &iter,
				 path);
	gtk_tree_model_get (GTK_TREE_MODEL (baobab.model), &iter,
			    COL_H_PARSENAME, &baobab.selected_path, -1);

	file = g_file_parse_name (baobab.selected_path);

	if (baobab.CONTENTS_CHANGED_DELAYED) {
		GFile *home_file;

		home_file = g_file_new_for_path (g_get_home_dir ());
		if (g_file_has_prefix (file, home_file)) {
			baobab.CONTENTS_CHANGED_DELAYED = FALSE;
			if (baobab.STOP_SCANNING) {
				contents_changed ();
			}
		}
		g_object_unref (home_file);
	}

	/* right-click */
	if (event->button == 3) {

		if (g_file_query_exists (file, NULL)) {
		     popupmenu_list (path, event, can_trash_file (file));
		}
	}

	gtk_tree_path_free (path);
	g_object_unref (file);

	return FALSE;
}

static gboolean
baobab_treeview_equal_func (GtkTreeModel *model,
                            gint column,
                            const gchar *key,
                            GtkTreeIter *iter,
                            gpointer data)
{
	gboolean results = TRUE;
	gchar *name;

	gtk_tree_model_get (model, iter, 1, &name, -1);

	if (name != NULL) {
		gchar * casefold_key;
		gchar * casefold_name;

		casefold_key = g_utf8_casefold (key, -1);
		casefold_name = g_utf8_casefold (name, -1);

		if ((casefold_key != NULL) &&
		    (casefold_name != NULL) &&
		    (strstr (casefold_name, casefold_key) != NULL)) {
			results = FALSE;
		}
		g_free (casefold_key);
		g_free (casefold_name);
		g_free (name);
	}
	return results;
}

static void
perc_cell_data_func (GtkTreeViewColumn *col,
		     GtkCellRenderer *renderer,
		     GtkTreeModel *model,
		     GtkTreeIter *iter,
		     gpointer user_data)
{
	gdouble perc;
	gchar textperc[10];

	gtk_tree_model_get (model, iter, COL_H_PERC, &perc, -1);

 	if (perc < 0)
		strcpy (textperc, "-.- %");
	else if (perc == 100.0)
		strcpy (textperc, "100 %");
	else
 		g_sprintf (textperc, " %.1f %%", perc);

	g_object_set (renderer, "text", textperc, NULL);
}

GtkWidget *
create_directory_treeview (void)
{
	GtkCellRenderer *cell;
	GtkTreeViewColumn *col;
	GtkWidget *scrolled;

	GtkWidget *tvw = GTK_WIDGET (gtk_builder_get_object (baobab.main_ui, "treeview1"));

	g_signal_connect (tvw, "row-expanded",
			  G_CALLBACK (on_tv_row_expanded), NULL);
	g_signal_connect (tvw, "cursor-changed",
			  G_CALLBACK (on_tv_cur_changed), NULL);
	g_signal_connect (tvw, "button-press-event",
			  G_CALLBACK (on_tv_button_press), NULL);

	/* dir name column */
	g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (tvw)), "changed",
			  G_CALLBACK (on_tv_selection_changed), NULL);
	cell = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (NULL, cell, "markup",
							COL_DIR_NAME, "text",
							COL_DIR_NAME, NULL);
	gtk_tree_view_column_set_sort_column_id (col, COL_DIR_NAME);
	gtk_tree_view_column_set_reorderable (col, TRUE);
	gtk_tree_view_column_set_title (col, _("Folder"));
	gtk_tree_view_column_set_sizing (col, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tvw), col);
	
	/* percentage bar & text column */
	col = gtk_tree_view_column_new ();

	cell = baobab_cell_renderer_progress_new ();
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	gtk_tree_view_column_set_attributes (col, cell, "perc",
	                                     COL_H_PERC, NULL);

	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, TRUE);
	gtk_tree_view_column_set_cell_data_func (col, cell,
						 perc_cell_data_func,
						 NULL, NULL);

	g_object_set (G_OBJECT (cell), "xalign", (gfloat) 1.0, NULL);
	gtk_tree_view_column_set_sort_column_id (col, COL_H_PERC);
	gtk_tree_view_column_set_reorderable (col, TRUE);
	gtk_tree_view_column_set_title (col, _("Usage"));
	gtk_tree_view_column_set_sizing (col, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_column_set_resizable (col, FALSE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tvw), col);

	/* directory size column */
	cell = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (NULL, cell, "markup",
							COL_DIR_SIZE, "text",
							COL_DIR_SIZE, NULL);
	g_object_set (G_OBJECT (cell), "xalign", (gfloat) 1.0, NULL);
	gtk_tree_view_column_set_sort_column_id (col,
						 baobab.show_allocated ? COL_H_ALLOCSIZE : COL_H_SIZE);
	gtk_tree_view_column_set_reorderable (col, TRUE);
	gtk_tree_view_column_set_title (col, _("Size"));
	gtk_tree_view_column_set_sizing (col, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tvw), col);

	/* store this column, we need it when toggling 'allocated' */
	g_object_set_data (G_OBJECT (tvw), "baobab_size_col", col);

	/* objects column */
	cell = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (NULL, cell, "markup",
							COL_ELEMENTS, "text",
							COL_ELEMENTS, NULL);
	g_object_set (G_OBJECT (cell), "xalign", (gfloat) 1.0, NULL);
	gtk_tree_view_column_set_sort_column_id (col, COL_H_ELEMENTS);
	gtk_tree_view_column_set_reorderable (col, TRUE);
	gtk_tree_view_column_set_title (col, _("Contents"));
	gtk_tree_view_column_set_sizing (col, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tvw), col);

	/* hardlink column */
	cell = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (NULL, cell, "markup",
							COL_HARDLINK, "text",
						 	COL_HARDLINK, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tvw), col);

	gtk_tree_view_collapse_all (GTK_TREE_VIEW (tvw));
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tvw), FALSE);
	scrolled = GTK_WIDGET (gtk_builder_get_object (baobab.main_ui, "scrolledwindow1"));
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (tvw),
	                                     baobab_treeview_equal_func, 
	                                     NULL, NULL);

	baobab.model = create_model ();

	/* By default, sort by size */
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (baobab.model),
					      baobab.show_allocated ? COL_H_ALLOCSIZE : COL_H_SIZE,
					      GTK_SORT_DESCENDING);

	gtk_tree_view_set_model (GTK_TREE_VIEW (tvw),
				 GTK_TREE_MODEL (baobab.model));
	g_object_unref (baobab.model);

	return tvw;
}

void
baobab_treeview_show_allocated_size (GtkWidget *tv,
				     gboolean show_allocated)
{
	gint sort_id;
	gint new_sort_id;
	GtkSortType order;
	GtkTreeViewColumn *size_col;

	g_return_if_fail (GTK_IS_TREE_VIEW (tv));

	gtk_tree_sortable_get_sort_column_id (GTK_TREE_SORTABLE (baobab.model),
					      &sort_id, &order);

	/* set the sort id for the size column */
	size_col = g_object_get_data (G_OBJECT (tv), "baobab_size_col");
	new_sort_id = show_allocated ? COL_H_ALLOCSIZE : COL_H_SIZE;
	gtk_tree_view_column_set_sort_column_id (size_col, new_sort_id);

	/* if we are currently sorted on size or allocated size,
	 * then trigger a resort (with the same order) */
	if (sort_id == COL_H_SIZE || sort_id == COL_H_ALLOCSIZE) {
		gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (baobab.model),
						      new_sort_id, order);
	}
}
