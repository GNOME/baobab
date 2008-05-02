/*
 * baobab-graphwin.c
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

#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glade/glade-xml.h>

#include "baobab.h"
#include "baobab-graphwin.h"
#include "baobab-utils.h"
#include "callbacks.h"
#include "baobab-tree-map.h"

struct _BaobabTreemapWindow {
	/* GUI elements */
	GtkWidget *win_graph;
	BaobabTreeMap *treemap;
	GtkWidget *sw;
	GtkWidget *quit_btn;
	GtkWidget *shot_btn;
	GtkWidget *zoom_in_btn;
	GtkWidget *zoom_out_btn;
	GtkWidget *zoom_100_btn;
	GtkWidget *refresh_btn;
	GtkWidget *depth_spin;
	GtkWidget *entry_total;
	GtkTreePath *path;
};

typedef struct _BaobabTreemapWindow BaobabTreemapWindow;

static void
quit_canvas (GtkWidget *widget, BaobabTreemapWindow *tm)
{
	gtk_widget_destroy (tm->win_graph);
	gtk_tree_path_free (tm->path);
	g_free (tm);
}

static void
zoom_in_cb (GtkWidget *widget, BaobabTreemapWindow *tm)
{
	gdouble d = baobab_tree_map_get_zoom (tm->treemap);
	baobab_tree_map_set_zoom (tm->treemap, d + 0.5);
}

static void
zoom_out_cb (GtkWidget *widget, BaobabTreemapWindow *tm)
{
	gdouble d = baobab_tree_map_get_zoom (tm->treemap);
	if (d <= 0.5)
		return;
	baobab_tree_map_set_zoom (tm->treemap, d - 0.5);
}

static void
zoom_100_cb (GtkWidget *widget, BaobabTreemapWindow *tm)
{
	baobab_tree_map_set_zoom (tm->treemap, 1.0);
}

static gint
tm_popupmenu (GtkWidget *widget, GdkEventButton *event,
	      BaobabTreemapWindow *tm)
{
	GtkWidget *pmenu, *open, *zoom_in, *zoom_100, *zoom_out;
	const gchar *selected_item;

	if (event->button != 3)
		return FALSE;

	if (baobab.selected_path) {
		g_free (baobab.selected_path);
		baobab.selected_path = NULL;
	}

	pmenu = gtk_menu_new ();
	open = gtk_image_menu_item_new_from_stock ("gtk-open", NULL);
	zoom_in = gtk_image_menu_item_new_from_stock ("gtk-zoom-in", NULL);
	zoom_100 = gtk_image_menu_item_new_from_stock ("gtk-zoom-100", NULL);
	zoom_out = gtk_image_menu_item_new_from_stock ("gtk-zoom-out", NULL);

	/* get folder name */
	selected_item = baobab_tree_map_get_selected_item_name (tm->treemap);
	baobab.selected_path = g_strdup (selected_item);

	g_signal_connect (open, "activate",
			  G_CALLBACK (open_file_cb), NULL);
	g_signal_connect (zoom_in, "activate",
			  G_CALLBACK (zoom_in_cb), tm);
	g_signal_connect (zoom_100, "activate",
			  G_CALLBACK (zoom_100_cb), tm);
	g_signal_connect (zoom_out, "activate",
			  G_CALLBACK (zoom_out_cb), tm);

	if (baobab.selected_path) {
		gtk_container_add (GTK_CONTAINER (pmenu), open);
		gtk_widget_show (open);
		gtk_container_add (GTK_CONTAINER (pmenu), zoom_in);
		gtk_widget_show (zoom_in);
		gtk_container_add (GTK_CONTAINER (pmenu), zoom_100);
		gtk_widget_show (zoom_100);
		gtk_container_add (GTK_CONTAINER (pmenu), zoom_out);
		gtk_widget_show (zoom_out);

		gtk_menu_popup (GTK_MENU (pmenu),
				NULL, NULL, NULL, NULL,
				event->button, event->time);
	}

	return FALSE;
}

static void
graph_save_cb (GtkWidget *widget, BaobabTreemapWindow *tm)
{
	GdkPixbuf *map_pixbuf;
	GtkWidget *fs_dlg;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *opt_menu;
	gchar *sel_type;

	map_pixbuf = baobab_tree_map_get_pixbuf (tm->treemap);

	if (!map_pixbuf) {
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (tm->win_graph),
							    GTK_DIALOG_DESTROY_WITH_PARENT,
							    GTK_MESSAGE_ERROR,
							    GTK_BUTTONS_OK,
							    _("Cannot create pixbuf image!"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return;
	}

	fs_dlg = gtk_file_chooser_dialog_new (_("Save Snapshot"),
					      GTK_WINDOW (tm->win_graph),
					      GTK_FILE_CHOOSER_ACTION_SAVE,
					      GTK_STOCK_CANCEL,
					      GTK_RESPONSE_CANCEL,
					      GTK_STOCK_SAVE,
					      GTK_RESPONSE_ACCEPT, NULL);

	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (fs_dlg),
					     g_get_home_dir ());

#if GTK_CHECK_VERSION(2,8,0)
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (fs_dlg), TRUE);
#endif

	/* extra widget */
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 0);
	gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (fs_dlg), vbox);

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 6);

	label = gtk_label_new_with_mnemonic (_("_Image type:"));
	gtk_box_pack_start (GTK_BOX (hbox),
			    label,
			    FALSE, FALSE, 0);

	opt_menu = gtk_combo_box_new_text ();
	gtk_combo_box_append_text (GTK_COMBO_BOX (opt_menu), "jpeg");
	gtk_combo_box_append_text (GTK_COMBO_BOX (opt_menu), "png");
	gtk_combo_box_append_text (GTK_COMBO_BOX (opt_menu), "bmp");
	gtk_combo_box_set_active (GTK_COMBO_BOX (opt_menu), 0);
	gtk_box_pack_start (GTK_BOX (hbox), opt_menu, TRUE, TRUE, 0);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), opt_menu);
	gtk_widget_show_all (vbox);

	if (gtk_dialog_run (GTK_DIALOG (fs_dlg)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fs_dlg));
		sel_type = gtk_combo_box_get_active_text (GTK_COMBO_BOX (opt_menu));
		gdk_pixbuf_save (map_pixbuf, filename, sel_type, NULL, NULL);

		g_free (filename);
		g_free (sel_type);
	}

	gtk_widget_destroy (fs_dlg);
	g_object_unref (map_pixbuf);
}

static void
tm_spin_changed (GtkSpinButton *spinbutton, BaobabTreemapWindow *tm)
{
	gint value;

	value = gtk_spin_button_get_value_as_int (spinbutton);
	if (value == -1)
		gtk_entry_set_text (GTK_ENTRY (spinbutton),
				    _("Unlimited"));
}

static void
tm_refresh (GtkWidget *widget, BaobabTreemapWindow *tm)
{
	gint value;
	gint tot;
	gchar *total;
	GdkCursor *cursor = NULL;

	/* change the cursor */
	if (tm->win_graph->window) {
		cursor = gdk_cursor_new (GDK_WATCH);
		gdk_window_set_cursor (tm->win_graph->window, cursor);
	}

	value = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (tm->depth_spin));
	baobab_tree_map_refresh (tm->treemap, value);

	tot = baobab_tree_map_get_total_elements (tm-> treemap);
	total = g_strdup_printf ("%d", tot);
	gtk_entry_set_text (GTK_ENTRY (tm->entry_total), total);
	g_free (total);

	/* cursor clean up */
	if (tm->win_graph->window) {
		gdk_window_set_cursor (tm->win_graph->window, NULL);
		if (cursor)
			gdk_cursor_unref (cursor);
	}
}

void
baobab_graphwin_create (GtkTreeModel *mdl,
			const gchar *path_to_string,
			gchar *gladepath,
			gint nFolderNameCol,
			gint nAllocSizeCol,
			gint required_depth)
{
	GladeXML *graph_xml;
	gint tot;
	gchar *total, *dir;
	GtkTreeIter iter;
	gchar *title;
	GdkCursor *cursor = NULL;
	BaobabTreemapWindow *bb_tm = NULL;

	bb_tm = g_new0 (BaobabTreemapWindow, 1);
	if (!bb_tm) {
		GtkWidget *dialog = gtk_message_dialog_new (NULL,
							    GTK_DIALOG_DESTROY_WITH_PARENT,
							    GTK_MESSAGE_ERROR,
							    GTK_BUTTONS_OK,
							    _("Cannot allocate memory for graphical window!"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return;
	}

	/* Get the path & folder name */
	bb_tm->path = gtk_tree_path_new_from_string (path_to_string);
	gtk_tree_model_get_iter (mdl, &iter, bb_tm->path);
	gtk_tree_model_get (mdl, &iter, nFolderNameCol, &dir, -1);

	/* Glade stuff */
	graph_xml = glade_xml_new (gladepath, "graph_win", NULL);
	glade_xml_signal_autoconnect (graph_xml);
	bb_tm->win_graph = glade_xml_get_widget (graph_xml, "graph_win");
	bb_tm->sw = glade_xml_get_widget (graph_xml, "graph_scroll");
	bb_tm->quit_btn = glade_xml_get_widget (graph_xml, "graph_close_btn");
	bb_tm->shot_btn = glade_xml_get_widget (graph_xml, "graph_save");
	bb_tm->zoom_in_btn = glade_xml_get_widget (graph_xml, "graph_zoom_in");
	bb_tm->zoom_100_btn = glade_xml_get_widget (graph_xml, "graph_zoom_100");
	bb_tm->zoom_out_btn = glade_xml_get_widget (graph_xml, "graph_zoom_out");
	bb_tm->refresh_btn = glade_xml_get_widget (graph_xml, "graph_refresh");
	bb_tm->depth_spin = glade_xml_get_widget (graph_xml, "graph_spin");
	bb_tm->entry_total = glade_xml_get_widget (graph_xml, "graph_total");

	g_object_unref (graph_xml);

	/* Main treemap window */
	title = g_strdup_printf ("%s - %s", dir, _("Graphical Usage Map"));
	gtk_window_set_title (GTK_WINDOW (bb_tm->win_graph), title);
	g_free (title);

	gtk_window_maximize (GTK_WINDOW (bb_tm->win_graph));

	gtk_editable_set_editable (GTK_EDITABLE (bb_tm->depth_spin), FALSE);
	gtk_entry_set_width_chars (GTK_ENTRY (bb_tm->depth_spin), 11);
	if (required_depth == -1)
		gtk_entry_set_text (GTK_ENTRY (bb_tm->depth_spin),
				    _("Unlimited"));

	/* The treemap */
	bb_tm->treemap = baobab_tree_map_new ();
	gtk_container_add (GTK_CONTAINER (bb_tm->sw),
			   GTK_WIDGET (bb_tm->treemap));
	gtk_widget_show_all (bb_tm->win_graph);

	/* connect signals */
	g_signal_connect (bb_tm->quit_btn, "clicked",
			  G_CALLBACK (quit_canvas), bb_tm);
	g_signal_connect (bb_tm->shot_btn, "clicked",
			  G_CALLBACK (graph_save_cb), bb_tm);
	g_signal_connect (bb_tm->zoom_in_btn, "clicked",
			  G_CALLBACK (zoom_in_cb), bb_tm);
	g_signal_connect (bb_tm->zoom_100_btn, "clicked",
			  G_CALLBACK (zoom_100_cb), bb_tm);
	g_signal_connect (bb_tm->zoom_out_btn, "clicked",
			  G_CALLBACK (zoom_out_cb), bb_tm);
	g_signal_connect (bb_tm->depth_spin, "value-changed",
			  G_CALLBACK (tm_spin_changed), bb_tm);
	g_signal_connect (bb_tm->refresh_btn, "clicked",
			  G_CALLBACK (tm_refresh), bb_tm);
	g_signal_connect (bb_tm->treemap, "button-press-event",
			  G_CALLBACK (tm_popupmenu), bb_tm);

	/* change the cursor */
	if (bb_tm->win_graph->window) {
		cursor = gdk_cursor_new (GDK_WATCH);
		gdk_window_set_cursor (bb_tm->win_graph->window, cursor);
	}

	/* Draw the treemap */
	baobab_tree_map_draw (bb_tm->treemap,
			      mdl,
			      bb_tm->path,
			      nFolderNameCol,
			      nAllocSizeCol, required_depth);

	/* set the total folders entry */
	tot = baobab_tree_map_get_total_elements (bb_tm->treemap);
	total = g_strdup_printf ("%d", tot);
	gtk_entry_set_text (GTK_ENTRY (bb_tm->entry_total), total);
	g_free (total);

	/* cursor clean up */
	if (bb_tm->win_graph->window) {
		gdk_window_set_cursor (bb_tm->win_graph->window, NULL);
		if (cursor)
			gdk_cursor_unref (cursor);
	}

	g_free (dir);
}

