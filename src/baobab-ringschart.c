/*
 * baobab-ringschart.c
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
 *   Felipe Erias <femorandeira@igalia.com>
 *   Pablo Santamaria <psantamaria@igalia.com>
 *   Jacobo Aragunde <jaragunde@igalia.com>
 *   Eduardo Lima <elima@igalia.com>
 *   Mario Sanchez <msanchez@igalia.com>
 *   Miguel Gomez <magomez@igalia.com>
 *   Henrique Ferreiro <hferreiro@igalia.com>
 *   Alejandro Pinheiro <apinheiro@igalia.com>
 *   Carlos Sanmartin <csanmartin@igalia.com>
 *   Alejandro Garcia <alex@igalia.com>
 */

#include <math.h>
#include <string.h>

#include <gtk/gtk.h>
#include <pango/pangocairo.h>

#include "baobab-chart.h"
#include "baobab-ringschart.h"

#define BAOBAB_RINGSCHART_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                                            BAOBAB_RINGSCHART_TYPE, \
                                            BaobabRingschartPrivate))

#define ITEM_BORDER_WIDTH  1
#define CHART_PADDING     13
#define ITEM_MIN_ANGLE     0.03
#define EDGE_ANGLE         0.004

#define SUBFOLDER_TIP_PADDING 3

G_DEFINE_TYPE (BaobabRingschart, baobab_ringschart, BAOBAB_CHART_TYPE);

typedef struct _BaobabRingschartItem BaobabRingschartItem;

struct _BaobabRingschartItem
{
  gdouble min_radius;
  gdouble max_radius;
  gdouble start_angle;
  gdouble angle;
  gboolean continued;
};

struct _BaobabRingschartPrivate
{
  gboolean subfoldertips_enabled;
  BaobabChartItem *highlighted_item;
  guint tips_timeout_event;
  GList *subtip_items;
  gboolean drawing_subtips;
  gint subtip_timeout;
};

static void baobab_ringschart_class_init (BaobabRingschartClass *class);
static void baobab_ringschart_init (BaobabRingschart *object);
static void baobab_ringschart_draw_sector (cairo_t *cr,
                                           gdouble center_x, gdouble center_y,
                                           gdouble radius, gdouble thickness,
                                           gdouble init_angle, gdouble final_angle,
                                           BaobabChartColor fill_color,
                                           gboolean continued, guint border);
static void baobab_ringschart_draw_item (GtkWidget *chart,
                                         cairo_t *cr,
                                         BaobabChartItem *item,
                                         gboolean highlighted);
static void baobab_ringschart_calculate_item_geometry (GtkWidget *chart,
                                                       BaobabChartItem *item);
static gboolean baobab_ringschart_is_point_over_item (GtkWidget *chart,
                                                      BaobabChartItem *item,
                                                      gdouble x,
                                                      gdouble y);
static void baobab_ringschart_get_point_min_rect (gdouble cx, gdouble cy,
                                                  gdouble radius, gdouble angle,
                                                  GdkRectangle *rect);
static void baobab_ringschart_get_item_rectangle (GtkWidget *chart,
                                                  BaobabChartItem *item);
static void baobab_ringschart_pre_draw (GtkWidget *chart, cairo_t *cr);
static void baobab_ringschart_post_draw (GtkWidget *chart, cairo_t *cr);


static void
baobab_ringschart_class_init (BaobabRingschartClass *class)
{
  GObjectClass *obj_class;
  BaobabChartClass *chart_class;

  obj_class = G_OBJECT_CLASS (class);
  chart_class = BAOBAB_CHART_CLASS (class);

  /* BaobabChart abstract methods */
  chart_class->draw_item = baobab_ringschart_draw_item;
  chart_class->calculate_item_geometry = baobab_ringschart_calculate_item_geometry;
  chart_class->is_point_over_item = baobab_ringschart_is_point_over_item;
  chart_class->get_item_rectangle = baobab_ringschart_get_item_rectangle;
  chart_class->pre_draw = baobab_ringschart_pre_draw;
  chart_class->post_draw = baobab_ringschart_post_draw;

  g_type_class_add_private (obj_class, sizeof (BaobabRingschartPrivate));
}

static void
baobab_ringschart_init (BaobabRingschart *chart)
{
  BaobabRingschartPrivate *priv;
  GtkSettings* settings;
  gint timeout;

  priv = BAOBAB_RINGSCHART_GET_PRIVATE (chart);

  priv->subfoldertips_enabled = FALSE;
  priv->highlighted_item = NULL;
  priv->tips_timeout_event = 0;
  priv->subtip_items = NULL;
  priv->drawing_subtips = FALSE;

  settings = gtk_settings_get_default ();
  g_object_get (G_OBJECT (settings), "gtk-tooltip-timeout", &timeout, NULL);
  priv->subtip_timeout = 2 * timeout;
}

static void
baobab_ringschart_draw_sector (cairo_t *cr,
                               gdouble center_x,
                               gdouble center_y,
                               gdouble radius,
                               gdouble thickness,
                               gdouble init_angle,
                               gdouble final_angle,
                               BaobabChartColor fill_color,
                               gboolean continued,
                               guint border)
{
  cairo_set_line_width (cr, border);
  if (radius > 0)
    cairo_arc (cr, center_x, center_y, radius, init_angle, final_angle);
  cairo_arc_negative (cr, center_x, center_y, radius+thickness,
                      final_angle, init_angle);
  cairo_close_path(cr);

  cairo_set_source_rgb (cr, fill_color.red, fill_color.green, fill_color.blue);
  cairo_fill_preserve (cr);
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_stroke (cr);

  if (continued)
    {
      cairo_set_line_width (cr, 3);
      cairo_arc (cr, center_x, center_y, radius+thickness + 4,
                 init_angle+EDGE_ANGLE, final_angle-EDGE_ANGLE);
      cairo_stroke (cr);
    }
}

static void
baobab_ringschart_draw_item (GtkWidget *chart,
                             cairo_t *cr,
                             BaobabChartItem *item,
                             gboolean highlighted)
{
  BaobabRingschartPrivate *priv;
  BaobabRingschartItem *data;
  BaobabChartColor fill_color;
  GtkAllocation allocation;

  priv = BAOBAB_RINGSCHART_GET_PRIVATE (chart);

  data = (BaobabRingschartItem *) item->data;

  if (priv->drawing_subtips)
    if ( (priv->highlighted_item) && (item->parent) &&
         (((BaobabChartItem *) item->parent->data) == priv->highlighted_item) )
      {
        GList *node;
        node = g_new0 (GList, 1);
        node->data = (gpointer) item;

        node->next = priv->subtip_items;
        if (priv->subtip_items)
          priv->subtip_items->prev = node;
        priv->subtip_items = node;
      }

  baobab_chart_get_item_color (&fill_color,
                               data->start_angle / M_PI * 99,
                               item->depth,
                               highlighted);

  gtk_widget_get_allocation (chart, &allocation);
  baobab_ringschart_draw_sector (cr,
                                 allocation.width / 2,
                                 allocation.height / 2,
                                 data->min_radius,
                                 data->max_radius - data->min_radius,
                                 data->start_angle,
                                 data->start_angle + data->angle,
                                 fill_color,
                                 data->continued,
                                 ITEM_BORDER_WIDTH);
}

static void
baobab_ringschart_calculate_item_geometry (GtkWidget *chart,
                                           BaobabChartItem *item)
{
  BaobabRingschartItem *data;
  BaobabRingschartItem p_data;
  BaobabChartItem *parent = NULL;
  GtkAllocation allocation;

  gdouble max_radius;
  gdouble thickness;
  guint max_depth;

  max_depth = baobab_chart_get_max_depth (chart);

  if (item->data == NULL)
    item->data = g_new (BaobabRingschartItem, 1);

  data = (BaobabRingschartItem *) item->data;

  data->continued = FALSE;
  item->visible = FALSE;

  gtk_widget_get_allocation (chart, &allocation);
  max_radius = MIN (allocation.width / 2, allocation.height / 2) - CHART_PADDING;
  thickness = max_radius / (max_depth + 1);

  if (item->parent == NULL)
    {
      data->min_radius = 0;
      data->max_radius = thickness;
      data->start_angle = 0;
      data->angle = 2 * M_PI;
    }
  else
    {
      parent = (BaobabChartItem *) item->parent->data;
      g_memmove (&p_data, parent->data, sizeof (BaobabRingschartItem));

      data->min_radius = (item->depth) * thickness;

      if (data->min_radius + thickness > max_radius)
        return;
      else
        data->max_radius = data->min_radius + thickness;

      data->angle = p_data.angle * item->rel_size / 100;
      if (data->angle < ITEM_MIN_ANGLE)
        return;

      data->start_angle = p_data.start_angle + p_data.angle * item->rel_start / 100;

      data->continued = (item->has_any_child) && (item->depth == max_depth);

      parent->has_visible_children = TRUE;
    }

  item->visible = TRUE;
  baobab_ringschart_get_item_rectangle (chart, item);
}

static gboolean
baobab_ringschart_is_point_over_item (GtkWidget *chart,
                                      BaobabChartItem *item,
                                      gdouble x,
                                      gdouble y)
{
  BaobabRingschartItem *data;
  gdouble radius, angle;
  GtkAllocation allocation;

  data = (BaobabRingschartItem *) item->data;
  gtk_widget_get_allocation (chart, &allocation);
  x = x - allocation.width / 2;
  y = y - allocation.height / 2;

  radius = sqrt (x*x + y*y);
  angle = atan2 (y, x);
  angle = (angle > 0) ? angle : angle + 2*G_PI;

  return (radius >= data->min_radius) &&
         (radius <= data->max_radius) &&
         (angle >= data->start_angle) &&
         (angle <= data->start_angle + data->angle);
}

static void
baobab_ringschart_get_point_min_rect (gdouble cx,
                                      gdouble cy,
                                      gdouble radius,
                                      gdouble angle,
                                      GdkRectangle *rect)
{
  gdouble x, y;

  x = cx + cos (angle) * radius;
  y = cy + sin (angle) * radius;

  rect->x = MIN (rect->x, x);
  rect->y = MIN (rect->y, y);
  rect->width = MAX (rect->width, x);
  rect->height = MAX (rect->height, y);
}

static void
baobab_ringschart_get_item_rectangle (GtkWidget *chart,
                                       BaobabChartItem *item)
{
  BaobabRingschartItem *data;
  GdkRectangle rect;
  gdouble cx, cy, r1, r2, a1, a2;
  GtkAllocation allocation;

  data = (BaobabRingschartItem *) item->data;

  gtk_widget_get_allocation (chart, &allocation);
  cx = allocation.width / 2;
  cy = allocation.height / 2;
  r1 = data->min_radius;
  r2 = data->max_radius;
  a1 = data->start_angle;
  a2 = data->start_angle + data->angle;

  rect.x = allocation.width;
  rect.y = allocation.height;
  rect.width = 0;
  rect.height = 0;

  baobab_ringschart_get_point_min_rect (cx, cy, r1, a1, &rect);
  baobab_ringschart_get_point_min_rect (cx, cy, r2, a1, &rect);
  baobab_ringschart_get_point_min_rect (cx, cy, r1, a2, &rect);
  baobab_ringschart_get_point_min_rect (cx, cy, r2, a2, &rect);

  if ( (a1 <= M_PI/2) && (a2 >= M_PI/2) )
    rect.height = MAX (rect.height, cy + sin (M_PI/2) * r2);

  if ( (a1 <= M_PI) && (a2 >= M_PI) )
    rect.x = MIN (rect.x, cx + cos (M_PI) * r2);

  if ( (a1 <= M_PI*1.5) && (a2 >= M_PI*1.5) )
    rect.y = MIN (rect.y, cy + sin (M_PI*1.5) * r2);

  if ( (a1 <= M_PI*2) && (a2 >= M_PI*2) )
    rect.width = MAX (rect.width, cx + cos (M_PI*2) * r2);

  rect.width -= rect.x;
  rect.height -= rect.y;

  item->rect = rect;
}

gboolean
baobab_ringschart_subfolder_tips_timeout (gpointer data)
{
  BaobabRingschartPrivate *priv;

  priv = BAOBAB_RINGSCHART_GET_PRIVATE (data);

  priv->drawing_subtips = TRUE;

  gtk_widget_queue_draw (GTK_WIDGET (data));

  return FALSE;
}

void
baobab_ringschart_clean_subforlder_tips_state (GtkWidget *chart)
{
  BaobabRingschartPrivate *priv;
  GList *node;

  priv = BAOBAB_RINGSCHART_GET_PRIVATE (chart);

  if (priv->drawing_subtips)
    gtk_widget_queue_draw (chart);

  priv->drawing_subtips = FALSE;

  if (priv->highlighted_item == NULL)
    return;

  if (priv->tips_timeout_event)
    {
      g_source_remove (priv->tips_timeout_event);
      priv->tips_timeout_event = 0;
    }

  priv->highlighted_item = NULL;

  /* Free subtip_items GList */
  node = priv->subtip_items;
  while (node != NULL)
    {
      priv->subtip_items = node->next;
      g_free (node);

      node = priv->subtip_items;
    }
  priv->subtip_items = NULL;
}

static void
baobab_ringschart_draw_subfolder_tips (GtkWidget *chart, cairo_t *cr)
{
  BaobabRingschartPrivate *priv;
  GList *node;
  BaobabChartItem *item;
  BaobabRingschartItem *data;

  gdouble q_angle, q_width, q_height;

  gdouble tip_angle, tip_x, tip_y;
  gdouble middle_angle, middle_angle_n, middle_radius;
  gdouble min_angle, max_angle;
  gdouble sector_center_x, sector_center_y;
  gdouble a;
  guint i;
  GtkAllocation allocation;

  PangoLayout *layout;
  PangoRectangle layout_rect;
  gchar *markup = NULL;

  cairo_rectangle_t tooltip_rect;
  GdkRectangle _rect, last_rect;

  priv = BAOBAB_RINGSCHART_GET_PRIVATE (chart);

  gtk_widget_get_allocation (chart, &allocation);
  q_width = allocation.width / 2;
  q_height = allocation.height / 2;
  q_angle = atan2 (q_height, q_width);

  memset (&last_rect, 0, sizeof (GdkRectangle));

  cairo_save (cr);

  node = priv->subtip_items;
  while (node != NULL)
    {
      item = (BaobabChartItem *) node->data;
      data = (BaobabRingschartItem *) item->data;

      /* get the middle angle */
      middle_angle = data->start_angle + data->angle / 2;

      /* normalize the middle angle (take it to the first quadrant) */
      middle_angle_n = middle_angle;
      while (middle_angle_n > M_PI/2)
        middle_angle_n -= M_PI;
      middle_angle_n = ABS (middle_angle_n);

      /* get the pango layout and its enclosing rectangle */
      layout = gtk_widget_create_pango_layout (chart, NULL);
      markup = g_strconcat ("<span size=\"small\">", item->name, "</span>", NULL);
      pango_layout_set_markup (layout, markup, -1);
      g_free (markup);
      pango_layout_set_indent (layout, 0);
      pango_layout_set_spacing (layout, 0);
      pango_layout_get_pixel_extents (layout, NULL, &layout_rect);

      /* get the center point of the tooltip rectangle */
      if (middle_angle_n < q_angle)
        {
          tip_x = q_width - layout_rect.width/2 - SUBFOLDER_TIP_PADDING * 2;
          tip_y = tan (middle_angle_n) * tip_x;
        }
      else
        {
          tip_y = q_height - layout_rect.height/2 - SUBFOLDER_TIP_PADDING * 2;
          tip_x = tip_y / tan (middle_angle_n);
        }

      /* get the tooltip rectangle */
      tooltip_rect.x = q_width + tip_x - layout_rect.width/2 - SUBFOLDER_TIP_PADDING;
      tooltip_rect.y = q_height + tip_y - layout_rect.height/2 - SUBFOLDER_TIP_PADDING;
      tooltip_rect.width = layout_rect.width + SUBFOLDER_TIP_PADDING * 2;
      tooltip_rect.height = layout_rect.height + SUBFOLDER_TIP_PADDING * 2;

      /* Check tooltip's width is not greater than half of the widget */
      if (tooltip_rect.width > q_width)
        {
          g_object_unref (layout);
          node = node->next;
          continue;
        }

      /* translate tooltip rectangle and edge angles to the original quadrant */
      a = middle_angle;
      i = 0;
      while (a > M_PI/2)
        {
          if (i % 2 == 0)
            tooltip_rect.x = allocation.width - tooltip_rect.x
                             - tooltip_rect.width;
          else
            tooltip_rect.y = allocation.height - tooltip_rect.y
                             - tooltip_rect.height;

          i++;
          a -= M_PI/2;
        }

      /* get the GdkRectangle of the tooltip (with a little padding) */
      _rect.x = tooltip_rect.x - 1;
      _rect.y = tooltip_rect.y - 1;
      _rect.width = tooltip_rect.width + 2;
      _rect.height = tooltip_rect.height + 2;

      /* Check if tooltip overlaps */
      if (! gdk_rectangle_intersect (&_rect, &last_rect, NULL))
        {
          g_memmove (&last_rect, &_rect, sizeof (GdkRectangle));

          /* Finally draw the tooltip to cairo! */

          /* TODO: Do not hardcode colors */

          /* avoid blurred lines */
          tooltip_rect.x = floor (tooltip_rect.x) + 0.5;
          tooltip_rect.y = floor (tooltip_rect.y) + 0.5;

          middle_radius = data->min_radius + (data->max_radius - data->min_radius) / 2;
          sector_center_x = q_width + middle_radius * cos (middle_angle);
          sector_center_y = q_height + middle_radius * sin (middle_angle);

          /* draw line from sector center to tooltip center */
          cairo_set_line_width (cr, 1);
          cairo_move_to (cr, sector_center_x, sector_center_y);
          cairo_set_source_rgb (cr, 0.8275, 0.8431, 0.8118); /* tango: #d3d7cf */
          cairo_line_to (cr, tooltip_rect.x + tooltip_rect.width / 2,
                             tooltip_rect.y + tooltip_rect.height / 2);
          cairo_stroke (cr);

          /* draw a tiny circle in sector center */
          cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.6666 );
          cairo_arc (cr, sector_center_x, sector_center_y, 1.0, 0, 2 * G_PI );
          cairo_stroke (cr);

          /* draw tooltip box */
          cairo_set_line_width (cr, 0.5);
          cairo_rectangle (cr, tooltip_rect.x, tooltip_rect.y,
                               tooltip_rect.width, tooltip_rect.height);
          cairo_set_source_rgb (cr, 0.8275, 0.8431, 0.8118);  /* tango: #d3d7cf */
          cairo_fill_preserve(cr);
          cairo_set_source_rgb (cr, 0.5333, 0.5412, 0.5216);  /* tango: #888a85 */
          cairo_stroke (cr);

          /* draw the text inside the box */
          cairo_set_source_rgb (cr, 0, 0, 0);
          cairo_move_to (cr, tooltip_rect.x + SUBFOLDER_TIP_PADDING,
                             tooltip_rect.y + SUBFOLDER_TIP_PADDING);
          pango_cairo_show_layout (cr, layout);
          cairo_set_line_width (cr, 1);
          cairo_stroke (cr);
        }

      /* Free stuff */
      g_object_unref (layout);

      node = node->next;
    }

  cairo_restore (cr);
}

static void
baobab_ringschart_pre_draw (GtkWidget *chart, cairo_t *cr)
{
  BaobabRingschartPrivate *priv;
  BaobabChartItem *hl_item;

  priv = BAOBAB_RINGSCHART_GET_PRIVATE (chart);

  hl_item = baobab_chart_get_highlighted_item (chart);

  if ( (hl_item == NULL) || (! hl_item->has_visible_children) )
    {
      baobab_ringschart_clean_subforlder_tips_state (chart);

      return;
    }

  if (hl_item != priv->highlighted_item)
    {
      baobab_ringschart_clean_subforlder_tips_state (chart);

      priv->highlighted_item = hl_item;

      /* Launch timer to show subfolder tooltips */
      priv->tips_timeout_event = g_timeout_add (priv->subtip_timeout,
                        (GSourceFunc) baobab_ringschart_subfolder_tips_timeout,
                        (gpointer) chart);
    }
}

static void
baobab_ringschart_post_draw (GtkWidget *chart, cairo_t *cr)
{
  BaobabRingschartPrivate *priv;

  priv = BAOBAB_RINGSCHART_GET_PRIVATE (chart);

  if (priv->drawing_subtips)
    {
      /* Reverse the glist, which was created from the tail */
      priv->subtip_items = g_list_reverse (priv->subtip_items);

      baobab_ringschart_draw_subfolder_tips (chart, cr);
    }
}

/* Public functions start here */

/**
 * baobab_ringschart_new:
 *
 * Constructor for the baobab_ringschart class
 *
 * Returns: a new #BaobabRingschart object
 *
 **/
GtkWidget *
baobab_ringschart_new (void)
{
  return g_object_new (BAOBAB_RINGSCHART_TYPE, NULL);
}

/**
 * baobab_ringschart_set_subfoldertips_enabled:
 * @chart: the #BaobabRingschart to set the
 * @enabled: boolean to determine whether to show sub-folder tips.
 *
 * Enables/disables drawing tooltips of sub-forlders of the highlighted sector.
 *
 * Fails if @chart is not a #BaobabRingschart.
 *
 **/
void
baobab_ringschart_set_subfoldertips_enabled (GtkWidget *chart, gboolean enabled)
{
  BaobabRingschartPrivate *priv;

  priv = BAOBAB_RINGSCHART_GET_PRIVATE (chart);

  priv->subfoldertips_enabled = enabled;

  if ( (! enabled) && (priv->drawing_subtips) )
    {
      /* Turn off currently drawn tips */
      baobab_ringschart_clean_subforlder_tips_state (chart);
    }
}
