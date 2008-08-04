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

#include <config.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "baobab-chart.h"

#define BAOBAB_CHART_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                                       BAOBAB_CHART_TYPE, \
                                       BaobabChartPrivate))

G_DEFINE_ABSTRACT_TYPE (BaobabChart, baobab_chart, GTK_TYPE_WIDGET);

enum
{
  LEFT_BUTTON   = 1,
  MIDDLE_BUTTON = 2,
  RIGHT_BUTTON  = 3
};


struct _BaobabChartPrivate
{
  gboolean summary_mode;

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
static void baobab_chart_free_items (GtkWidget *chart);
static void baobab_chart_draw (GtkWidget *chart,
                               cairo_t *cr,
                               GdkRectangle area);
static void baobab_chart_update_draw (BaobabChart *chart,
                                      GtkTreePath *path);
static void baobab_chart_row_changed (GtkTreeModel *model,
                                      GtkTreePath *path,
                                      GtkTreeIter *iter,
                                      gpointer data);
static void baobab_chart_row_inserted (GtkTreeModel *model,
                                       GtkTreePath *path,
                                       GtkTreeIter *iter,
                                       gpointer data);
static void baobab_chart_row_has_child_toggled (GtkTreeModel *model,
                                                GtkTreePath *path,
                                                GtkTreeIter *iter,
                                                gpointer data);
static void baobab_chart_row_deleted (GtkTreeModel *model,
                                      GtkTreePath *path,
                                      gpointer data);
static void baobab_chart_rows_reordered (GtkTreeModel *model,
                                         GtkTreePath *parent,
                                         GtkTreeIter *iter,
                                         gint *new_order,
                                         gpointer data);
static gboolean baobab_chart_expose (GtkWidget *chart,
                                     GdkEventExpose *event);
static void baobab_chart_interpolate_colors (BaobabChartColor *color,
                                             BaobabChartColor colora,
                                             BaobabChartColor colorb,
                                             gdouble percentage);
static gint baobab_chart_button_release (GtkWidget *widget,
                                         GdkEventButton *event);
static gint baobab_chart_scroll (GtkWidget *widget,
                                 GdkEventScroll *event);
static gint baobab_chart_motion_notify (GtkWidget *widget,
                                        GdkEventMotion *event);
static gint baobab_chart_leave_notify (GtkWidget *widget,
                                       GdkEventCrossing *event);
static inline void baobab_chart_disconnect_signals (GtkWidget *chart,
                                                    GtkTreeModel *model);
static inline void baobab_chart_connect_signals (GtkWidget *chart,
                                                 GtkTreeModel *model);
static void baobab_chart_get_items (GtkWidget *chart, GtkTreePath *root);
static gboolean baobab_chart_query_tooltip (GtkWidget  *widget,
                                            gint        x,
                                            gint        y,
                                            gboolean    keyboard_mode,
                                            GtkTooltip *tooltip,
                                            gpointer    user_data);


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
  widget_class->expose_event = baobab_chart_expose;
  widget_class->size_allocate = baobab_chart_size_allocate;
  widget_class->scroll_event = baobab_chart_scroll;

  /* Baobab Chart abstract methods */
  class->draw_item               = NULL;
  class->pre_draw                = NULL;
  class->post_draw               = NULL;
  class->calculate_item_geometry = NULL;
  class->is_point_over_item      = NULL;
  class->get_item_rectangle      = NULL;

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
    g_signal_new ("item_activated",
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
  BaobabChartPrivate *priv;

  priv = BAOBAB_CHART_GET_PRIVATE (chart);
  chart->priv = priv;

  priv->summary_mode = TRUE;
  priv->model = NULL;
  priv->max_depth = BAOBAB_CHART_MAX_DEPTH;
  priv->name_column = 0;
  priv->size_column = 0;
  priv->info_column = 0;
  priv->percentage_column = 0;
  priv->valid_column = 0;
  priv->button_pressed = FALSE;
  priv->is_frozen = FALSE;
  priv->memento = NULL;
  priv->root = NULL;

  priv->first_item = NULL;
  priv->last_item = NULL;
  priv->highlighted_item = NULL;
}

static void
baobab_chart_dispose (GObject *object)
{
  BaobabChartPrivate *priv;

  baobab_chart_free_items (GTK_WIDGET (object));

  priv = BAOBAB_CHART (object)->priv;

  if (priv->model)
    {
      baobab_chart_disconnect_signals (GTK_WIDGET (object),
                                       priv->model);

      g_object_unref (priv->model);

      priv->model = NULL;
    }

  if (priv->root)
    {
      gtk_tree_row_reference_free (priv->root);

      priv->root = NULL;
    }

  G_OBJECT_CLASS (baobab_chart_parent_class)->dispose (object);
}

static void
baobab_chart_realize (GtkWidget *widget)
{
  BaobabChart *chart;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (BAOBAB_IS_CHART (widget));

  chart = BAOBAB_CHART (widget);
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
                                   &attributes,
                                   attributes_mask);
  gdk_window_set_user_data (widget->window, chart);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

  gtk_widget_add_events (widget,
                         GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK |
                         GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK |
                         GDK_POINTER_MOTION_HINT_MASK | GDK_LEAVE_NOTIFY_MASK);
}

static void
baobab_chart_size_allocate (GtkWidget *widget,
                            GtkAllocation *allocation)
{
  BaobabChartPrivate *priv;
  BaobabChartClass *class;
  BaobabChartItem *item;
  GList *node;

  g_return_if_fail (BAOBAB_IS_CHART (widget));
  g_return_if_fail (allocation != NULL);

  priv = BAOBAB_CHART (widget)->priv;
  class = BAOBAB_CHART_GET_CLASS (widget);

  widget->allocation = *allocation;

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
                  allocation->x, allocation->y,
                  allocation->width, allocation->height);

      node = priv->first_item;
      while (node != NULL)
        {
          item = (BaobabChartItem *) node->data;
          item->has_visible_children = FALSE;
          item->visible = FALSE;
          class->calculate_item_geometry (widget, item);

          node = node->next;
        }
    }
}

static void
baobab_chart_set_property (GObject         *object,
                           guint            prop_id,
                           const GValue    *value,
                           GParamSpec      *pspec)
{
  BaobabChart *chart;

  chart = BAOBAB_CHART (object);

  switch (prop_id)
    {
    case PROP_MAX_DEPTH:
      baobab_chart_set_max_depth (GTK_WIDGET (chart), g_value_get_int (value));
      break;
    case PROP_MODEL:
      baobab_chart_set_model (GTK_WIDGET (chart), g_value_get_object (value));
      break;
    case PROP_ROOT:
      baobab_chart_set_root (GTK_WIDGET (chart), g_value_get_object (value));
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
  BaobabChartPrivate *priv;

  priv = BAOBAB_CHART (object)->priv;

  switch (prop_id)
    {
    case PROP_MAX_DEPTH:
      g_value_set_int (value, priv->max_depth);
      break;
    case PROP_MODEL:
      g_value_set_object (value, priv->model);
      break;
    case PROP_ROOT:
      g_value_set_object (value, priv->root);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static GList
*baobab_chart_add_item (GtkWidget *chart,
                        guint depth,
                        gdouble rel_start,
                        gdouble rel_size,
                        GtkTreeIter iter)
{
  BaobabChartPrivate *priv;
  BaobabChartItem *item;

  gchar *name;
  gchar *size;

  priv = BAOBAB_CHART_GET_PRIVATE (chart);

  gtk_tree_model_get (priv->model, &iter,
                      priv->name_column, &name, -1);
  gtk_tree_model_get (priv->model, &iter,
                      priv->size_column, &size, -1);

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

  priv->last_item = g_list_prepend (priv->last_item, item);

  return priv->last_item;
}

static void
baobab_chart_free_items (GtkWidget *chart)
{
  BaobabChartPrivate *priv;
  BaobabChartItem *item;
  GList *node;
  GList *next;

  priv = BAOBAB_CHART_GET_PRIVATE (chart);

  node = priv->first_item;
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

  priv->first_item = NULL;
  priv->last_item = NULL;
  priv->highlighted_item = NULL;
}

static void
baobab_chart_get_items (GtkWidget *chart, GtkTreePath *root)
{
  BaobabChartPrivate *priv;
  BaobabChartItem *item, *child_item;

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
  gdouble rel_size;

  priv = BAOBAB_CHART_GET_PRIVATE (chart);

  /* First we free current item list */
  baobab_chart_free_items (chart);

  /* Get the tree iteration corresponding to root */
  gtk_tree_model_get_iter (priv->model, &initial_iter, root);


  model_root_path = gtk_tree_path_new_first ();
  gtk_tree_model_get_iter (priv->model, &model_root_iter, model_root_path);
  gtk_tree_path_free (model_root_path);

  gtk_tree_model_get (priv->model, &model_root_iter,
                      priv->percentage_column, &size, -1);

  /* Create first item */
  node = baobab_chart_add_item (chart, 0, 0, 100, initial_iter);

  /* If summary mode, insert a new item that will become root */
  if (priv->summary_mode)
    {
      item = (BaobabChartItem *) node->data;
      g_free (item->size);
      item->size = NULL;

      child_node = baobab_chart_add_item (chart, 1, 0, size, initial_iter);
      child_item = (BaobabChartItem *) child_node->data;
      child_item->parent = node;
      child_item->name = item->name;

      item->name = NULL;
    }

  /* Iterate through childs building the list */
  class = BAOBAB_CHART_GET_CLASS (chart);

  do
    {
      item = (BaobabChartItem *) node->data;
      item->has_any_child = gtk_tree_model_iter_children (priv->model, 
                                                          &child_iter, 
                                                          &(item->iter));

      /* Calculate item geometry */
      class->calculate_item_geometry (chart, item);

      if (! item->visible)
        {
          node = node->prev;
          continue;
        }

      /* Get item's children and add them to the list */
      if ((item->has_any_child) && (item->depth < priv->max_depth))
        {
          rel_start = 0;

          do
            {
              gtk_tree_model_get (priv->model, &child_iter,
                                  priv->percentage_column, &size, -1);

              child_node = baobab_chart_add_item (chart,
                                                  item->depth + 1,
                                                  rel_start,
                                                  size,
                                                  child_iter);
              child = (BaobabChartItem *) child_node->data;
              child->parent = node;
              rel_start += size;
            }
          while (gtk_tree_model_iter_next (priv->model, &child_iter));
        }

      node = node->prev;
    }
  while (node != NULL);

  /* Reverse the list, 'cause we created it from the tail, for efficiency reasons */
  priv->first_item = g_list_reverse (priv->last_item);

  priv->model_changed = FALSE;
}

static void
baobab_chart_draw (GtkWidget *chart,
                   cairo_t *cr,
                   GdkRectangle area)
{
  BaobabChartPrivate *priv;
  BaobabChartClass *class;

  GList *node;
  BaobabChartItem *item;
  gboolean highlighted;

  priv = BAOBAB_CHART_GET_PRIVATE (chart);
  class = BAOBAB_CHART_GET_CLASS (chart);

  /* call pre-draw abstract method */
  if (class->pre_draw)
    class->pre_draw (chart, cr);

  cairo_save (cr);

  node = priv->first_item;
  while (node != NULL)
    {
      item = (BaobabChartItem *) node->data;

      if ((item->visible) && (gdk_rectangle_intersect (&area, &item->rect, NULL)))
        {
          highlighted = (node == priv->highlighted_item);

          class->draw_item (chart, cr, item, highlighted);
        }

      node = node->next;
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
  BaobabChartPrivate *priv;
  GtkTreePath *root_path = NULL;
  gint root_depth, node_depth;

  if (!GTK_WIDGET_REALIZED (chart))
    return;

  priv = BAOBAB_CHART (chart)->priv;

  if (priv->root != NULL)
    {
      root_path = gtk_tree_row_reference_get_path (priv->root);

      if (root_path == NULL)
        {
          gtk_tree_row_reference_free (priv->root);
          priv->root = NULL;
        }
    }

  if (priv->root == NULL)
    root_path = gtk_tree_path_new_first ();


  root_depth = gtk_tree_path_get_depth (root_path);
  node_depth = gtk_tree_path_get_depth (path);

  if (((node_depth-root_depth)<=priv->max_depth)&&
      ((gtk_tree_path_is_ancestor (root_path, path))||
       (gtk_tree_path_compare (root_path, path) == 0)))
    {
      gtk_widget_queue_draw (GTK_WIDGET (chart));
    }

  gtk_tree_path_free (root_path);
}

static void
baobab_chart_row_changed (GtkTreeModel    *model,
                          GtkTreePath     *path,
                          GtkTreeIter     *iter,
                          gpointer         data)
{
  g_return_if_fail (BAOBAB_IS_CHART (data));
  g_return_if_fail (path != NULL || iter != NULL);

  BAOBAB_CHART_GET_PRIVATE (data)->model_changed = TRUE;

  baobab_chart_update_draw (BAOBAB_CHART (data), path);
}

static void
baobab_chart_row_inserted (GtkTreeModel    *model,
                           GtkTreePath     *path,
                           GtkTreeIter     *iter,
                           gpointer         data)
{
  g_return_if_fail (BAOBAB_IS_CHART (data));
  g_return_if_fail (path != NULL || iter != NULL);

  BAOBAB_CHART_GET_PRIVATE (data)->model_changed = TRUE;

  baobab_chart_update_draw (BAOBAB_CHART (data), path);
}

static void
baobab_chart_row_has_child_toggled (GtkTreeModel    *model,
                                    GtkTreePath     *path,
                                    GtkTreeIter     *iter,
                                    gpointer         data)
{
  g_return_if_fail (BAOBAB_IS_CHART (data));
  g_return_if_fail (path != NULL || iter != NULL);

  BAOBAB_CHART_GET_PRIVATE (data)->model_changed = TRUE;

  baobab_chart_update_draw (BAOBAB_CHART (data), path);
}

static void
baobab_chart_row_deleted (GtkTreeModel    *model,
                          GtkTreePath     *path,
                          gpointer         data)
{
  g_return_if_fail (BAOBAB_IS_CHART (data));
  g_return_if_fail (path != NULL);

  BAOBAB_CHART_GET_PRIVATE (data)->model_changed = TRUE;

  baobab_chart_update_draw (BAOBAB_CHART (data), path);

}

static void
baobab_chart_rows_reordered (GtkTreeModel    *model,
                             GtkTreePath     *path,
                             GtkTreeIter     *iter,
                             gint            *new_order,
                             gpointer         data)
{
  g_return_if_fail (BAOBAB_IS_CHART (data));
  g_return_if_fail (path != NULL || iter != NULL);

  BAOBAB_CHART_GET_PRIVATE (data)->model_changed = TRUE;

  baobab_chart_update_draw (BAOBAB_CHART (data), path);

}

static gboolean
baobab_chart_expose (GtkWidget *chart, GdkEventExpose *event)
{
  cairo_t *cr;
  BaobabChartPrivate *priv;
  gint w, h;
  gdouble p, sx, sy;
  GtkTreePath *root_path = NULL;
  GtkTreePath *current_path = NULL;

  priv = BAOBAB_CHART (chart)->priv;

  /* the columns are not set we paint nothing */
  if (priv->name_column == priv->percentage_column)
    return FALSE;

  /* get a cairo_t */
  cr = gdk_cairo_create (chart->window);

  cairo_rectangle (cr,
                   event->area.x, event->area.y,
                   event->area.width, event->area.height);

  /* there is no model we can not paint */
  if ((priv->is_frozen) || (priv->model == NULL))
    {
      if (priv->memento != NULL)
        {
          w = cairo_image_surface_get_width (priv->memento);
          h = cairo_image_surface_get_height (priv->memento);

          cairo_clip (cr);

          if (w > 0 && h > 0 &&
          !(chart->allocation.width == w &&
                chart->allocation.height == h))
            {
              /* minimal available proportion */
              p = MIN (chart->allocation.width / (1.0 * w),
                       chart->allocation.height / (1.0 * h));

              sx = (gdouble) (chart->allocation.width - w * p) / 2.0;
              sy = (gdouble) (chart->allocation.height - h * p) / 2.0;

              cairo_translate (cr, sx, sy);
              cairo_scale (cr, p, p);
            }

          cairo_set_source_surface (cr,
                                    priv->memento,
                                    0, 0);
          cairo_paint (cr);
        }
    }
  else
    {
      cairo_set_source_rgb (cr, 1, 1, 1);
      cairo_fill_preserve (cr);

      cairo_clip (cr);

      if (priv->root != NULL)
        root_path = gtk_tree_row_reference_get_path (priv->root);

      if (root_path == NULL) {
        root_path = gtk_tree_path_new_first ();
        priv->root = NULL;
      }

      /* Check if tree model was modified in any way */
      if ((priv->model_changed) ||
           (priv->first_item == NULL))
        baobab_chart_get_items (chart, root_path);
      else
        {
          /* Check if root was changed */
          current_path = gtk_tree_model_get_path (priv->model,
                         &((BaobabChartItem*) priv->first_item->data)->iter);

          if (gtk_tree_path_compare (root_path, current_path) != 0)
            baobab_chart_get_items (chart, root_path);

          gtk_tree_path_free (current_path);
        }

      gtk_tree_path_free (root_path);

      baobab_chart_draw (chart, cr, event->area);
    }

  cairo_destroy (cr);

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

static gint
baobab_chart_button_release (GtkWidget *widget,
                             GdkEventButton *event)
{
  BaobabChartPrivate *priv;

  priv = BAOBAB_CHART (widget)->priv;

  if (priv->is_frozen)
    return TRUE;

  switch (event->button)
    {
    case LEFT_BUTTON:
      /* Enter into a subdir */
      if (priv->highlighted_item != NULL)
        g_signal_emit (BAOBAB_CHART (widget),
                       baobab_chart_signals[ITEM_ACTIVATED],
                       0, &((BaobabChartItem*) priv->highlighted_item->data)->iter);

      break;

    case MIDDLE_BUTTON:
      /* Go back to the parent dir */
      baobab_chart_move_up_root (widget);
      break;
    }

  return FALSE;
}

static gint
baobab_chart_scroll (GtkWidget *widget,
                     GdkEventScroll *event)
{
  BaobabChartPrivate * priv = BAOBAB_CHART_GET_PRIVATE (widget);

  switch (event->direction)
    {
    case GDK_SCROLL_LEFT :
    case GDK_SCROLL_UP :
      baobab_chart_zoom_out (widget);
      /* change the selected item when zooming */
      baobab_chart_motion_notify (widget, (GdkEventMotion *)event);
      break;

    case GDK_SCROLL_RIGHT :
    case GDK_SCROLL_DOWN :
      baobab_chart_zoom_in (widget);
      break;
    }

  return FALSE;
}

static void
baobab_chart_set_item_highlight (GtkWidget *chart,
                                 GList *node,
                                 gboolean highlighted)
{
  BaobabChartItem *item;
  BaobabChartPrivate *priv;
  BaobabChartClass *class;

  if (node == NULL)
    return;

  item = (BaobabChartItem *) node->data;
  priv = BAOBAB_CHART_GET_PRIVATE (chart);
  class = BAOBAB_CHART_GET_CLASS (chart);

  if (highlighted)
    priv->highlighted_item = node;
  else
    priv->highlighted_item = NULL;

  gdk_window_invalidate_rect (chart->window, &item->rect, TRUE);
}

static gint
baobab_chart_motion_notify (GtkWidget *widget,
                            GdkEventMotion *event)
{
  BaobabChartPrivate *priv;
  BaobabChartClass *class;
  GList *node;
  BaobabChartItem *item;
  gboolean found = FALSE;

  priv = BAOBAB_CHART_GET_PRIVATE (widget);
  class = BAOBAB_CHART_GET_CLASS (widget);

  /* Check if the pointer is over an item */
  node = priv->last_item;
  while (node != NULL)
    {
      item = (BaobabChartItem *) node->data;

      if ((item->visible) && (class->is_point_over_item (widget, item, event->x, event->y)))
        {
          if (priv->highlighted_item != node)
            {
              baobab_chart_set_item_highlight (widget, priv->highlighted_item, FALSE);

              gtk_widget_set_has_tooltip (widget, TRUE);
              baobab_chart_set_item_highlight (widget, node, TRUE);
            }

          found = TRUE;
          break;
        }
      node = node->prev;
    }

  /* If we never found a highlighted item, but there is an old highlighted item,
     redraw it to turn it off */
  if (! found)
    {
      baobab_chart_set_item_highlight (widget, priv->highlighted_item, FALSE);
      gtk_widget_set_has_tooltip (widget, FALSE);
    }

  /* Continue receiving motion notifies */
  gdk_event_request_motions (event);

  return FALSE;
}

static gint
baobab_chart_leave_notify (GtkWidget *widget,
                           GdkEventCrossing *event)
{
  BaobabChartPrivate *priv;

  priv = BAOBAB_CHART_GET_PRIVATE (widget);
  baobab_chart_set_item_highlight (widget, priv->highlighted_item, FALSE);

  return FALSE;
}

static inline void
baobab_chart_connect_signals (GtkWidget *chart,
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
  g_signal_connect (chart,
                    "button-release-event",
                    G_CALLBACK (baobab_chart_button_release),
                    chart);
}

static inline void
baobab_chart_disconnect_signals (GtkWidget *chart,
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
  g_signal_handlers_disconnect_by_func (chart,
                                        baobab_chart_button_release,
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
  BaobabChartPrivate *priv;
  BaobabChartItem *item;
  char *markup;

  priv = BAOBAB_CHART_GET_PRIVATE (widget);

  if (priv->highlighted_item == NULL)
    return FALSE;

  item = (BaobabChartItem *) priv->highlighted_item->data;

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

GdkPixbuf*
baobab_chart_get_pixbuf (GtkWidget *widget)
{
  gint w, h;
  GdkPixbuf *pixbuf;

  g_return_if_fail (BAOBAB_IS_CHART (widget));

  gdk_drawable_get_size (widget->window, &w, &h);
  pixbuf = gdk_pixbuf_get_from_drawable (NULL,
                                         widget->window,
                                         gdk_colormap_get_system (),
                                         0, 0,
                                         0, 0,
                                         w, h);

  return pixbuf;
}

/* Public functions start here */

/**
 * baobab_chart_new:
 *
 * Constructor for the baobab_chart class
 *
 * Returns: a new #BaobabChart object
 *
 **/
GtkWidget *
baobab_chart_new ()
{
  return g_object_new (BAOBAB_CHART_TYPE, NULL);
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
baobab_chart_set_model_with_columns (GtkWidget *chart,
                                     GtkTreeModel *model,
                                     guint name_column,
                                     guint size_column,
                                     guint info_column,
                                     guint percentage_column,
                                     guint valid_column,
                                     GtkTreePath *root)
{
  BaobabChartPrivate *priv;

  g_return_if_fail (BAOBAB_IS_CHART (chart));
  g_return_if_fail (GTK_IS_TREE_MODEL (model));

  priv = BAOBAB_CHART (chart)->priv;

  baobab_chart_set_model (chart, model);

  if (root != NULL)
    {
      priv->root = gtk_tree_row_reference_new (model, root);
      g_object_notify (G_OBJECT (chart), "root");
    }

  priv->name_column = name_column;
  priv->size_column = size_column;
  priv->info_column = info_column;
  priv->percentage_column = percentage_column;
  priv->valid_column = valid_column;
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
baobab_chart_set_model (GtkWidget *chart,
                             GtkTreeModel *model)
{
  BaobabChartPrivate *priv;

  g_return_if_fail (BAOBAB_IS_CHART (chart));
  g_return_if_fail (GTK_IS_TREE_MODEL (model));

  priv = BAOBAB_CHART (chart)->priv;

  if (model == priv->model)
    return;

  if (priv->model)
    {
      if (! priv->is_frozen)
        baobab_chart_disconnect_signals (chart,
                                         priv->model);
      g_object_unref (priv->model);
    }

  priv->model = model;
  g_object_ref (priv->model);

  if (! priv->is_frozen)
    baobab_chart_connect_signals (chart,
                                  priv->model);

  if (priv->root)
    gtk_tree_row_reference_free (priv->root);

  priv->root = NULL;

  g_object_notify (G_OBJECT (chart), "model");

  gtk_widget_queue_draw (chart);
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
baobab_chart_get_model (GtkWidget *chart)
{
  g_return_val_if_fail (BAOBAB_IS_CHART (chart), NULL);

  return BAOBAB_CHART (chart)->priv->model;
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
baobab_chart_set_max_depth (GtkWidget *chart,
                            guint max_depth)
{
  BaobabChartPrivate *priv;

  g_return_if_fail (BAOBAB_IS_CHART (chart));

  priv = BAOBAB_CHART_GET_PRIVATE (chart);

  max_depth = MIN (max_depth, BAOBAB_CHART_MAX_DEPTH);
  max_depth = MAX (max_depth, BAOBAB_CHART_MIN_DEPTH);

  if (max_depth == priv->max_depth)
    return;

  priv->max_depth = max_depth;
  g_object_notify (G_OBJECT (chart), "max-depth");

  priv->model_changed = TRUE;

  gtk_widget_queue_draw (chart);
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
baobab_chart_get_max_depth (GtkWidget *chart)
{
  g_return_val_if_fail (BAOBAB_IS_CHART (chart), 0);

  return BAOBAB_CHART (chart)->priv->max_depth;
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
baobab_chart_set_root (GtkWidget *chart,
                       GtkTreePath *root)
{
  BaobabChartPrivate *priv;
  GtkTreeIter iter = {0};
  GtkTreePath *current_root;

  g_return_if_fail (BAOBAB_IS_CHART (chart));

  priv = BAOBAB_CHART (chart)->priv;

  g_return_if_fail (priv->model != NULL);

  if (priv->root) {
    /* Check that given root is different from current */
    current_root = gtk_tree_row_reference_get_path (priv->root);
    if ( (current_root) && (gtk_tree_path_compare (current_root, root) == 0) )
      return;

    /* Free current root */
    gtk_tree_row_reference_free (priv->root);
  }

  priv->root = gtk_tree_row_reference_new (priv->model, root);

  g_object_notify (G_OBJECT (chart), "root");

  gtk_widget_queue_draw (chart);
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
GtkTreePath*
baobab_chart_get_root (GtkWidget *chart)
{
  g_return_val_if_fail (BAOBAB_IS_CHART (chart), NULL);

  if (BAOBAB_CHART (chart)->priv->root)
    return gtk_tree_row_reference_get_path (BAOBAB_CHART (chart)->priv->root);
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
baobab_chart_freeze_updates (GtkWidget *chart)
{
  BaobabChartPrivate *priv;
  cairo_surface_t *surface = NULL;
  cairo_t *cr = NULL;
  GdkRectangle area;

  g_return_if_fail (BAOBAB_IS_CHART (chart));

  priv = BAOBAB_CHART_GET_PRIVATE (chart);

  if (priv->is_frozen)
    return;

  if (priv->model)
    baobab_chart_disconnect_signals (chart,
                                     priv->model);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        chart->allocation.width,
                                        chart->allocation.height);

  if (cairo_surface_status (surface) == CAIRO_STATUS_SUCCESS)
    {
      cr = cairo_create (surface);

      area.x = 0;
      area.y = 0;
      area.width = chart->allocation.width;
      area.height = chart->allocation.height;
      baobab_chart_draw (chart, cr, area);

      cairo_rectangle (cr,
                       0, 0,
                       chart->allocation.width,
                       chart->allocation.height);

      cairo_set_source_rgba (cr, 0.93, 0.93, 0.93, 0.5);  /* tango: eeeeec */
      cairo_fill_preserve (cr);

      cairo_clip (cr);

      priv->memento = surface;

      cairo_destroy (cr);
    }

  priv->is_frozen = TRUE;

  gtk_widget_queue_draw (chart);
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
baobab_chart_thaw_updates (GtkWidget *chart)
{
  BaobabChartPrivate *priv;

  g_return_if_fail (BAOBAB_IS_CHART (chart));

  priv = BAOBAB_CHART_GET_PRIVATE (chart);

  if (priv->is_frozen)
    {
      if (priv->model)
        baobab_chart_connect_signals (chart,
                                      priv->model);

      if (priv->memento)
        {
          cairo_surface_destroy (priv->memento);
          priv->memento = NULL;
        }

      priv->is_frozen = FALSE;

      priv->model_changed = TRUE;
      gtk_widget_queue_draw (chart);
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
baobab_chart_zoom_in (GtkWidget *chart)
{
  g_return_if_fail (BAOBAB_IS_CHART (chart));

  baobab_chart_set_max_depth (chart,
                              baobab_chart_get_max_depth (chart) - 1);
}

/**
 * baobab_chart_zoom_in:
 * @chart: the #BaobabChart requested to zoom out.
 *
 * Zooms out the chart by increasing its maximun depth.
 *
 * Fails if @chart is not a #BaobabChart.
 **/
void
baobab_chart_zoom_out (GtkWidget *chart)
{
  g_return_if_fail (BAOBAB_IS_CHART (chart));

  baobab_chart_set_max_depth (chart,
                              baobab_chart_get_max_depth (chart) + 1);
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
baobab_chart_move_up_root (GtkWidget *chart)
{
  BaobabChartPrivate *priv;

  GtkTreeIter parent_iter;
  GtkTreePath *path;
  GtkTreeIter root_iter;

  gint valid;
  GtkTreePath *parent_path;

  g_return_if_fail (BAOBAB_IS_CHART (chart));

  priv = BAOBAB_CHART_GET_PRIVATE (chart);

  if (priv->root == NULL)
    return;

  path = gtk_tree_row_reference_get_path (priv->root);

  if (path != NULL)
    gtk_tree_model_get_iter (priv->model, &root_iter, path);
  else
    return;

  if (gtk_tree_model_iter_parent (priv->model, &parent_iter, &root_iter))
    {
      gtk_tree_model_get (priv->model, &parent_iter, priv->valid_column,
                          &valid, -1);

      if (valid == -1)
        return;

      gtk_tree_row_reference_free (priv->root);
      parent_path = gtk_tree_model_get_path (priv->model, &parent_iter);
      priv->root = gtk_tree_row_reference_new (priv->model, parent_path);
      gtk_tree_path_free (parent_path);

      g_signal_emit (BAOBAB_CHART (chart),
                     baobab_chart_signals[ITEM_ACTIVATED],
                     0, &parent_iter);

      gtk_widget_queue_draw (chart);
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
baobab_chart_save_snapshot (GtkWidget *chart)
{
  BaobabChartPrivate *priv;

  GdkPixbuf *pixbuf;

  GtkWidget *fs_dlg;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *opt_menu;
  gchar *sel_type;
  gchar *filename;

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
  gtk_combo_box_append_text (GTK_COMBO_BOX (opt_menu), "png");
  gtk_combo_box_append_text (GTK_COMBO_BOX (opt_menu), "jpeg");
  gtk_combo_box_append_text (GTK_COMBO_BOX (opt_menu), "bmp");
  gtk_combo_box_set_active (GTK_COMBO_BOX (opt_menu), 0);
  gtk_box_pack_start (GTK_BOX (hbox), opt_menu, TRUE, TRUE, 0);

  gtk_label_set_mnemonic_widget (GTK_LABEL (label), opt_menu);
  gtk_widget_show_all (vbox);

  if (gtk_dialog_run (GTK_DIALOG (fs_dlg)) == GTK_RESPONSE_ACCEPT)
    {
      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fs_dlg));
      sel_type = gtk_combo_box_get_active_text (GTK_COMBO_BOX (opt_menu));
      gdk_pixbuf_save (pixbuf, filename, sel_type, NULL, NULL);

      g_free (filename);
      g_free (sel_type);
    }

  gtk_widget_destroy (fs_dlg);
  g_object_unref (pixbuf);
}

/**
 * baobab_chart_set_summary_mode:
 * @chart: the #BaobabChart to set summary mode to.
 * @summary_mode: boolean specifying TRUE is summary mode is on.
 *
 * Toggles on/off the summary mode (the initial mode that shows general state
 * before any scan has been made).
 *
 * Fails if @chart is not a #BaobabChart.
 **/
void
baobab_chart_set_summary_mode (GtkWidget *chart,
                               gboolean summary_mode)
{
  BaobabChartPrivate *priv;

  g_return_if_fail (BAOBAB_IS_CHART (chart));

  priv = BAOBAB_CHART_GET_PRIVATE (chart);
  priv->summary_mode = summary_mode;
}

/**
 * baobab_chart_get_summary_mode:
 * @chart: the #BaobabChart to obtain summary mode from.
 *
 * Returns a boolean representing whether the chart is in summary mode (the 
 * initial mode that shows general state before any scan has been made).
 *
 * Fails if @chart is not a #BaobabChart.
 **/
gboolean
baobab_chart_get_summary_mode (GtkWidget *chart)
{
  BaobabChartPrivate *priv;

  g_return_if_fail (BAOBAB_IS_CHART (chart));

  priv = BAOBAB_CHART_GET_PRIVATE (chart);
  return priv->summary_mode;
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
baobab_chart_is_frozen (GtkWidget *chart)
{
  BaobabChartPrivate *priv;

  g_return_if_fail (BAOBAB_IS_CHART (chart));

  priv = BAOBAB_CHART_GET_PRIVATE (chart);
  return priv->is_frozen;
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
baobab_chart_get_highlighted_item (GtkWidget *chart)
{
  BaobabChartPrivate *priv;

  g_return_if_fail (BAOBAB_IS_CHART (chart));

  priv = BAOBAB_CHART_GET_PRIVATE (chart);
  return (priv->highlighted_item ? 
    (BaobabChartItem *) priv->highlighted_item->data : NULL);
}
