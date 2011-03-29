/*
 * baobab-treemap.c
 *
 * Copyright (C) 2008 Igalia
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
 *
 * Authors:
 *   Fabio Marzocca  <thesaltydog@gmail.com>
 *   Paolo Borelli <pborelli@katamail.com>
 *   Miguel Gomez <magomez@igalia.com>
 *   Eduardo Lima Mitev <elima@igalia.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>
#include <string.h>
#include <gtk/gtk.h>

#include "baobab-chart.h"
#include "baobab-treemap.h"

#define BAOBAB_TREEMAP_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                                         BAOBAB_TREEMAP_TYPE, \
                                         BaobabTreemapPrivate))

#define ITEM_TEXT_PADDING  3
#define ITEM_BORDER_WIDTH  1
#define ITEM_PADDING       6

#define ITEM_MIN_WIDTH    3
#define ITEM_MIN_HEIGHT   3

#define ITEM_SHOW_LABEL   TRUE

G_DEFINE_TYPE (BaobabTreemap, baobab_treemap, BAOBAB_CHART_TYPE);

struct _BaobabTreemapPrivate
{
  guint max_visible_depth;
  gboolean more_visible_childs;
};

static void baobab_treemap_class_init (BaobabTreemapClass *class);
static void baobab_treemap_init (BaobabTreemap *object);
static void baobab_treemap_draw_rectangle (GtkWidget *chart,
                                           cairo_t *cr,
                                           gdouble x, gdouble y, gdouble width, gdouble height,
                                           BaobabChartColor fill_color,
                                           const char *text,
                                           gboolean show_text);
static void baobab_treemap_draw_item (GtkWidget *chart,
                                      cairo_t *cr,
                                      BaobabChartItem *item,
                                      gboolean highlighted);
static void baobab_treemap_calculate_item_geometry (GtkWidget *chart,
                                                    BaobabChartItem *item);
static gboolean baobab_treemap_is_point_over_item (GtkWidget *chart,
                                                   BaobabChartItem *item,
                                                   gdouble x,
                                                   gdouble y);
static void baobab_treemap_get_item_rectangle (GtkWidget *chart,
                                               BaobabChartItem *item);
guint baobab_treemap_can_zoom_in (GtkWidget *chart);
guint baobab_treemap_can_zoom_out (GtkWidget *chart);

static void
baobab_treemap_class_init (BaobabTreemapClass *class)
{
  GObjectClass *obj_class;
  BaobabChartClass *chart_class;

  obj_class = G_OBJECT_CLASS (class);
  chart_class = BAOBAB_CHART_CLASS (class);

  /* BaobabChart abstract methods */
  chart_class->draw_item = baobab_treemap_draw_item;
  chart_class->calculate_item_geometry = baobab_treemap_calculate_item_geometry;
  chart_class->is_point_over_item = baobab_treemap_is_point_over_item;
  chart_class->get_item_rectangle = baobab_treemap_get_item_rectangle;
  chart_class->can_zoom_in        = baobab_treemap_can_zoom_in;
  chart_class->can_zoom_out       = baobab_treemap_can_zoom_out;

  g_type_class_add_private (obj_class, sizeof (BaobabTreemapPrivate));
}

static void
baobab_treemap_init (BaobabTreemap *chart)
{
  BaobabTreemapPrivate *priv;

  priv = BAOBAB_TREEMAP_GET_PRIVATE (chart);

  chart->priv = priv;
}

static void
baobab_treemap_draw_rectangle (GtkWidget *chart,
                               cairo_t *cr,
                               gdouble x, gdouble y, gdouble width, gdouble height,
                               BaobabChartColor fill_color,
                               const char *text,
                               gboolean show_text)
{
  guint border = ITEM_BORDER_WIDTH;
  PangoRectangle rect;
  PangoLayout *layout;

  cairo_stroke (cr);

  cairo_set_line_width (cr, border);
  cairo_rectangle (cr, x + border, y + border,
                   width - border*2,
                   height - border*2);
  cairo_set_source_rgb (cr, fill_color.red, fill_color.green, fill_color.blue);
  cairo_fill_preserve (cr);
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_stroke (cr);

  if ((show_text) && (ITEM_SHOW_LABEL))
    {
      layout = gtk_widget_create_pango_layout (chart, NULL);
      pango_layout_set_markup (layout, text, -1);
      pango_layout_get_pixel_extents (layout, NULL, &rect);

      if ((rect.width + ITEM_TEXT_PADDING*2 <= width) &&
          (rect.height + ITEM_TEXT_PADDING*2 <= height))
        {
          cairo_move_to (cr, x + width/2 - rect.width/2,
                         y + height/2 - rect.height/2);
          pango_cairo_show_layout (cr, layout);
        }

      g_object_unref (layout);
    }
}

static void
baobab_treemap_draw_item (GtkWidget *chart,
                          cairo_t *cr,
                          BaobabChartItem *item,
                          gboolean highlighted)
{
  cairo_rectangle_t * rect;
  BaobabChartColor fill_color;
  GtkAllocation allocation;
  gdouble width, height;

  rect = (cairo_rectangle_t *) item->data;

  gtk_widget_get_allocation (chart, &allocation);

  if (item->depth % 2 != 0)
    {
      baobab_chart_get_item_color (&fill_color, rect->x/allocation.width*200,
                                   item->depth, highlighted);
      width = rect->width - ITEM_PADDING;
      height = rect->height;
    }
  else
    {
      baobab_chart_get_item_color (&fill_color, rect->y/allocation.height*200,
                                   item->depth, highlighted);
      width = rect->width;
      height = rect->height - ITEM_PADDING;
    }

  baobab_treemap_draw_rectangle (chart,
                                 cr,
                                 rect->x,
                                 rect->y,
                                 width,
                                 height,
                                 fill_color,
                                 item->name,
                                 (! item->has_visible_children) );
}

static void
baobab_treemap_calculate_item_geometry (GtkWidget *chart,
                                        BaobabChartItem *item)
{
  BaobabTreemapPrivate *priv;
  cairo_rectangle_t p_area;
  static cairo_rectangle_t *rect;
  gdouble width, height;
  BaobabChartItem *parent = NULL;
  GtkAllocation allocation;

  priv = BAOBAB_TREEMAP (chart)->priv;

  if (item->depth == 0)
    {
      priv->max_visible_depth = 0;
      priv->more_visible_childs = FALSE;
    }

  item->visible = FALSE;

  if (item->parent == NULL)
    {
      gtk_widget_get_allocation (chart, &allocation);
      p_area.x = 0 - ITEM_PADDING/2;
      p_area.y = 0 - ITEM_PADDING/2;
      p_area.width = allocation.width + ITEM_PADDING * 2;
      p_area.height = allocation.height + ITEM_PADDING;
    }
  else
    {
      parent = (BaobabChartItem *) item->parent->data;
      g_memmove (&p_area, parent->data, sizeof (cairo_rectangle_t));
    }

  if (item->data == NULL)
    item->data = g_new (cairo_rectangle_t, 1);

  rect = (cairo_rectangle_t *) item->data;

  if (item->depth % 2 != 0)
    {
      width = p_area.width - ITEM_PADDING;

      rect->x = p_area.x + (item->rel_start * width / 100) + ITEM_PADDING;
      rect->y = p_area.y + ITEM_PADDING;
      rect->width = width * item->rel_size / 100;
      rect->height = p_area.height - ITEM_PADDING * 3;
    }
  else
    {
      height = p_area.height - ITEM_PADDING;

      rect->x = p_area.x + ITEM_PADDING;
      rect->y = p_area.y + (item->rel_start * height / 100) + ITEM_PADDING;
      rect->width = p_area.width - ITEM_PADDING * 3;
      rect->height = height * item->rel_size / 100;
    }

  if ((rect->width - ITEM_PADDING < ITEM_MIN_WIDTH) ||
      (rect->height - ITEM_PADDING < ITEM_MIN_HEIGHT))
    return;

  rect->x = floor (rect->x) + 0.5;
  rect->y = floor (rect->y) + 0.5;
  rect->width = floor (rect->width);
  rect->height = floor (rect->height);

  item->visible = TRUE;

  if (parent != NULL)
    parent->has_visible_children = TRUE;

  baobab_treemap_get_item_rectangle (chart, item);

  if (item->depth == baobab_chart_get_max_depth (chart) + 1)
    priv->more_visible_childs = TRUE;
  else
    priv->max_visible_depth = MAX (priv->max_visible_depth, item->depth);
}

static gboolean
baobab_treemap_is_point_over_item (GtkWidget *chart,
                                   BaobabChartItem *item,
                                   gdouble x,
                                   gdouble y)
{
  GdkRectangle *rect;

  rect = &item->rect;
  return ((x >= rect->x) && (x <= rect->x + rect->width) &&
          (y >= rect->y) && (y <= rect->y + rect->height));
}

static void
baobab_treemap_get_item_rectangle (GtkWidget *chart,
                                   BaobabChartItem *item)
{
  cairo_rectangle_t *_rect;

  _rect = (cairo_rectangle_t *) item->data;

  item->rect.x = _rect->x;
  item->rect.y = _rect->y;
  if (item->depth % 2 != 0)
    {
      item->rect.width = _rect->width - ITEM_PADDING;
      item->rect.height = _rect->height;
    }
  else
    {
      item->rect.width = _rect->width;
      item->rect.height = _rect->height - ITEM_PADDING;
    }

}

guint
baobab_treemap_can_zoom_in (GtkWidget *chart)
{
  BaobabTreemapPrivate *priv;

  priv = BAOBAB_TREEMAP (chart)->priv;

  return MAX (0, (gint) (priv->max_visible_depth - 1));
}

guint
baobab_treemap_can_zoom_out (GtkWidget *chart)
{
  BaobabTreemapPrivate *priv;

  priv = BAOBAB_TREEMAP (chart)->priv;

  return priv->more_visible_childs ? 1 : 0;
}

/* Public functions start here */

/**
 * baobab_treemap_new:
 *
 * Constructor for the baobab_treemap class
 *
 * Returns: a new #BaobabTreemap object
 *
 **/
GtkWidget*
baobab_treemap_new (void)
{
  return g_object_new (BAOBAB_TREEMAP_TYPE, NULL);
}
