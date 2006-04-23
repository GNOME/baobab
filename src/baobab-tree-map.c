/*
 **************************************************************************
 *            baobab-tree-map.c
 *
 *  Copyright  2005-2006  Fabio Marzocca <thesaltydog@gmail.com>
 *  
 *
 *  Draw a graphical treemap from a hierarchical data structure
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
 ***************************************************************************
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "baobab-tree-map.h"

struct _rect_coords {
	gdouble x1;
	gdouble y1;
	gdouble x2;
	gdouble y2;
};

typedef struct _rect_coords rect_coords;

#define BAOBAB_TREE_MAP_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), \
					   BAOBAB_TYPE_TREE_MAP, BaobabTreeMapPrivate))


struct _BaobabTreeMapPrivate
{
	GtkTooltips 			*tooltips;
	GnomeCanvasGroup		*group;
	GtkTreeModel			*model;
	gint				COL_FOLDERNAME;
	gint				COL_SIZE;
	gint				req_depth;
	gint				total_elements;
	GtkTreePath			*first_path;
	gchar				*item_name;
};

G_DEFINE_TYPE(BaobabTreeMap, baobab_tree_map, GNOME_TYPE_CANVAS)

static void
baobab_tree_map_finalize (GObject *object)
{
	BaobabTreeMap *treemap = BAOBAB_TREE_MAP(object);

	g_object_unref (treemap->priv->tooltips);

	G_OBJECT_CLASS (baobab_tree_map_parent_class)->finalize (object);
}

static void 
baobab_tree_map_class_init (BaobabTreeMapClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = baobab_tree_map_finalize;

	g_type_class_add_private (object_class, sizeof (BaobabTreeMapPrivate));
}

static gint
item_event (GtkWidget           *item,
	    GdkEventButton      *event,
	    BaobabTreeMap *tm)
{
	switch (event->button) {
	case 1:
	case 3:
		{
			/* get item name */
			GtkTooltipsData * t_data = gtk_tooltips_data_get(item);
			if (t_data) tm->priv->item_name = t_data->tip_text;
		}
		break;
        default:
		break;
	}

	return FALSE;
}

static void
setup_widget_item (BaobabTreeMap *tm,
		   GnomeCanvasItem     *item)
{
	GtkWidget *widget = ((GnomeCanvasWidget *)item)->widget; 

	g_signal_connect (widget,
			  "button-press-event",
			  G_CALLBACK (item_event),
			  tm);
}

static void 
draw_rect (BaobabTreeMap *tm,
	   const rect_coords   *R,
	   guint                color,
	   gchar               *tip)
{
	gnome_canvas_item_new (tm->priv->group,
                               gnome_canvas_rect_get_type(),
			       "x1", R->x1,
			       "y1", R->y1,
			       "x2", R->x2,
			       "y2", R->y2,
			       "fill_color_rgba", color,
			       "outline_color_rgba", 200,
			       "width_pixels", 0, 
			       NULL);

	tm->priv->total_elements++;

	/* setup the event box for tooltips */
	if ((R->x2-R->x1)< 1.5 || (R->y2-R->y1)< 1.5)
		return;

	GtkWidget * widget = gtk_event_box_new ();
	gtk_event_box_set_visible_window ((GtkEventBox*)widget, FALSE);
	gtk_tooltips_set_tip (tm->priv->tooltips, widget, tip, NULL);

    	setup_widget_item(tm,gnome_canvas_item_new (tm->priv->group,
                           gnome_canvas_widget_get_type(),
                           "x", R->x1,
                           "y", R->y1,
                           "width", R->x2-R->x1,
                           "height", R->y2-R->y1,
                           "widget", widget,									 
                           NULL));

	gtk_widget_show(widget);
}

static void
loop_treemap (BaobabTreeMap *tm, 
	      GtkTreeIter          anc_iter, 
	      guint64              anc_size,
	      rect_coords         *R, 
	      gboolean             b_horiz, 
	      gint                 cur_depth)
{
	gchar *name;
	gdouble ratio;
	guint64	cur_size;
	GtkTreeIter cur_iter;
	rect_coords cur_R;

	if (tm->priv->req_depth > -1)
		if (cur_depth > tm->priv->req_depth)
			return;

	cur_R.x1 = R->x1;
	cur_R.y1 = R->y1;
	cur_R.x2 = R->x2;
	cur_R.y2 = R->y2;

	gtk_tree_model_iter_children(tm->priv->model,&cur_iter,&anc_iter);
	do {	
		gtk_tree_model_get(tm->priv->model,&cur_iter,tm->priv->COL_FOLDERNAME,&name,-1);
		gtk_tree_model_get(tm->priv->model,&cur_iter,tm->priv->COL_SIZE,&cur_size,-1);
		if (cur_size == 0 || anc_size ==0) {
			g_free(name);
			continue;
		}
		ratio = (gdouble)cur_size/(gdouble)anc_size;

		/* check if rect is horiz or vert */
		if (!b_horiz) {
			cur_R.x2 = (R->x2-R->x1)*ratio+cur_R.x1;
		}
		else  {
			cur_R.y2 = (R->y2-R->y1)*ratio+cur_R.y1;
		}
			
		draw_rect(tm,(const rect_coords *)&cur_R,g_random_int(),name);
		
		/* recurse if iter has child */		
		if (gtk_tree_model_iter_has_child(tm->priv->model,&cur_iter)) {
			gtk_tree_model_get(tm->priv->model,
						&cur_iter,
						tm->priv->COL_SIZE,
						&cur_size,
						-1);
			loop_treemap(tm, cur_iter,cur_size,&cur_R,!b_horiz, cur_depth+1);
		}
		
		/* set up new rect for next child (sibling)*/
		if (!b_horiz) {
			cur_R.x1 = cur_R.x2;
			cur_R.x2 = R->x2;
		}
		else {
			cur_R.y1 = cur_R.y2;
			cur_R.y2 = R->y2;
		}
		g_free(name);
	} while (gtk_tree_model_iter_next(tm->priv->model,&cur_iter));		
}



static void
baobab_tree_map_init (BaobabTreeMap *treemap)
{

	treemap->priv = BAOBAB_TREE_MAP_GET_PRIVATE (treemap);

	treemap->priv->tooltips = gtk_tooltips_new ();
	g_object_ref (treemap->priv->tooltips);
	gtk_object_sink (GTK_OBJECT(treemap->priv->tooltips));

}

/**************  Start of public functions *****************************/
void
baobab_tree_map_draw (BaobabTreeMap *tm,
		     GtkTreeModel *model,
		     GtkTreePath *path,
		     gint nNameCol,
		     gint nSizeCol,
		     gint required_depth)
{
	GtkTreeIter iter;
	gchar *name;
	gboolean b_horiz;
	guint64 size;
	rect_coords R;

	tm->priv->model = model;
	tm->priv->first_path = path;
	tm->priv->COL_FOLDERNAME = nNameCol;
	tm->priv->COL_SIZE = nSizeCol;
	tm->priv->req_depth = required_depth;
	tm->priv->item_name = NULL;

	gnome_canvas_get_scroll_region (GNOME_CANVAS (tm), &R.x1, &R.y1, &R.x2, &R.y2);
	baobab_tree_map_clear (tm);

	gtk_tree_model_get_iter (tm->priv->model, &iter,tm->priv->first_path);	
	gtk_tree_model_get(tm->priv->model,&iter,tm->priv->COL_FOLDERNAME,&name,-1);

	/* how to draw the rectangle */
	b_horiz = ((R.y2-R.y1) >= (R.x2-R.x1));

	draw_rect (tm, (const rect_coords *)&R, g_random_int(), name);
	
	if (gtk_tree_model_iter_has_child(tm->priv->model,&iter)) {
		gtk_tree_model_get(tm->priv->model,&iter,tm->priv->COL_SIZE,&size,-1);
		loop_treemap(tm,iter,size,&R,b_horiz,1);
	}

	g_free(name);
}

void
baobab_tree_map_refresh(BaobabTreeMap *tm, gint new_depth)
{
	baobab_tree_map_draw (tm, tm->priv->model,
				tm->priv->first_path,
				tm->priv->COL_FOLDERNAME,
				tm->priv->COL_SIZE,
				new_depth);
}


gint
baobab_tree_map_get_total_elements (BaobabTreeMap *tm)
{
	return tm->priv->total_elements;
}

void
baobab_tree_map_clear (BaobabTreeMap *tm)
{
	GList *a, *b;

	a = tm->priv->group->item_list;
	while (a)
	{
		b = a->next;
		gtk_object_destroy (GTK_OBJECT (a->data));
		a = b;
	}

	tm->priv->total_elements = 0;
}

GdkPixbuf *
baobab_tree_map_get_pixbuf (BaobabTreeMap *tm)
{
	gint w,h;
	GdkPixbuf *map_pixbuf;
	
	gdk_drawable_get_size ((GTK_WIDGET(tm))->window, &w, &h);
	map_pixbuf = gdk_pixbuf_get_from_drawable(NULL,
					    GTK_WIDGET (tm)->window,
					    gdk_colormap_get_system(),
					    0, 0,
					    0, 0,
					    w, h);

	return map_pixbuf;
}

gdouble
baobab_tree_map_get_zoom (BaobabTreeMap *tm)
{
	return GNOME_CANVAS (tm)->pixels_per_unit;
}

void
baobab_tree_map_set_zoom (BaobabTreeMap *tm,
			 gdouble        new_zoom)
{
	gnome_canvas_set_pixels_per_unit (GNOME_CANVAS (tm), new_zoom);
}

const gchar *
baobab_tree_map_get_selected_item_name(BaobabTreeMap *tm)
{
	return (const gchar *)tm->priv->item_name;
}


BaobabTreeMap *
baobab_tree_map_new (void)
{
	BaobabTreeMap * tm;
	gint screen_w, screen_h;

	tm = g_object_new (BAOBAB_TYPE_TREE_MAP, "aa", TRUE, NULL); 

	screen_w = gdk_screen_get_width (gdk_screen_get_default());
	screen_h = gdk_screen_get_height (gdk_screen_get_default()) * 0.83;
	gnome_canvas_set_scroll_region (GNOME_CANVAS (tm), 0.0, 0.0, screen_w, screen_h);
	gnome_canvas_set_center_scroll_region (GNOME_CANVAS (tm), TRUE);
	tm->priv->group = gnome_canvas_root (GNOME_CANVAS (tm));

	return tm;
}

