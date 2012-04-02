/*
 * baobab-chart.c
 *
 * Copyright (C) 2006, 2007, 2008 Igalia
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "baobab-chart.h"

#define SNAPSHOT_DEF_FILENAME_FORMAT "%s-disk-usage"

G_DEFINE_ABSTRACT_TYPE (BaobabChart, baobab_chart, GTK_TYPE_WIDGET);

#define BAOBAB_CHART_MAX_DEPTH 8
#define BAOBAB_CHART_MIN_DEPTH 1

struct _BaobabChartPrivate
{
  guint name_column;
  guint size_column;
  guint info_column;
  guint percentage_column;
  guint valid_column;
  gboolean button_pressed;
  gboolean is_frozen;
  cairo_surface_t *memento;

  guint max_depth;
  gboolean model_changed;

  GtkTreeModel *model;
  GtkTreeRowReference *root;

  GList *first_item;
  GList *last_item;
  GList *highlighted_item;

  GtkWidget *popup_menu;
};

/* Signals */
enum
{
  ITEM_ACTIVATED,
  LAST_SIGNAL
};

static guint baobab_chart_signals [LAST_SIGNAL] = { 0 };

/* Properties */
enum
{
  PROP_0,
  PROP_MAX_DEPTH,
  PROP_MODEL,
  PROP_ROOT,
};

/* Colors */
const BaobabChartColor baobab_chart_tango_colors[] = {{0.94, 0.16, 0.16}, /* tango: ef2929 */
                                                      {0.68, 0.49, 0.66}, /* tango: ad7fa8 */
                                                      {0.45, 0.62, 0.82}, /* tango: 729fcf */
                                                      {0.54, 0.89, 0.20}, /* tango: 8ae234 */
                                                      {0.91, 0.73, 0.43}, /* tango: e9b96e */
                                                      {0.99, 0.68, 0.25}}; /* tango: fcaf3e */

static void baobab_chart_class_init (BaobabChartClass *class);
static void baobab_chart_init (BaobabChart *object);
static void baobab_chart_realize (GtkWidget *widget);
static void baobab_chart_unrealize (GtkWidget *widget);
static void baobab_chart_dispose (GObject *object);
static void baobab_chart_size_allocate (GtkWidget *widget,
                                        GtkAllocation *allocation);
static void baobab_chart_set_property (GObject *object,
                                       guint prop_id,
                                       const GValue *value,
                                       GParamSpec *pspec);
static void baobab_chart_get_property (GObject *object,
                                       guint prop_id,
                                       GValue *value,
                                       GParamSpec *pspec);
static void baobab_chart_free_items (BaobabChart *chart);
static void baobab_chart_draw_chart (BaobabChart *chart,
                                     cairo_t *cr);
static void baobab_chart_update_draw (BaobabChart *chart,
                                      GtkTreePath *path);
static gboolean baobab_chart_draw (GtkWidget *chart,
                                   cairo_t *cr);
static void baobab_chart_interpolate_colors (BaobabChartColor *color,
                                             BaobabChartColor colora,
                                             BaobabChartColor colorb,
                                             gdouble percentage);
static gint baobab_chart_button_press_event (GtkWidget      *widget,
                                             GdkEventButton *event);
static gboolean baobab_chart_popup_menu (GtkWidget *widget);
static gint baobab_chart_scroll (GtkWidget *widget,
                                 GdkEventScroll *event);
static gint baobab_chart_motion_notify (GtkWidget *widget,
                                        GdkEventMotion *event);
static gint baobab_chart_leave_notify (GtkWidget *widget,
                                       GdkEventCrossing *event);
static void baobab_chart_disconnect_signals (BaobabChart *chart,
                                             GtkTreeModel *model);
static void baobab_chart_connect_signals (BaobabChart *chart,
                                          GtkTreeModel *model);
static void baobab_chart_get_items (BaobabChart *chart, GtkTreePath *root);
static gboolean baobab_chart_query_tooltip (GtkWidget  *widget,
                                            gint        x,
                                            gint        y,
                                            gboolean    keyboard_mode,
                                            GtkTooltip *tooltip,
                                            gpointer    user_data);
static void baobab_chart_item_activated (BaobabChart *chart,
                                         GtkTreeIter *iter);


static void
baobab_chart_class_init (BaobabChartClass *class)
{
  GObjectClass *obj_class;
  GtkWidgetClass *widget_class;

  obj_class = G_OBJECT_CLASS (class);
  widget_class = GTK_WIDGET_CLASS (class);

  /* GtkObject signals */
  obj_class->set_property = baobab_chart_set_property;
  obj_class->get_property = baobab_chart_get_property;
  obj_class->dispose = baobab_chart_dispose;

  /* GtkWidget signals */
  widget_class->realize = baobab_chart_realize;
  widget_class->unrealize = baobab_chart_unrealize;
  widget_class->draw = baobab_chart_draw;
  widget_class->size_allocate = baobab_chart_size_allocate;
  widget_class->button_press_event = baobab_chart_button_press_event;
  widget_class->popup_menu = baobab_chart_popup_menu;
  widget_class->scroll_event = baobab_chart_scroll;

  /* BaobabChart signals */
  class->item_activated = baobab_chart_item_activated;

  /* BaobabChart abstract methods */
  class->draw_item               = NULL;
  class->pre_draw                = NULL;
  class->post_draw               = NULL;
  class->calculate_item_geometry = NULL;
  class->is_point_over_item      = NULL;
  class->get_item_rectangle      = NULL;
  class->can_zoom_in             = NULL;
  class->can_zoom_out            = NULL;

  g_object_class_install_property (obj_class,
                                   PROP_MAX_DEPTH,
                                   g_param_spec_int ("max-depth",
                                   _("Maximum depth"),
                                   _("The maximum depth drawn in the chart from the root"),
                                   1,
                                   BAOBAB_CHART_MAX_DEPTH,
                                   BAOBAB_CHART_MAX_DEPTH,
                                   G_PARAM_READWRITE));

  g_object_class_install_property (obj_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("model",
                                   _("Chart model"),
                                   _("Set the model of the chart"),
                                   GTK_TYPE_TREE_MODEL,
                                   G_PARAM_READWRITE));

  g_object_class_install_property (obj_class,
                                   PROP_ROOT,
                                   g_param_spec_boxed ("root",
                                   _("Chart root node"),
                                   _("Set the root node from the model"),
                                   GTK_TYPE_TREE_ITER,
                                   G_PARAM_READWRITE));

  baobab_chart_signals[ITEM_ACTIVATED] =
    g_signal_new ("item-activated",
          G_TYPE_FROM_CLASS (obj_class),
          G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
          G_STRUCT_OFFSET (BaobabChartClass, item_activated),
          NULL, NULL,
          g_cclosure_marshal_VOID__BOXED,
          G_TYPE_NONE, 1,
          GTK_TYPE_TREE_ITER);

  g_type_class_add_private (obj_class, sizeof (BaobabChartPrivate));
}

static void
baobab_chart_init (BaobabChart *chart)
{
  chart->priv = G_TYPE_INSTANCE_GET_PRIVATE (chart, BAOBAB_TYPE_CHART, BaobabChartPrivate);

  chart->priv->model = NULL;
  chart->priv->max_depth = BAOBAB_CHART_MAX_DEPTH;
  chart->priv->name_column = 0;
  chart->priv->size_column = 0;
  chart->priv->info_column = 0;
  chart->priv->percentage_column = 0;
  chart->priv->valid_column = 0;
  chart->priv->button_pressed = FALSE;
  chart->priv->is_frozen = FALSE;
  chart->priv->memento = NULL;
  chart->priv->root = NULL;

  chart->priv->first_item = NULL;
  chart->priv->last_item = NULL;
  chart->priv->highlighted_item = NULL;
}

static void
baobab_chart_dispose (GObject *object)
{
  BaobabChart *chart;

  chart = BAOBAB_CHART (object);

  baobab_chart_free_items (chart);

  if (chart->priv->model)
    {
      baobab_chart_disconnect_signals (chart, chart->priv->model);
      g_object_unref (chart->priv->model);
      chart->priv->model = NULL;
    }

  if (chart->priv->root)
    {
      gtk_tree_row_reference_free (chart->priv->root);
      chart->priv->root = NULL;
    }

  G_OBJECT_CLASS (baobab_chart_parent_class)->dispose (object);
}

static void
baobab_chart_realize (GtkWidget *widget)
{
  BaobabChart *chart;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GtkAllocation allocation;
  GdkWindow *window;
  GtkStyleContext *context;

  g_return_if_fail (BAOBAB_IS_CHART (widget));

  chart = BAOBAB_CHART (widget);
  gtk_widget_set_realized (widget, TRUE);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.event_mask = gtk_widget_get_events (widget);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  window = gdk_window_new (gtk_widget_get_parent_window (widget),
                           &attributes,
                           attributes_mask);
  gtk_widget_set_window (widget, window);
  gdk_window_set_user_data (window, chart);

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_set_background (context, window);

  gtk_widget_add_events (widget,
                         GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK |
                         GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK |
                         GDK_POINTER_MOTION_HINT_MASK | GDK_LEAVE_NOTIFY_MASK |
                         GDK_SCROLL_MASK);
}

static void
baobab_chart_unrealize (GtkWidget *widget)
{
  BaobabChart *chart;

  chart = BAOBAB_CHART (widget);

  if (chart->priv->popup_menu)
    {
      gtk_widget_destroy (chart->priv->popup_menu);
      chart->priv->popup_menu = NULL;
    }

  GTK_WIDGET_CLASS (baobab_chart_parent_class)->unrealize (widget);
}

static void
baobab_chart_size_allocate (GtkWidget     *widget,
                            GtkAllocation *allocation)
{
  BaobabChart *chart;
  BaobabChartClass *class;
  BaobabChartItem *item;
  GList *node;

  g_return_if_fail (BAOBAB_IS_CHART (widget));
  g_return_if_fail (allocation != NULL);

  chart = BAOBAB_CHART (widget);
  class = BAOBAB_CHART_GET_CLASS (chart);

  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (gtk_widget_get_window (widget),
                  allocation->x, allocation->y,
                  allocation->width, allocation->height);

      for (node = chart->priv->first_item; node != NULL; node = node->next)
        {
          item = (BaobabChartItem *) node->data;
          item->has_visible_children = FALSE;
          item->visible = FALSE;
          class->calculate_item_geometry (chart, item);
        }
    }
}

static void
baobab_chart_set_property (GObject         *object,
                           guint            prop_id,
                           const GValue    *value,
                           GParamSpec      *pspec)
{
  BaobabChart *chart = BAOBAB_CHART (object);

  switch (prop_id)
    {
    case PROP_MAX_DEPTH:
      baobab_chart_set_max_depth (chart, g_value_get_int (value));
      break;
    case PROP_MODEL:
      baobab_chart_set_model (chart, g_value_get_object (value));
      break;
    case PROP_ROOT:
      baobab_chart_set_root (chart, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
baobab_chart_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  BaobabChart *chart = BAOBAB_CHART (object);

  switch (prop_id)
    {
    case PROP_MAX_DEPTH:
      g_value_set_int (value, chart->priv->max_depth);
      break;
    case PROP_MODEL:
      g_value_set_object (value, chart->priv->model);
      break;
    case PROP_ROOT:
      g_value_set_object (value, chart->priv->root);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static GList *
baobab_chart_add_item (BaobabChart *chart,
                       guint        depth,
                       gdouble      rel_start,
                       gdouble      rel_size,
                       GtkTreeIter  iter)
{
  BaobabChartItem *item;
  gchar *name;
  gchar *size;

  gtk_tree_model_get (chart->priv->model, &iter,
                      chart->priv->name_column, &name,
                      chart->priv->size_column, &size,
                      -1);

  item = g_new (BaobabChartItem, 1);
  item->name = name;
  item->size = size;
  item->depth = depth;
  item->rel_start = rel_start;
  item->rel_size = rel_size;
  item->has_any_child = FALSE;
  item->visible = FALSE;
  item->has_visible_children = FALSE;

  item->iter = iter;

  item->parent = NULL;
  item->data = NULL;

  chart->priv->last_item = g_list_prepend (chart->priv->last_item, item);

  return chart->priv->last_item;
}

static void
baobab_chart_free_items (BaobabChart *chart)
{
  BaobabChartItem *item;
  GList *node;
  GList *next;

  node = chart->priv->first_item;

  while (node != NULL)
    {
      next = node->next;

      item = (BaobabChartItem *) node->data;

      g_free (item->name);
      g_free (item->size);

      g_free (item->data);
      item->data = NULL;

      g_free (item);
      g_list_free_1 (node);

      node = next;
    }

  chart->priv->first_item = NULL;
  chart->priv->last_item = NULL;
  chart->priv->highlighted_item = NULL;
}

static void
baobab_chart_get_items (BaobabChart *chart,
                        GtkTreePath *root)
{
  BaobabChartItem *item;
  GList *node;
  GtkTreeIter initial_iter = {0};
  gdouble size;
  GtkTreePath *model_root_path;
  GtkTreeIter model_root_iter;
  BaobabChartClass *class;
  GtkTreeIter child_iter = {0};
  GList *child_node;
  BaobabChartItem *child;
  gdouble rel_start;

  /* First we free current item list */
  baobab_chart_free_items (chart);

  /* Get the tree iteration corresponding to root */
  if (!gtk_tree_model_get_iter (chart->priv->model, &initial_iter, root))
    {
      chart->priv->model_changed = FALSE;
      return;
    }

  model_root_path = gtk_tree_path_new_first ();
  gtk_tree_model_get_iter (chart->priv->model, &model_root_iter, model_root_path);
  gtk_tree_path_free (model_root_path);

  gtk_tree_model_get (chart->priv->model, &model_root_iter,
                      chart->priv->percentage_column, &size, -1);

  /* Create first item */
  node = baobab_chart_add_item (chart, 0, 0, 100, initial_iter);

  /* Iterate through childs building the list */
  class = BAOBAB_CHART_GET_CLASS (chart);

  do
    {
      item = (BaobabChartItem *) node->data;
      item->has_any_child = gtk_tree_model_iter_children (chart->priv->model, &child_iter, &(item->iter));

      /* Calculate item geometry */
      class->calculate_item_geometry (chart, item);

      if (! item->visible)
        {
          node = node->prev;
          continue;
        }

      /* Get item's children and add them to the list */
      if ((item->has_any_child) && (item->depth < chart->priv->max_depth + 1))
        {
          rel_start = 0;

          do
            {
              gtk_tree_model_get (chart->priv->model, &child_iter, chart->priv->percentage_column, &size, -1);

              child_node = baobab_chart_add_item (chart,
                                                  item->depth + 1,
                                                  rel_start,
                                                  size,
                                                  child_iter);
              child = (BaobabChartItem *) child_node->data;
              child->parent = node;
              rel_start += size;
            }
          while (gtk_tree_model_iter_next (chart->priv->model, &child_iter));
        }

      node = node->prev;
    }
  while (node != NULL);

  /* Reverse the list, 'cause we created it from the tail, for efficiency reasons */
  chart->priv->first_item = g_list_reverse (chart->priv->last_item);

  chart->priv->model_changed = FALSE;
}

static void
baobab_chart_draw_chart (BaobabChart *chart,
                         cairo_t     *cr)
{
  BaobabChartClass *class;
  GList *node;

  class = BAOBAB_CHART_GET_CLASS (chart);

  /* call pre-draw abstract method */
  if (class->pre_draw)
    class->pre_draw (chart, cr);

  cairo_save (cr);

  for (node = chart->priv->first_item; node != NULL; node = node->next)
    {
      BaobabChartItem *item;
      GdkRectangle clip;

      item = (BaobabChartItem *) node->data;

      if (gdk_cairo_get_clip_rectangle (cr, &clip) &&
          (item->visible) &&
          (gdk_rectangle_intersect (&clip, &item->rect, NULL)) &&
          (item->depth <= chart->priv->max_depth))
        {
          gboolean highlighted;

          highlighted = (node == chart->priv->highlighted_item);
          class->draw_item (chart, cr, item, highlighted);
        }
    }

  cairo_restore (cr);

  /* call post-draw abstract method */
  if (class->post_draw)
    class->post_draw (chart, cr);
}

static void
baobab_chart_update_draw (BaobabChart* chart,
                          GtkTreePath *path)
{
  GtkTreePath *root_path = NULL;
  gint root_depth, node_depth;

  if (!gtk_widget_get_realized ( GTK_WIDGET (chart)))
    return;

  if (chart->priv->root != NULL)
    {
      root_path = gtk_tree_row_reference_get_path (chart->priv->root);

      if (root_path == NULL)
        {
          gtk_tree_row_reference_free (chart->priv->root);
          chart->priv->root = NULL;
        }
    }

  if (chart->priv->root == NULL)
    root_path = gtk_tree_path_new_first ();


  root_depth = gtk_tree_path_get_depth (root_path);
  node_depth = gtk_tree_path_get_depth (path);

  if (((node_depth-root_depth)<=chart->priv->max_depth)&&
      ((gtk_tree_path_is_ancestor (root_path, path))||
       (gtk_tree_path_compare (root_path, path) == 0)))
    {
      gtk_widget_queue_draw (GTK_WIDGET (chart));
    }

  gtk_tree_path_free (root_path);
}

static void
baobab_chart_row_changed (GtkTreeModel *model,
                          GtkTreePath  *path,
                          GtkTreeIter  *iter,
                          BaobabChart  *chart)
{
  g_return_if_fail (path != NULL || iter != NULL);

  chart->priv->model_changed = TRUE;
  baobab_chart_update_draw (chart, path);
}

static void
baobab_chart_row_inserted (GtkTreeModel *model,
                           GtkTreePath  *path,
                           GtkTreeIter  *iter,
                           BaobabChart  *chart)
{
  g_return_if_fail (path != NULL || iter != NULL);

  chart->priv->model_changed = TRUE;
  baobab_chart_update_draw (chart, path);
}

static void
baobab_chart_row_has_child_toggled (GtkTreeModel *model,
                                    GtkTreePath  *path,
                                    GtkTreeIter  *iter,
                                    BaobabChart  *chart)
{
  g_return_if_fail (path != NULL || iter != NULL);

  chart->priv->model_changed = TRUE;
  baobab_chart_update_draw (chart, path);
}

static void
baobab_chart_row_deleted (GtkTreeModel *model,
                          GtkTreePath  *path,
                          BaobabChart  *chart)
{
  g_return_if_fail (path != NULL);

  chart->priv->model_changed = TRUE;
  baobab_chart_update_draw (chart, path);
}

static void
baobab_chart_rows_reordered (GtkTreeModel *model,
                             GtkTreePath  *path,
                             GtkTreeIter  *iter,
                             gint         *new_order,
                             BaobabChart  *chart)
{
  g_return_if_fail (path != NULL || iter != NULL);

  chart->priv->model_changed = TRUE;
  baobab_chart_update_draw (chart, path);
}

static gboolean
baobab_chart_draw (GtkWidget *widget,
                   cairo_t   *cr)
{
  BaobabChart *chart;

  chart = BAOBAB_CHART (widget);

  /* the columns are not set we paint nothing */
  if (chart->priv->name_column == chart->priv->percentage_column)
    return FALSE;

  /* there is no model we can not paint */
  if ((chart->priv->is_frozen) || (chart->priv->model == NULL))
    {
      if (chart->priv->memento != NULL)
        {
          gint w, h, aw, ah;
          gdouble p, sx, sy;

          w = cairo_image_surface_get_width (chart->priv->memento);
          h = cairo_image_surface_get_height (chart->priv->memento);

          aw = gtk_widget_get_allocated_width (widget);
          ah = gtk_widget_get_allocated_height (widget);

          if (w > 0 && h > 0 && !(aw == w && aw == h))
            {
              /* minimal available proportion */
              p = MIN (aw / (1.0 * w), ah / (1.0 * h));

              sx = (gdouble) (aw - w * p) / 2.0;
              sy = (gdouble) (ah - h * p) / 2.0;

              cairo_translate (cr, sx, sy);
              cairo_scale (cr, p, p);
            }

          cairo_set_source_surface (cr, chart->priv->memento, 0, 0);
          cairo_paint (cr);
        }
    }
  else
    {
      GtkTreePath *root_path = NULL;

      cairo_set_source_rgb (cr, 1, 1, 1);
      cairo_fill_preserve (cr);

      if (chart->priv->root != NULL)
        root_path = gtk_tree_row_reference_get_path (chart->priv->root);

      if (root_path == NULL)
        {
          root_path = gtk_tree_path_new_first ();
          chart->priv->root = NULL;
        }

      /* Check if tree model was modified in any way */
      if ((chart->priv->model_changed) || (chart->priv->first_item == NULL))
        {
          baobab_chart_get_items (chart, root_path);
        }
      else
        {
          GtkTreePath *current_path;

          /* Check if root was changed */
          current_path = gtk_tree_model_get_path (chart->priv->model,
                         &((BaobabChartItem*) chart->priv->first_item->data)->iter);

          if (gtk_tree_path_compare (root_path, current_path) != 0)
            baobab_chart_get_items (chart, root_path);

          gtk_tree_path_free (current_path);
        }

      gtk_tree_path_free (root_path);

      baobab_chart_draw_chart (chart, cr);
    }

  return FALSE;
}

static void
baobab_chart_interpolate_colors (BaobabChartColor *color,
                                 BaobabChartColor colora,
                                 BaobabChartColor colorb,
                                 gdouble percentage)
{
  gdouble diff;

  diff = colora.red - colorb.red;
  color->red = colora.red-diff*percentage;

  diff = colora.green - colorb.green;
  color->green = colora.green-diff*percentage;

  diff = colora.blue - colorb.blue;
  color->blue = colora.blue-diff*percentage;
}

void
baobab_chart_get_item_color (BaobabChartColor *color,
                             gdouble rel_position,
                             gint depth,
                             gboolean highlighted)
{
  gdouble intensity;
  gint color_number;
  gint next_color_number;
  gdouble maximum;
  static const BaobabChartColor level_color = {0.83, 0.84, 0.82};
  static const BaobabChartColor level_color_hl = {0.88, 0.89, 0.87};

  intensity = 1 - (((depth-1)*0.3) / BAOBAB_CHART_MAX_DEPTH);

  if (depth == 0)
    *color = level_color;
  else
    {
      color_number = rel_position / (100/3);
      next_color_number = (color_number + 1) % 6;

      baobab_chart_interpolate_colors (color,
                                       baobab_chart_tango_colors[color_number],
                                       baobab_chart_tango_colors[next_color_number],
                                       (rel_position - color_number * 100/3) / (100/3));
      color->red = color->red * intensity;
      color->green = color->green * intensity;
      color->blue = color->blue * intensity;
    }

  if (highlighted)
    {
      if (depth == 0)
        *color = level_color_hl;
      else
        {
          maximum = MAX (color->red,
                         MAX (color->green,
                              color->blue));
          color->red /= maximum;
          color->green /= maximum;
          color->blue /= maximum;
        }
    }
}

static void
popup_menu_detach (GtkWidget *attach_widget,
                   GtkMenu   *menu)
{
  BAOBAB_CHART (attach_widget)->priv->popup_menu = NULL;
}

static void
popup_menu_activate_up (GtkMenuItem *checkmenuitem,
                        BaobabChart *chart)
{
  baobab_chart_move_up_root (chart);
}

static void
popup_menu_activate_zoom_in (GtkMenuItem *checkmenuitem,
                             BaobabChart *chart)
{
  baobab_chart_zoom_in (chart);
}

static void
popup_menu_activate_zoom_out (GtkMenuItem *checkmenuitem,
                              BaobabChart *chart)
{
  baobab_chart_zoom_out (chart);
}

static void
popup_menu_activate_snapshot (GtkMenuItem *checkmenuitem,
                              BaobabChart *chart)
{
  baobab_chart_save_snapshot (chart);
}

static void
do_popup_menu (BaobabChart    *chart,
               GdkEventButton *event)
{
  GtkWidget *menu;
  GtkWidget *up_item;
  GtkWidget *zoom_in_item;
  GtkWidget *zoom_out_item;
  GtkWidget *snapshot_item;
  GtkTreePath *root_path;

  menu = gtk_menu_new ();
  chart->priv->popup_menu = menu;

  gtk_menu_attach_to_widget (GTK_MENU (menu), GTK_WIDGET (chart), popup_menu_detach);

  up_item = gtk_image_menu_item_new_with_label (_("Move to parent folder"));
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (up_item),
                                 gtk_image_new_from_stock(GTK_STOCK_GO_UP, GTK_ICON_SIZE_MENU));

  zoom_in_item = gtk_image_menu_item_new_with_label (_("Zoom in")) ;
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (zoom_in_item),
                                 gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_MENU));

  zoom_out_item = gtk_image_menu_item_new_with_label (_("Zoom out"));
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (zoom_out_item),
                                 gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU));

  snapshot_item = gtk_image_menu_item_new_with_label (_("Save screenshot"));
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (snapshot_item),
                                 gtk_image_new_from_file (BAOBAB_PIX_DIR "shot.png"));

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), up_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), zoom_in_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), zoom_out_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), snapshot_item);

  g_signal_connect (up_item, "activate",
                    G_CALLBACK (popup_menu_activate_up), chart);
  g_signal_connect (zoom_in_item, "activate",
                    G_CALLBACK (popup_menu_activate_zoom_in), chart);
  g_signal_connect (zoom_out_item, "activate",
                    G_CALLBACK (popup_menu_activate_zoom_out), chart);
  g_signal_connect (snapshot_item, "activate",
                    G_CALLBACK (popup_menu_activate_snapshot), chart);

  gtk_widget_show_all (menu);

  root_path = baobab_chart_get_root (chart);

  gtk_widget_set_sensitive (up_item,
                            (!chart->priv->is_frozen) &&
                            ((root_path != NULL) &&
                            (gtk_tree_path_get_depth (root_path) > 1)));
  gtk_widget_set_sensitive (zoom_in_item,
                            baobab_chart_can_zoom_in (chart));
  gtk_widget_set_sensitive (zoom_out_item,
                            baobab_chart_can_zoom_out (chart));
  gtk_widget_set_sensitive (snapshot_item,
                            (!chart->priv->is_frozen));

  gtk_menu_popup (GTK_MENU (menu),
                  NULL, NULL, NULL, NULL,
                  event ? event->button : 0,
                  event ? event->time : gtk_get_current_event_time ());

  gtk_tree_path_free (root_path);
}

static gint
baobab_chart_button_press_event (GtkWidget      *widget,
                                 GdkEventButton *event)
{
  BaobabChart *chart = BAOBAB_CHART (widget);

  if (event->type == GDK_BUTTON_PRESS)
    {
      if (gdk_event_triggers_context_menu ((GdkEvent *) event))
        {
          do_popup_menu (chart, event);
          return TRUE;
        }
      else if (!chart->priv->is_frozen)
        {
          switch (event->button)
            {
              case 1:
                if (chart->priv->highlighted_item != NULL)
                  {
                    GtkTreePath *path, *root_path;
                    GtkTreeIter *iter;

                    iter = &((BaobabChartItem*) chart->priv->highlighted_item->data)->iter;
                    path = gtk_tree_model_get_path (chart->priv->model, iter);

                    if (chart->priv->root)
                      root_path = gtk_tree_row_reference_get_path (chart->priv->root);
                    else
                      root_path = gtk_tree_path_new_first ();

                    if (gtk_tree_path_compare (path, root_path) == 0)
                      {
                        /* Go back to the parent dir */
                        baobab_chart_move_up_root (chart);
                      }
                    else
                      {
                        /* Enter into a subdir */
                        g_signal_emit (chart,
                                       baobab_chart_signals[ITEM_ACTIVATED], 0,
                                       iter);
                      }

                    gtk_tree_path_free (path);
                  }

                break;

              case 2:
                /* Go back to the parent dir */
                baobab_chart_move_up_root (chart);
                break;
            }

          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
baobab_chart_popup_menu (GtkWidget *widget)
{
  do_popup_menu (BAOBAB_CHART (widget), NULL);

  return TRUE;
}

static gint
baobab_chart_scroll (GtkWidget *widget,
                     GdkEventScroll *event)
{
  BaobabChart *chart = BAOBAB_CHART (widget);

  switch (event->direction)
    {
    case GDK_SCROLL_LEFT :
    case GDK_SCROLL_UP :
      if (baobab_chart_can_zoom_out (chart))
        baobab_chart_zoom_out (chart);
      /* change the selected item when zooming */
      baobab_chart_motion_notify (widget, (GdkEventMotion *)event);
      break;

    case GDK_SCROLL_RIGHT :
    case GDK_SCROLL_DOWN :
      if (baobab_chart_can_zoom_in (chart))
        baobab_chart_zoom_in (chart);
      break;

    case GDK_SCROLL_SMOOTH :
      /* since we don't add GDK_SMOOTH_SCROLL_MASK to received
         events, this is actually never reached and it's here
         just to silence compiler warnings */
      break;
    }

  return FALSE;
}

static void
baobab_chart_set_item_highlight (BaobabChart *chart,
                                 GList       *node,
                                 gboolean     highlighted)
{
  BaobabChartItem *item;

  if (node == NULL)
    return;

  item = (BaobabChartItem *) node->data;

  if (highlighted)
    chart->priv->highlighted_item = node;
  else
    chart->priv->highlighted_item = NULL;

  gdk_window_invalidate_rect (gtk_widget_get_window (GTK_WIDGET (chart)),
                              &item->rect, TRUE);
}

static gint
baobab_chart_motion_notify (GtkWidget      *widget,
                            GdkEventMotion *event)
{
  BaobabChart *chart;
  BaobabChartPrivate *priv;
  BaobabChartClass *class;
  GList *node;
  BaobabChartItem *item;
  gboolean found = FALSE;

  chart = BAOBAB_CHART (widget);
  priv = chart->priv;

  class = BAOBAB_CHART_GET_CLASS (widget);

  /* Check if the pointer is over an item */
  for (node = priv->last_item; node != NULL; node = node->prev)
    {
      item = (BaobabChartItem *) node->data;

      if ((item->visible) && (class->is_point_over_item (chart, item, event->x, event->y)))
        {
          if (chart->priv->highlighted_item != node)
            {
              baobab_chart_set_item_highlight (chart, priv->highlighted_item, FALSE);

              gtk_widget_set_has_tooltip (widget, TRUE);
              baobab_chart_set_item_highlight (chart, node, TRUE);
            }

          found = TRUE;
          break;
        }
    }

  /* If we never found a highlighted item, but there is an old highlighted item,
     redraw it to turn it off */
  if (! found)
    {
      baobab_chart_set_item_highlight (chart, priv->highlighted_item, FALSE);
      gtk_widget_set_has_tooltip (widget, FALSE);
    }

  /* Continue receiving motion notifies */
  gdk_event_request_motions (event);

  return FALSE;
}

static gint
baobab_chart_leave_notify (GtkWidget        *widget,
                           GdkEventCrossing *event)
{
  BaobabChart *chart;

  chart = BAOBAB_CHART (widget);

  baobab_chart_set_item_highlight (chart, chart->priv->highlighted_item, FALSE);

  return FALSE;
}

static void
baobab_chart_connect_signals (BaobabChart *chart,
                              GtkTreeModel *model)
{
  g_signal_connect (model,
                    "row_changed",
                    G_CALLBACK (baobab_chart_row_changed),
                    chart);
  g_signal_connect (model,
                    "row_inserted",
                    G_CALLBACK (baobab_chart_row_inserted),
                    chart);
  g_signal_connect (model,
                    "row_has_child_toggled",
                    G_CALLBACK (baobab_chart_row_has_child_toggled),
                    chart);
  g_signal_connect (model,
                    "row_deleted",
                    G_CALLBACK (baobab_chart_row_deleted),
                    chart);
  g_signal_connect (model,
                    "rows_reordered",
                    G_CALLBACK (baobab_chart_rows_reordered),
                    chart);
  g_signal_connect (chart,
                    "query-tooltip",
                    G_CALLBACK (baobab_chart_query_tooltip),
                    chart);
  g_signal_connect (chart,
                    "motion-notify-event",
                    G_CALLBACK (baobab_chart_motion_notify),
                    chart);
  g_signal_connect (chart,
                    "leave-notify-event",
                    G_CALLBACK (baobab_chart_leave_notify),
                    chart);
}

static void
baobab_chart_disconnect_signals (BaobabChart  *chart,
                                 GtkTreeModel *model)
{
  g_signal_handlers_disconnect_by_func (model,
                                        baobab_chart_row_changed,
                                        chart);
  g_signal_handlers_disconnect_by_func (model,
                                        baobab_chart_row_inserted,
                                        chart);
  g_signal_handlers_disconnect_by_func (model,
                                        baobab_chart_row_has_child_toggled,
                                        chart);
  g_signal_handlers_disconnect_by_func (model,
                                        baobab_chart_row_deleted,
                                        chart);
  g_signal_handlers_disconnect_by_func (model,
                                        baobab_chart_rows_reordered,
                                        chart);
  g_signal_handlers_disconnect_by_func (chart,
                                        baobab_chart_query_tooltip,
                                        chart);
  g_signal_handlers_disconnect_by_func (chart,
                                        baobab_chart_motion_notify,
                                        chart);
  g_signal_handlers_disconnect_by_func (chart,
                                        baobab_chart_leave_notify,
                                        chart);
}

static gboolean
baobab_chart_query_tooltip (GtkWidget  *widget,
                            gint        x,
                            gint        y,
                            gboolean    keyboard_mode,
                            GtkTooltip *tooltip,
                            gpointer    user_data)
{
  BaobabChart *chart = BAOBAB_CHART (widget);
  BaobabChartItem *item;
  char *markup;

  if (chart->priv->highlighted_item == NULL)
    return FALSE;

  item = (BaobabChartItem *) chart->priv->highlighted_item->data;

  if ( (item->name == NULL) || (item->size == NULL) )
    return FALSE;

  gtk_tooltip_set_tip_area (tooltip, &item->rect);

  markup = g_strconcat (item->name,
                        "\n",
                        item->size,
                        NULL);
  gtk_tooltip_set_markup (tooltip, markup);
  g_free (markup);

  return TRUE;
}

static void
baobab_chart_item_activated (BaobabChart *chart,
                             GtkTreeIter *iter)
{
  GtkTreePath *path;

  path = gtk_tree_model_get_path (chart->priv->model, iter);
  baobab_chart_set_root (chart, path);

  gtk_tree_path_free (path);
}

static GdkPixbuf *
baobab_chart_get_pixbuf (BaobabChart *chart)
{
  gint w, h;
  GdkPixbuf *pixbuf;

  g_return_val_if_fail (BAOBAB_IS_CHART (chart), NULL);

  w = gtk_widget_get_allocated_width (GTK_WIDGET (chart));
  h = gtk_widget_get_allocated_height (GTK_WIDGET (chart));
  pixbuf = gdk_pixbuf_get_from_window (gtk_widget_get_window (GTK_WIDGET (chart)), 0, 0, w, h);

  return pixbuf;
}

/* Public functions start here */

/**
 * baobab_chart_new:
 *
 * Constructor for the baobab_chart class
 *
 * Returns: a new #BaobabChart object
 **/
BaobabChart *
baobab_chart_new (void)
{
  return g_object_new (BAOBAB_TYPE_CHART, NULL);
}

/**
 * baobab_chart_set_model_with_columns:
 * @chart: the #BaobabChart whose model is going to be set
 * @model: the #GtkTreeModel which is going to set as the model of
 * @chart
 * @name_column: number of column inside @model where the file name is
 * stored
 * @size_column: number of column inside @model where the file size is
 * stored
 * @info_column: number of column inside @model where the percentage
 * of disk usage is stored
 * @percentage_column: number of column inside @model where the disk
 * usage percentage is stored
 * @valid_column: number of column inside @model where the flag indicating
 * if the row data is right or not.
 * @root: a #GtkTreePath indicating the node of @model which will be
 * used as root.
 *
 * Sets @model as the #GtkTreeModel used by @chart. Indicates the
 * columns inside @model where the values file name, file
 * size, file information, disk usage percentage and data correction are stored, and
 * the node which will be used as the root of @chart.  Once
 * the model has been successfully set, a redraw of the window is
 * forced.
 * This function is intended to be used the first time a #GtkTreeModel
 * is assigned to @chart, or when the columns containing the needed data
 * are going to change. In other cases, #baobab_chart_set_model should
 * be used.
 * This function does not change the state of the signals from the model, which
 * is controlled by he #baobab_chart_freeze_updates and the
 * #baobab_chart_thaw_updates functions.
 *
 * Fails if @chart is not a #BaobabChart or if @model is not a
 * #GtkTreeModel.
 **/
void
baobab_chart_set_model_with_columns (BaobabChart  *chart,
                                     GtkTreeModel *model,
                                     guint         name_column,
                                     guint         size_column,
                                     guint         info_column,
                                     guint         percentage_column,
                                     guint         valid_column,
                                     GtkTreePath  *root)
{
  g_return_if_fail (BAOBAB_IS_CHART (chart));
  g_return_if_fail (GTK_IS_TREE_MODEL (model));


  baobab_chart_set_model (chart, model);

  if (root != NULL)
    {
      chart->priv->root = gtk_tree_row_reference_new (model, root);
      g_object_notify (G_OBJECT (chart), "root");
    }

  chart->priv->name_column = name_column;
  chart->priv->size_column = size_column;
  chart->priv->info_column = info_column;
  chart->priv->percentage_column = percentage_column;
  chart->priv->valid_column = valid_column;
}

/**
 * baobab_chart_set_model:
 * @chart: the #BaobabChart whose model is going to be set
 * @model: the #GtkTreeModel which is going to set as the model of
 * @chart
 *
 * Sets @model as the #GtkTreeModel used by @chart, and takes the needed
 * data from the columns especified in the last call to
 * #baobab_chart_set_model_with_colums.
 * This function does not change the state of the signals from the model, which
 * is controlled by he #baobab_chart_freeze_updates and the
 * #baobab_chart_thaw_updates functions.
 *
 * Fails if @chart is not a #BaobabChart or if @model is not a
 * #GtkTreeModel.
 **/
void
baobab_chart_set_model (BaobabChart  *chart,
                        GtkTreeModel *model)
{
  g_return_if_fail (BAOBAB_IS_CHART (chart));
  g_return_if_fail (GTK_IS_TREE_MODEL (model));

  if (model == chart->priv->model)
    return;

  if (chart->priv->model)
    {
      if (! chart->priv->is_frozen)
        baobab_chart_disconnect_signals (chart, chart->priv->model);
      g_object_unref (chart->priv->model);
    }

  chart->priv->model = model;
  g_object_ref (chart->priv->model);

  if (! chart->priv->is_frozen)
    baobab_chart_connect_signals (chart, chart->priv->model);

  if (chart->priv->root)
    gtk_tree_row_reference_free (chart->priv->root);

  chart->priv->root = NULL;

  g_object_notify (G_OBJECT (chart), "model");

  gtk_widget_queue_draw (GTK_WIDGET (chart));
}

/**
 * baobab_chart_get_model:
 * @chart: a #BaobabChart whose model will be returned.
 *
 * Returns the #GtkTreeModel which is the model used by @chart.
 *
 * Returns: %NULL if @chart is not a #BaobabChart.
 **/
GtkTreeModel *
baobab_chart_get_model (BaobabChart *chart)
{
  g_return_val_if_fail (BAOBAB_IS_CHART (chart), NULL);

  return chart->priv->model;
}

/**
 * baobab_chart_set_max_depth:
 * @chart: a #BaobabChart
 * @max_depth: the new maximum depth to show in the widget.
 *
 * Sets the maximum number of nested levels that are going to be show in the
 * wigdet, and causes a redraw of the widget to show the new maximum
 * depth. If max_depth is < 1 MAX_DRAWABLE_DEPTH is used.
 *
 * Fails if @chart is not a #BaobabChart.
 **/
void
baobab_chart_set_max_depth (BaobabChart *chart,
                            guint        max_depth)
{
  g_return_if_fail (BAOBAB_IS_CHART (chart));

  max_depth = MIN (max_depth, BAOBAB_CHART_MAX_DEPTH);
  max_depth = MAX (max_depth, BAOBAB_CHART_MIN_DEPTH);

  if (max_depth == chart->priv->max_depth)
    return;

  chart->priv->max_depth = max_depth;
  g_object_notify (G_OBJECT (chart), "max-depth");

  chart->priv->model_changed = TRUE;

  gtk_widget_queue_draw (GTK_WIDGET (chart));
}

/**
 * baobab_chart_get_max_depth:
 * @chart: a #BaobabChart.
 *
 * Returns the maximum number of levels that will be show in the
 * widget.
 *
 * Fails if @chart is not a #BaobabChart.
 **/
guint
baobab_chart_get_max_depth (BaobabChart *chart)
{
  g_return_val_if_fail (BAOBAB_IS_CHART (chart), 0);

  return chart->priv->max_depth;
}

/**
 * baobab_chart_set_root:
 * @chart: a #BaobabChart
 * @root: a #GtkTreePath indicating the node which will be used as
 * the widget root.
 *
 * Sets the node pointed by @root as the new root of the widget
 * @chart.
 *
 * Fails if @chart is not a #BaobabChart or if @chart has not
 * a #GtkTreeModel set.
 **/
void
baobab_chart_set_root (BaobabChart *chart,
                       GtkTreePath *root)
{
  g_return_if_fail (BAOBAB_IS_CHART (chart));
  g_return_if_fail (chart->priv->model != NULL);

  if (chart->priv->root)
    {
      /* Check that given root is different from current */
      GtkTreePath *current_root = gtk_tree_row_reference_get_path (chart->priv->root);
      if (current_root && (gtk_tree_path_compare (current_root, root) == 0))
        return;

      /* Free current root */
      gtk_tree_row_reference_free (chart->priv->root);
    }
  else if (!root)
    return;

  chart->priv->root = root ? gtk_tree_row_reference_new (chart->priv->model, root) : NULL;

  g_object_notify (G_OBJECT (chart), "root");

  gtk_widget_queue_draw (GTK_WIDGET (chart));
}

/**
 * baobab_chart_get_root:
 * @chart: a #BaobabChart.
 *
 * Returns a #GtkTreePath pointing to the root of the widget. The
 * programmer has the responsability to free the used memory once
 * finished with the returned value. It returns NULL if there is no
 * root node defined
 *
 * Fails if @chart is not a #BaobabChart.
 **/
GtkTreePath *
baobab_chart_get_root (BaobabChart *chart)
{
  g_return_val_if_fail (BAOBAB_IS_CHART (chart), NULL);

  if (chart->priv->root)
    return gtk_tree_row_reference_get_path (chart->priv->root);
  else
    return NULL;
}

/**
 * baobab_chart_freeze_updates:
 * @chart: the #BaobabChart whose model signals are going to be frozen.
 *
 * Disconnects @chart from the signals emitted by its model, and sets
 * the window of @chart to a "processing" state, so that the window
 * ignores changes in the chart's model and mouse events.
 * In order to connect again the window to the model, a call to
 * #baobab_chart_thaw_updates must be done.
 *
 * Fails if @chart is not a #BaobabChart.
 **/
void
baobab_chart_freeze_updates (BaobabChart *chart)
{
  cairo_surface_t *surface = NULL;
  GtkAllocation allocation;

  g_return_if_fail (BAOBAB_IS_CHART (chart));

  if (chart->priv->is_frozen)
    return;

  if (chart->priv->model)
    baobab_chart_disconnect_signals (chart, chart->priv->model);

  gtk_widget_get_allocation (GTK_WIDGET (chart), &allocation);
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        allocation.width,
                                        allocation.height);

  if (cairo_surface_status (surface) == CAIRO_STATUS_SUCCESS)
    {
      cairo_t *cr;

      cr = cairo_create (surface);

      baobab_chart_draw_chart (chart, cr);

      cairo_rectangle (cr,
                       0, 0,
                       allocation.width,
                       allocation.height);

      cairo_set_source_rgba (cr, 0.93, 0.93, 0.93, 0.5);  /* tango: eeeeec */
      cairo_fill_preserve (cr);

      cairo_clip (cr);

      chart->priv->memento = surface;

      cairo_destroy (cr);
    }

  chart->priv->is_frozen = TRUE;

  gtk_widget_queue_draw (GTK_WIDGET (chart));
}

/**
 * baobab_chart_thaw_updates:
 * @chart: the #BaobabChart whose model signals are frozen.
 *
 * Reconnects @chart to the signals emitted by its model, which
 * were disconnected through a call to #baobab_chart_freeze_updates.
 * Takes the window out of its "processing" state and forces a redraw
 * of the widget.
 *
 * Fails if @chart is not a #BaobabChart.
 **/
void
baobab_chart_thaw_updates (BaobabChart *chart)
{
  g_return_if_fail (BAOBAB_IS_CHART (chart));

  if (chart->priv->is_frozen)
    {
      if (chart->priv->model)
        baobab_chart_connect_signals (chart, chart->priv->model);

      if (chart->priv->memento)
        {
          cairo_surface_destroy (chart->priv->memento);
          chart->priv->memento = NULL;
        }

      chart->priv->is_frozen = FALSE;

      chart->priv->model_changed = TRUE;
      gtk_widget_queue_draw (GTK_WIDGET (chart));
    }
}

/**
 * baobab_chart_zoom_in:
 * @chart: the #BaobabChart requested to zoom in.
 *
 * Zooms in the chart by decreasing its maximun depth.
 *
 * Fails if @chart is not a #BaobabChart.
 **/
void
baobab_chart_zoom_in (BaobabChart *chart)
{
  BaobabChartClass *class;
  guint new_max_depth;

  g_return_if_fail (BAOBAB_IS_CHART (chart));

  class = BAOBAB_CHART_GET_CLASS (chart);

  if (class->can_zoom_in != NULL)
    new_max_depth = class->can_zoom_in (chart);
  else
    new_max_depth = chart->priv->max_depth - 1;

  baobab_chart_set_max_depth (chart, new_max_depth);
}

/**
 * baobab_chart_zoom_out:
 * @chart: the #BaobabChart requested to zoom out.
 *
 * Zooms out the chart by increasing its maximun depth.
 *
 * Fails if @chart is not a #BaobabChart.
 **/
void
baobab_chart_zoom_out (BaobabChart *chart)
{
  g_return_if_fail (BAOBAB_IS_CHART (chart));

  baobab_chart_set_max_depth (chart, baobab_chart_get_max_depth (chart) + 1);
}

/**
 * baobab_chart_can_zoom_in:
 * @chart: the #BaobabChart to ask if can be zoomed in.
 *
 * Returns a boolean telling whether the chart can be zoomed in, given its current
 * visualization conditions.
 *
 * Fails if @chart is not a #BaobabChart.
 **/
gboolean
baobab_chart_can_zoom_in (BaobabChart *chart)
{
  BaobabChartClass *class;

  g_return_val_if_fail (BAOBAB_IS_CHART (chart), FALSE);

  if (chart->priv->is_frozen)
    return FALSE;

  class = BAOBAB_CHART_GET_CLASS (chart);

  if (class->can_zoom_in != NULL)
    return class->can_zoom_in (chart) > 0;
  else
    return chart->priv->max_depth > 1;
}

/**
 * baobab_chart_can_zoom_out:
 * @chart: the #BaobabChart to ask if can be zoomed out.
 *
 * Returns a boolean telling whether the chart can be zoomed out, given its current
 * visualization conditions.
 *
 * Fails if @chart is not a #BaobabChart.
 **/
gboolean
baobab_chart_can_zoom_out (BaobabChart *chart)
{
  BaobabChartClass *class;

  g_return_val_if_fail (BAOBAB_IS_CHART (chart), FALSE);

  if (chart->priv->is_frozen)
    return FALSE;

  class = BAOBAB_CHART_GET_CLASS (chart);

  if (class->can_zoom_out != NULL)
    return class->can_zoom_out (chart) > 0;
  else
    return (chart->priv->max_depth < BAOBAB_CHART_MAX_DEPTH);
}

/**
 * baobab_chart_move_up_root:
 * @chart: the #BaobabChart whose root is requested to move up one level.
 *
 * Move root to the inmediate parent of the current root item of @chart.
 *
 * Fails if @chart is not a #BaobabChart.
 **/
void
baobab_chart_move_up_root (BaobabChart *chart)
{
  GtkTreeIter parent_iter;
  GtkTreePath *path;
  GtkTreeIter root_iter;

  gint valid;
  GtkTreePath *parent_path;

  g_return_if_fail (BAOBAB_IS_CHART (chart));

  if (chart->priv->root == NULL)
    return;

  path = gtk_tree_row_reference_get_path (chart->priv->root);

  if (path != NULL)
    gtk_tree_model_get_iter (chart->priv->model, &root_iter, path);
  else
    return;

  if (gtk_tree_model_iter_parent (chart->priv->model, &parent_iter, &root_iter))
    {
      gtk_tree_model_get (chart->priv->model, &parent_iter, chart->priv->valid_column,
                          &valid, -1);

      if (valid == -1)
        return;

      gtk_tree_row_reference_free (chart->priv->root);
      parent_path = gtk_tree_model_get_path (chart->priv->model, &parent_iter);
      chart->priv->root = gtk_tree_row_reference_new (chart->priv->model, parent_path);
      gtk_tree_path_free (parent_path);

      g_signal_emit (BAOBAB_CHART (chart),
                     baobab_chart_signals[ITEM_ACTIVATED],
                     0, &parent_iter);

      gtk_widget_queue_draw (GTK_WIDGET (chart));
    }

  gtk_tree_path_free (path);
}

/**
 * baobab_chart_save_snapshot:
 * @chart: the #BaobabChart requested to be exported to image.
 *
 * Opens a dialog to allow saving the current chart's image as a PNG, JPEG or 
 * BMP image.
 *
 * Fails if @chart is not a #BaobabChart.
 **/
void
baobab_chart_save_snapshot (BaobabChart *chart)
{
  GdkPixbuf *pixbuf;
  GtkWidget *fs_dlg;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *opt_menu;
  gchar *sel_type;
  gchar *filename;
  gchar *def_filename;

  BaobabChartItem *item;

  g_return_if_fail (BAOBAB_IS_CHART (chart));

  while (gtk_events_pending ())
    gtk_main_iteration ();

  /* Get the chart's pixbuf */
  pixbuf = baobab_chart_get_pixbuf (chart);
  if (pixbuf == NULL)
    {
      GtkWidget *dialog;
      dialog = gtk_message_dialog_new (NULL,
                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_OK,
                                       _("Cannot create pixbuf image!"));
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);

      return;
    }

  /* Popup the File chooser dialog */
  fs_dlg = gtk_file_chooser_dialog_new (_("Save Snapshot"),
                                        NULL,
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        GTK_STOCK_CANCEL,
                                        GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_SAVE,
                                        GTK_RESPONSE_ACCEPT, NULL);

  item = (BaobabChartItem *) chart->priv->first_item->data;
  def_filename = g_strdup_printf (SNAPSHOT_DEF_FILENAME_FORMAT, item->name);

  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (fs_dlg), def_filename);
  g_free (def_filename);

  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (fs_dlg),
                                       g_get_home_dir ());

  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (fs_dlg), TRUE);

  /* extra widget */
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 0);
  gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (fs_dlg), vbox);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 6);

  label = gtk_label_new_with_mnemonic (_("_Image type:"));
  gtk_box_pack_start (GTK_BOX (hbox),
                      label,
                      FALSE, FALSE, 0);

  opt_menu = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (opt_menu), "png");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (opt_menu), "jpeg");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (opt_menu), "bmp");
  gtk_combo_box_set_active (GTK_COMBO_BOX (opt_menu), 0);
  gtk_box_pack_start (GTK_BOX (hbox), opt_menu, TRUE, TRUE, 0);

  gtk_label_set_mnemonic_widget (GTK_LABEL (label), opt_menu);
  gtk_widget_show_all (vbox);

  if (gtk_dialog_run (GTK_DIALOG (fs_dlg)) == GTK_RESPONSE_ACCEPT)
    {
      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fs_dlg));
      sel_type = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (opt_menu));
      if (! g_str_has_suffix (filename, sel_type))
        {
          gchar *tmp;
          tmp = filename;
          filename = g_strjoin (".", filename, sel_type, NULL);
          g_free (tmp);
        }
      gdk_pixbuf_save (pixbuf, filename, sel_type, NULL, NULL);

      g_free (filename);
      g_free (sel_type);
    }

  gtk_widget_destroy (fs_dlg);
  g_object_unref (pixbuf);
}

/**
 * baobab_chart_is_frozen:
 * @chart: the #BaobabChart to ask if frozen.
 *
 * Returns a boolean telling whether the chart is in a frozen state, meanning 
 * that no actions should be taken uppon it.
 *
 * Fails if @chart is not a #BaobabChart.
 **/
gboolean
baobab_chart_is_frozen (BaobabChart *chart)
{
  g_return_val_if_fail (BAOBAB_IS_CHART (chart), FALSE);

  return chart->priv->is_frozen;
}

/**
 * baobab_chart_is_frozen:
 * @chart: the #BaobabChart to obtain the highlighted it from.
 *
 * Returns a BaobabChartItem corresponding to the item that currently has mouse 
 * pointer over, or NULL if no item is highlighted.
 *
 * Fails if @chart is not a #BaobabChart.
 **/
BaobabChartItem *
baobab_chart_get_highlighted_item (BaobabChart *chart)
{
  g_return_val_if_fail (BAOBAB_IS_CHART (chart), NULL);

  return (chart->priv->highlighted_item ?
    (BaobabChartItem *) chart->priv->highlighted_item->data : NULL);
}
