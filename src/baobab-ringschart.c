/*
 * baobab-ringschart.c
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
#include <math.h>
#include <gtk/gtk.h>
#include <pango/pangocairo.h>
#include <glib/gi18n.h>

#include "baobab-ringschart.h"

#define MIN_DRAWABLE_ANGLE 0.03
#define EDGE_ANGLE 0.01
#define FILE_COLOR 0.6
#define WINDOW_PADDING 5
#define TOOLTIP_PADDING 3
#define SUBFOLDERTIP_TRANSP 0.666
#define TOOLTIP_TRANSP 0.666
#define TOOLTIP_SPACING 1
#define TOOLTIP_TIMEOUT 300 /* miliseconds before showing the tooltip
                               window after mouse pointer rests over
                               a sector */

#define BAOBAB_RINGSCHART_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                                            BAOBAB_RINGSCHART_TYPE, \
                                            BaobabRingschartPrivate))

G_DEFINE_TYPE (BaobabRingschart, baobab_ringschart, GTK_TYPE_WIDGET);

enum {
  LEFT_BUTTON = 1,
  MIDDLE_BUTTON = 2,
  RIGHT_BUTTON = 3
};

struct _BaobabRingschartPrivate {
  GtkTreeModel *model;
  gdouble center_x;
  gdouble center_y;
  gdouble max_radius;
  gdouble min_radius;
  gdouble thickness;
  gdouble pointer_x;
  gdouble pointer_y;
  guint max_depth;
  guint name_column;
  guint size_column;
  guint info_column;
  guint percentage_column;
  guint valid_column;
  guint current_depth;
  gint drawn_elements;
  gdouble current_init_angle;
  gdouble current_final_angle;
  gdouble point_angle;
  gboolean button_pressed;
  gboolean real_sector;
  gboolean is_frozen;
  gboolean transparent_tips;
  gboolean subfoldertips_enabled;
  guint timeout_event_source;
  gboolean draw_tooltip;
  guint init_depth;
  gboolean draw_center;
  cairo_surface_t *memento;
  GtkTreeRowReference *root;
};

typedef struct _Color Color;

struct _Color {
  gdouble red;
  gdouble green;
  gdouble blue;
};

typedef struct _Tooltip Tooltip;

struct _Tooltip {
  gdouble init_angle;
  gdouble final_angle;
  gint depth;
  gchar *name;
  gchar *size;
  GtkTreeRowReference *ref;
};

struct _Tooltip_rectangle {
  gdouble x;
  gdouble y;
  gdouble width;
  gdouble height;
};

typedef struct _Tooltip_rectangle Tooltip_rectangle;

/* Signals */
enum {
  SECTOR_ACTIVATED,
  LAST_SIGNAL
};

static guint ringschart_signals [LAST_SIGNAL] = { 0 };

/* Properties */
enum {
  PROP_0,
  PROP_MAX_DEPTH,
  PROP_MODEL,
  PROP_ROOT,
};

const Color tango_colors[]  = {{0.94, 0.16, 0.16}, /* tango: ef2929 */
                               {0.68, 0.49, 0.66}, /* tango: ad7fa8 */
                               {0.45, 0.62, 0.82}, /* tango: 729fcf */
                               {0.54, 0.89, 0.20}, /* tango: 8ae234 */
                               {0.91, 0.73, 0.43}, /* tango: e9b96e */
                               {0.99, 0.68, 0.25}}; /* tango: fcaf3e */

static void baobab_ringschart_class_init (BaobabRingschartClass *class);
static void baobab_ringschart_init (BaobabRingschart *object);
static void baobab_ringschart_realize (GtkWidget *widget);
static void baobab_ringschart_dispose (GObject *object);
static void baobab_ringschart_size_allocate (GtkWidget *widget,
                                             GtkAllocation *allocation);
static void baobab_ringschart_set_property (GObject *object,
                                            guint prop_id,
                                            const GValue *value,
                                            GParamSpec *pspec);
static void baobab_ringschart_get_property (GObject *object,
                                            guint prop_id,
                                            GValue *value,
                                            GParamSpec *pspec);
static void baobab_ringschart_update_draw (BaobabRingschart *rchart,
                                           GtkTreePath *path);
static void baobab_ringschart_row_changed (GtkTreeModel *model,
                                           GtkTreePath *path,
                                           GtkTreeIter *iter,
                                           gpointer data);
static void baobab_ringschart_row_inserted (GtkTreeModel *model,
                                            GtkTreePath *path,
                                            GtkTreeIter *iter,
                                            gpointer data);
static void baobab_ringschart_row_has_child_toggled (GtkTreeModel *model,
                                                     GtkTreePath *path,
                                                     GtkTreeIter *iter,
                                                     gpointer data);
static void baobab_ringschart_row_deleted (GtkTreeModel *model,
                                           GtkTreePath *path,
                                           gpointer data);
static void baobab_ringschart_rows_reordered (GtkTreeModel *model,
                                              GtkTreePath *parent,
                                              GtkTreeIter *iter,
                                              gint *new_order,
                                              gpointer data);
static gboolean baobab_ringschart_expose (GtkWidget *rchart, 
                                          GdkEventExpose *event);
static void baobab_ringschart_draw (GtkWidget *rchart, 
                                    cairo_t *cr);
static void draw_sector (cairo_t *cr, 
                         gdouble center_x, 
                         gdouble center_y,
                         gdouble radius,
                         gdouble thickness,
                         gdouble init_angle,
                         gdouble final_angle,
                         Color fill_color,
                         gboolean continued);
static void draw_circle (cairo_t *cr, 
                         gdouble center_x, 
                         gdouble center_y,
                         gdouble radius,
                         gboolean highlighted);
static void draw_tooltips (GtkWidget *rchart,
                           cairo_t *cr,
                           GSList *tooltips);
static void draw_tip (cairo_t *cr,
                      GtkWidget *rchart,
                      gdouble init_angle,
                      gdouble final_angle,
                      gdouble depth,
                      gchar *name,
                      gchar *size);
static void draw_folder_tip (cairo_t *cr,
                             GtkWidget *rchart,
                             gdouble init_angle,
                             gdouble final_angle,
                             gdouble depth,
                             gchar *name,
                             gchar *size,
                             Tooltip_rectangle *previous_rectangle);
static void interpolate_colors (Color *color,
                                Color colora,
                                Color colorb,
                                gdouble percentage);
static void get_color (Color *color,
                       gdouble angle,
                       gint circle_number,
                       gboolean highlighted);
static void tree_traverse (cairo_t *cr,
                           BaobabRingschart *rchart,
                           GtkTreeIter *iter,
                           guint depth,
                           gdouble radius,
                           gdouble init_angle,
                           gdouble final_angle,
                           GSList **tooltips,
                           gboolean real_draw);
static gint baobab_ringschart_button_release (GtkWidget *widget,
                                              GdkEventButton *event);
static gint baobab_ringschart_scroll (GtkWidget *widget,
                                      GdkEventScroll *event);
static gint baobab_ringschart_motion_notify (GtkWidget *widget,
                                             GdkEventMotion *event);
static inline void baobab_ringschart_disconnect_signals (GtkWidget *rchart, 
                                                         GtkTreeModel *model);
static inline void baobab_ringschart_connect_signals (GtkWidget *rchart,
                                                      GtkTreeModel *model);


static void
baobab_ringschart_class_init (BaobabRingschartClass *class)
{
  GObjectClass *obj_class;
  GtkWidgetClass *widget_class;

  obj_class = G_OBJECT_CLASS (class);
  widget_class = GTK_WIDGET_CLASS (class);

  /* GtkObject signals */
  obj_class->set_property = baobab_ringschart_set_property;
  obj_class->get_property = baobab_ringschart_get_property;
  obj_class->dispose = baobab_ringschart_dispose;

  /* GtkWidget signals */
  widget_class->realize = baobab_ringschart_realize;
  widget_class->expose_event = baobab_ringschart_expose;
  widget_class->size_allocate = baobab_ringschart_size_allocate;
  widget_class->button_release_event = baobab_ringschart_button_release;
  widget_class->scroll_event = baobab_ringschart_scroll;
  widget_class->motion_notify_event = baobab_ringschart_motion_notify;

  g_object_class_install_property (obj_class,
                                   PROP_MAX_DEPTH,
                                   g_param_spec_int ("max-depth",
                                                     _("Maximum depth"),
                                                     _("The maximum depth drawn in the rings chart from the root"),
                                                     1,
                                                     MAX_DRAWABLE_DEPTH,
                                                     MAX_DRAWABLE_DEPTH,
                                                     G_PARAM_READWRITE));

  g_object_class_install_property (obj_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("model",
							_("Rings chart model"),
							_("Set the model of the rings chart"),
							GTK_TYPE_TREE_MODEL,
							G_PARAM_READWRITE));

  g_object_class_install_property (obj_class,
                                   PROP_ROOT,
                                   g_param_spec_boxed ("root",
                                                       _("Rings chart root node"),
                                                       _("Set the root node from the model"),
                                                       GTK_TYPE_TREE_ITER,
                                                       G_PARAM_READWRITE));

  ringschart_signals[SECTOR_ACTIVATED] =
    g_signal_new ("sector_activated",
		  G_TYPE_FROM_CLASS (obj_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (BaobabRingschartClass, sector_activated),
		  NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_TREE_ITER);

  g_type_class_add_private (obj_class, sizeof (BaobabRingschartPrivate));
}

static void
baobab_ringschart_init (BaobabRingschart *rchart)
{
  BaobabRingschartPrivate *priv;

  priv = BAOBAB_RINGSCHART_GET_PRIVATE (rchart);
  rchart->priv = priv;

  priv->model = NULL;
  priv->center_x = 0;  
  priv->center_y = 0;
  priv->max_radius= 0;
  priv->min_radius= 0;
  priv->thickness= 0;
  priv->max_depth = MAX_DRAWABLE_DEPTH;
  priv->name_column = 0;
  priv->size_column = 0;
  priv->info_column = 0;
  priv->percentage_column = 0;
  priv->valid_column = 0;
  /* we want to avoid to paint the first time */
  priv->current_depth = MAX_DRAWABLE_DEPTH +1;
  priv->drawn_elements = 0;
  priv->current_init_angle = 0; 
  priv->current_final_angle = 2*G_PI;
  priv->real_sector = FALSE;
  priv->button_pressed = FALSE;
  priv->is_frozen = FALSE;
  priv->transparent_tips = FALSE;
  priv->subfoldertips_enabled = FALSE;
  priv->init_depth = 0;
  priv->draw_center = FALSE;
  priv->memento = NULL;
  priv->root = NULL;
}

static void
baobab_ringschart_dispose (GObject *object)
{
  BaobabRingschartPrivate *priv;

  priv = BAOBAB_RINGSCHART (object)->priv;

  if (priv->model)
    {
      baobab_ringschart_disconnect_signals (GTK_WIDGET (object),
					    priv->model);

      g_object_unref (priv->model);

      priv->model = NULL;
    }

  if (priv->root)
    {
      gtk_tree_row_reference_free (priv->root);

      priv->root = NULL;
    }

  (* G_OBJECT_CLASS (baobab_ringschart_parent_class)->dispose) (object);
}

static void
baobab_ringschart_realize (GtkWidget *widget)
{
  BaobabRingschart *ringc;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (BAOBAB_IS_RINGSCHART (widget));

  ringc = BAOBAB_RINGSCHART (widget);
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

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, ringc);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

  gtk_widget_add_events (widget,
                         GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | 
                         GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
}

static void
baobab_ringschart_size_allocate (GtkWidget     *widget,
                                 GtkAllocation *allocation)
{
  BaobabRingschartPrivate *priv;

  g_return_if_fail (BAOBAB_IS_RINGSCHART (widget));
  g_return_if_fail (allocation != NULL);

  priv = BAOBAB_RINGSCHART (widget)->priv;

  widget->allocation = *allocation;

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);
    }
  
  priv->center_x = widget->allocation.width / 2;  
  priv->center_y = widget->allocation.height / 2;
  priv->max_radius= MIN (widget->allocation.width / 2,
                         widget->allocation.height / 2) - WINDOW_PADDING*2;
  priv->min_radius= priv->max_radius / (priv->max_depth + 1);
  priv->thickness= priv->min_radius;
}

static void
baobab_ringschart_set_property (GObject         *object,
                                guint            prop_id,
                                const GValue    *value,
                                GParamSpec      *pspec)
{
  BaobabRingschart *rings_chart;

  rings_chart = BAOBAB_RINGSCHART (object);

  switch (prop_id)
    {
    case PROP_MAX_DEPTH:
      baobab_ringschart_set_max_depth (GTK_WIDGET (rings_chart), g_value_get_int (value));
      break;
    case PROP_MODEL:
      baobab_ringschart_set_model (GTK_WIDGET (rings_chart), g_value_get_object (value));
      break;
    case PROP_ROOT:
      baobab_ringschart_set_root (GTK_WIDGET (rings_chart), g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
baobab_ringschart_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  BaobabRingschartPrivate *priv;

  priv = BAOBAB_RINGSCHART (object)->priv;

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

static void 
baobab_ringschart_update_draw (BaobabRingschart* rchart,
                               GtkTreePath *path)
{
  BaobabRingschartPrivate *priv;
  GtkTreePath *root_path = NULL;
  gint root_depth, node_depth;

  if (!GTK_WIDGET_REALIZED (rchart))
    return;

  priv = BAOBAB_RINGSCHART (rchart)->priv;

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
      gtk_widget_queue_draw (GTK_WIDGET (rchart));
    }

  gtk_tree_path_free (root_path);
}

static void 
baobab_ringschart_row_changed (GtkTreeModel    *model,
                               GtkTreePath     *path,
                               GtkTreeIter     *iter,
                               gpointer         data)
{
  g_return_if_fail (BAOBAB_IS_RINGSCHART (data));
  g_return_if_fail (path != NULL || iter != NULL);

  baobab_ringschart_update_draw (BAOBAB_RINGSCHART (data), path);

}

static void 
baobab_ringschart_row_inserted (GtkTreeModel    *model,
                                GtkTreePath     *path,
                                GtkTreeIter     *iter,
                                gpointer         data)
{
  g_return_if_fail (BAOBAB_IS_RINGSCHART (data));
  g_return_if_fail (path != NULL || iter != NULL);
  
  baobab_ringschart_update_draw (BAOBAB_RINGSCHART (data), path);

}

static void 
baobab_ringschart_row_has_child_toggled (GtkTreeModel    *model,
                                         GtkTreePath     *path,
                                         GtkTreeIter     *iter,
                                         gpointer         data)
{
  g_return_if_fail (BAOBAB_IS_RINGSCHART (data));
  g_return_if_fail (path != NULL || iter != NULL);

  baobab_ringschart_update_draw (BAOBAB_RINGSCHART (data), path);

}

static void 
baobab_ringschart_row_deleted (GtkTreeModel    *model,
                               GtkTreePath     *path,
                               gpointer         data)
{
  g_return_if_fail (BAOBAB_IS_RINGSCHART (data));
  g_return_if_fail (path != NULL);
  
  baobab_ringschart_update_draw (BAOBAB_RINGSCHART (data), path);

}

static void 
baobab_ringschart_rows_reordered (GtkTreeModel    *model,
                                  GtkTreePath     *path,
                                  GtkTreeIter     *iter,
                                  gint            *new_order,
                                  gpointer         data)
{
  g_return_if_fail (BAOBAB_IS_RINGSCHART (data));
  g_return_if_fail (path != NULL || iter != NULL);

  baobab_ringschart_update_draw (BAOBAB_RINGSCHART (data), path);

}

static gboolean
baobab_ringschart_expose (GtkWidget *rchart, GdkEventExpose *event)
{
  cairo_t *cr;
  BaobabRingschartPrivate *priv;

  priv = BAOBAB_RINGSCHART (rchart)->priv;

  /* the columns are not set we paint nothing */
  if (priv->name_column == priv->percentage_column)
    return FALSE;

  /* get a cairo_t */
  cr = gdk_cairo_create (rchart->window);

  cairo_rectangle (cr,
                   event->area.x, event->area.y,
                   event->area.width, event->area.height);

  /* there is no model we can not paint */
  if ((priv->is_frozen) || (priv->model == NULL))
    {
      if (priv->memento != NULL)
        { 
          gint w, h;

          w = cairo_image_surface_get_width (priv->memento);
          h = cairo_image_surface_get_height (priv->memento);

          cairo_clip (cr);
          
          if (w > 0 && h > 0 &&
	      !(rchart->allocation.width == w &&
                rchart->allocation.height == h))
            {
              gdouble p, sx, sy;
	  
              /* minimal available proportion */
              p = MIN (rchart->allocation.width / (1.0 * w),
                       rchart->allocation.height / (1.0 * h));
              
              sx = (gdouble) (rchart->allocation.width - w * p) / 2.0;
              sy = (gdouble) (rchart->allocation.height - h * p) / 2.0;

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
      
      baobab_ringschart_draw (rchart, cr);
    }

  cairo_destroy (cr);

  return FALSE;
}

static void
baobab_ringschart_draw (GtkWidget *rchart, cairo_t *cr)
{
  BaobabRingschartPrivate *priv;
  GtkTreeIter initial_iter = {0};  
  GSList *tooltips = NULL;
  gdouble perc;
  gdouble max_angle;

  cairo_stroke (cr); 

  priv = BAOBAB_RINGSCHART (rchart)->priv;

  if (priv->root != NULL)
    {
      GtkTreePath *path = gtk_tree_row_reference_get_path (priv->root);
      if (path != NULL)
        {
          gtk_tree_model_get_iter (priv->model, &initial_iter, path);
          gtk_tree_path_free (path);
        }
      else 
        {
          gtk_tree_row_reference_free (priv->root);
          priv->root = NULL;
        }
    }

  if (priv->root == NULL)
    gtk_tree_model_get_iter_first (priv->model, &initial_iter);

  priv->drawn_elements = 0;
  priv->current_init_angle = 0;  
  priv->current_final_angle = 3*G_PI;
  priv->real_sector = FALSE;
  
  /* the tree has just one node */
  if (priv->init_depth == 0)     
    max_angle = 2*G_PI;
  else
    {
      gtk_tree_model_get (priv->model, &initial_iter, 
                          priv->percentage_column, &perc, -1);

      max_angle = 2*G_PI*perc*0.01;
    }

  if (priv->draw_center)
    draw_circle (cr, priv->center_x, priv->center_y, priv->min_radius, FALSE);

  tree_traverse(cr, BAOBAB_RINGSCHART (rchart), &initial_iter, 
                priv->init_depth, priv->min_radius, 0, max_angle, &tooltips, 
                !priv->button_pressed);

  if (priv->root)
    {
      /* the root should not be invalid ;-) */
      GtkTreePath *path = gtk_tree_row_reference_get_path (priv->root);
      gtk_tree_model_get_iter (priv->model, &initial_iter, path);
      gtk_tree_path_free (path);
    }

  /* no matter what happens in the draw now the button will be
     unpressed */
  priv->button_pressed = FALSE;
  
  if (priv->draw_tooltip)
    {
      draw_tooltips (rchart, cr, tooltips);
    }

}

static void 
draw_sector (cairo_t *cr, 
             gdouble center_x, 
             gdouble center_y,
             gdouble radius,
             gdouble thickness,
             gdouble init_angle,
             gdouble final_angle,
             Color fill_color,
             gboolean continued)
{
  cairo_save (cr);

  cairo_set_line_width (cr, 1);
  cairo_arc (cr, center_x, center_y, radius, init_angle, final_angle);
  cairo_arc_negative (cr, center_x, center_y, radius+thickness, final_angle, init_angle);

  cairo_close_path(cr);

  cairo_set_source_rgb (cr, fill_color.red, fill_color.green, fill_color.blue);
  cairo_fill_preserve (cr);
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_stroke (cr);

  if (continued)
    {
      cairo_set_line_width (cr, 3);
      cairo_arc (cr, center_x, center_y, radius+thickness+4, init_angle+EDGE_ANGLE, 
                 final_angle-EDGE_ANGLE); 
      cairo_stroke (cr); 
    }

  cairo_restore (cr);
}

static void 
draw_circle (cairo_t *cr, 
             gdouble center_x, 
             gdouble center_y,
             gdouble radius,
             gboolean highlighted)
{
  Color fill_color;
  
  get_color (&fill_color, 0, 0, highlighted);
  
  cairo_save (cr);

  cairo_set_line_width (cr, 1);
  cairo_arc (cr, center_x, center_y, radius, 0, 2*G_PI);

  cairo_set_source_rgb (cr, fill_color.red, fill_color.green, fill_color.blue);
  cairo_fill_preserve (cr);
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_stroke (cr);

  cairo_restore (cr);
}

static void
draw_tooltips (GtkWidget *rchart,
               cairo_t *cr,
               GSList *tooltips)
{
  BaobabRingschartPrivate *priv;

  priv = BAOBAB_RINGSCHART (rchart)->priv;

  /* FIXME: replace this list of tips with just a variable, we are not
     going to have more than one tooltip */
  if (tooltips)
    {
      do
        {
          GSList *tips_iter = NULL;
          Tooltip *tooltip = NULL;
          GtkTreeIter child  = {0};
          GtkTreeIter iter = {0};
          Tooltip_rectangle *previous_rectangle = NULL;
          GtkTreePath *path = NULL;

          tips_iter = tooltips;
          tooltip = tips_iter->data;

          path = gtk_tree_row_reference_get_path (tooltip->ref);
          gtk_tree_model_get_iter (priv->model, &iter, path);

          /* draw the child tooltips */
          if (gtk_tree_model_iter_has_child (priv->model, &iter) && 
              (tooltip->depth == -1 || tooltip->depth < priv->max_depth-1))
            {
              gdouble _init_angle, _final_angle;

              gtk_tree_model_iter_children (priv->model, &child, &iter);
              _init_angle = tooltip->init_angle;

              previous_rectangle = g_new (Tooltip_rectangle, 1);
              previous_rectangle->x = 0.0;
              previous_rectangle->y = 0.0;
              previous_rectangle->width = 0.0;
              previous_rectangle->height = 0.0;

              do
                {
                  gchar *name;
                  gchar *size_column;
                  gdouble percentage;

                  gtk_tree_model_get (priv->model, &child, 
                                      priv->name_column, &name,
                                      priv->size_column, &size_column,
                                      priv->percentage_column, &percentage, -1);             
                  
                  _final_angle = _init_angle + 
                    (tooltip->final_angle - tooltip->init_angle) * percentage/100;

                  if (percentage > 0)
                    draw_folder_tip (cr, rchart, _init_angle, 
                                     _final_angle, tooltip->depth+1, name, 
                                     size_column, previous_rectangle);                 

                  g_free (name);
                  g_free (size_column);
                          
                  _init_angle = _final_angle;

                } while (gtk_tree_model_iter_next (priv->model, &child));
            }

          gtk_tree_path_free (path);

          /* check if the root is highlihgted in order to redraw it*/
          if ((priv->draw_center) && (tooltip->depth == -1))
                  draw_circle (cr, priv->center_x, priv->center_y, priv->min_radius, TRUE);

          draw_tip (cr, rchart, tooltip->init_angle, tooltip->final_angle, 
                    tooltip->depth, tooltip->name, tooltip->size);

          g_free (tooltip->name);
          g_free (tooltip->size);
          gtk_tree_row_reference_free (tooltip->ref);

          tooltips = g_slist_remove_link (tooltips, tips_iter);

          g_free (tooltip);
          g_slist_free_1 (tips_iter);
        }
      while (g_slist_next(tooltips));
    }
}

static void 
draw_tip (cairo_t *cr,
          GtkWidget *rchart,
          gdouble init_angle,
          gdouble final_angle,
          gdouble depth,
          gchar *name,
          gchar *size)
{
  BaobabRingschartPrivate *priv;
  gdouble medium_angle;
  gdouble start_radius;
  gdouble max_radius;
  gdouble origin_x;
  gdouble origin_y;
  gdouble tooltip_x;
  gdouble tooltip_y;
  gdouble end_x;
  gdouble end_y;
  gdouble tooltip_height;
  gdouble tooltip_width;  
  PangoRectangle rect;
  PangoLayout *layout;
  gchar *text = NULL;

  priv = BAOBAB_RINGSCHART_GET_PRIVATE (rchart);

  /* angle to start drawing from */
  /* medium_angle = ((final_angle - init_angle)/2) + init_angle; */
  medium_angle = priv->point_angle;

  /* radius to start drawing from */
  start_radius = (depth != -1) ? (depth + 0.1) * priv->thickness + priv->min_radius : 0;

  /* coordinates of the starting point of the line */
  origin_x = priv->center_x + (cos(medium_angle) * start_radius);
  origin_y = priv->center_y + (sin(medium_angle) * start_radius);

  /* get the size of the text label */
  text = (size != NULL)
    ? g_strconcat (name, "\n", size, NULL)
    : g_strdup (name);

  layout = gtk_widget_create_pango_layout (rchart, NULL);
  pango_layout_set_markup (layout, text, -1);

  pango_layout_get_pixel_extents (layout, NULL, &rect);

  tooltip_width = floor(rect.width+TOOLTIP_PADDING*2);  
  tooltip_height = floor(rect.height+TOOLTIP_PADDING*2);

  /* TODO */
  tooltip_x = MAX (priv->pointer_x - tooltip_width, 0);
  tooltip_x = ((tooltip_x + tooltip_width) > rchart->allocation.width) ?
    rchart->allocation.width - tooltip_width : tooltip_x;
  tooltip_y = MAX (priv->pointer_y - tooltip_height, 0);
  tooltip_y = ((tooltip_y + tooltip_height) > rchart->allocation.height) ?
    rchart->allocation.height - tooltip_height : tooltip_y;

  /* avoid blurred lines */
  tooltip_x = round (tooltip_x) + 0.5;
  tooltip_y = round (tooltip_y) + 0.5;  

  /* Time to do the drawing stuff */
  cairo_save (cr);
  cairo_set_source_rgb (cr, 0, 0, 0);

  cairo_set_line_width (cr, 1);
  cairo_rectangle (cr, tooltip_x, tooltip_y, tooltip_width, tooltip_height);
  cairo_set_source_rgb (cr, 0.99, 0.91, 0.31); /* tango: fce94f */
  cairo_fill_preserve (cr);

  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_move_to (cr, tooltip_x+TOOLTIP_PADDING, tooltip_y+TOOLTIP_PADDING);
  pango_cairo_show_layout (cr, layout);

  cairo_stroke (cr);

  cairo_restore (cr);

  g_object_unref (layout);
  g_free (text);
}

/* this function returns the area (rectangle) filled by this
particular tooltip in previous_rectangle, so that the next tooltip to
be drawn can decide wether it has enough space or not; in case that
the tooltip can't be drawn, then the values of previous_rectangle
remain unchanged */
static void
draw_folder_tip (cairo_t *cr,
                 GtkWidget *rchart,
                 gdouble init_angle,
                 gdouble final_angle,
                 gdouble depth,
                 gchar *name,
                 gchar *size,
                 Tooltip_rectangle *previous_rectangle)
{
  BaobabRingschartPrivate *priv;
  gdouble medium_angle;
  gdouble start_radius;
  gdouble max_radius;
  gdouble origin_x;
  gdouble origin_y;
  gdouble max_radius_x;
  gdouble max_radius_y;
  gdouble tooltip_x;
  gdouble tooltip_y;
  gdouble end_x;
  gdouble end_y;
  gdouble tooltip_height;
  gdouble tooltip_width;  
  PangoRectangle rect;
  PangoRectangle ink_rect;
  PangoLayout *layout;
  gchar *text = NULL;
  gint i = 0;
  gboolean valid = FALSE;
  gdouble angle;
  gdouble *test_angles;
  gint n_test_angles;

  priv = BAOBAB_RINGSCHART_GET_PRIVATE (rchart);

  if (final_angle - init_angle < MIN_DRAWABLE_ANGLE)
    return;

  if (!priv->subfoldertips_enabled)
    {
      /* subfoldertips aren't enabled */ 
      return;
    }

  /* prepare the angles to study */
  angle = final_angle - init_angle;
  test_angles = NULL;
  n_test_angles = 0;

  if (angle < G_PI/18) /* less than 10 degrees */
    {
      n_test_angles = 1;
      test_angles = g_malloc (n_test_angles * sizeof (gdouble));
      test_angles[0] = 0.5;    
    }
  else if (angle < G_PI/4)
    {
      n_test_angles = 3;
      test_angles = g_malloc (n_test_angles * sizeof (gdouble));
      test_angles[0] = 0.5;
      test_angles[1] = 0.20;
      test_angles[2] = 0.80;
    }
  else if (angle < G_PI/2)
    {
      n_test_angles = 5;
      test_angles = g_malloc (n_test_angles * sizeof (gdouble));
      test_angles[0] = 0.5;
      test_angles[1] = 0.33;
      test_angles[2] = 0.67; 
      test_angles[3] = 0.17;
      test_angles[4] = 0.83;
    } 
  else if (angle < 3*G_PI/4)
    {
      n_test_angles = 7;
      test_angles = g_malloc (n_test_angles * sizeof (gdouble));
      test_angles[0] = 0.5; 
      test_angles[1] = 0.375;
      test_angles[2] = 0.625; 
      test_angles[3] = 0.25; 
      test_angles[4] = 0.75;
      test_angles[5] = 0.125;
      test_angles[6] = 0.875;
    }
  else 
    {
      n_test_angles = 9;
      test_angles = g_malloc (n_test_angles * sizeof (gdouble));
      test_angles[0] = 0.5; 
      test_angles[1] = 0.4;
      test_angles[2] = 0.6; 
      test_angles[3] = 0.3; 
      test_angles[4] = 0.7;
      test_angles[5] = 0.2;
      test_angles[6] = 0.8;
      test_angles[7] = 0.1; 
      test_angles[8] = 0.9;
    }

  /* get the size of the text label */
  /* text = (size != NULL) ? g_strconcat (name, "\n", size, NULL) : g_strdup (name); */
  /* only the name is shown */
  text = g_strdup(name);
  text = g_strconcat("<span size=\"small\">", text, "</span>", NULL);
  
  layout = gtk_widget_create_pango_layout (rchart, NULL);
  pango_layout_set_markup (layout, text, -1);
  
  pango_layout_set_indent (layout, 0);
  pango_layout_set_spacing (layout, 0);
  
  pango_layout_get_pixel_extents (layout, &ink_rect, &rect);
  
  tooltip_width = rect.width + TOOLTIP_PADDING*2;
  tooltip_height = rect.height + TOOLTIP_PADDING*2;
  
  /* radius to start drawing from */
  start_radius = (depth != -1) ? (depth + 0.5) * priv->thickness + priv->min_radius : 0;

  /* try a maximum of n_test_angles different angles */
  while ( i < n_test_angles && !valid)
    {
      gint closest_x, closest_y;

      /* angle to start drawing from */
      medium_angle = angle*test_angles[i] + init_angle;

      i++;

      /* coordinates of the starting point of the line */
      origin_x = priv->center_x + (cos (medium_angle) * start_radius);
      origin_y = priv->center_y + (sin (medium_angle) * start_radius);

      /* Horizontal radius */
      max_radius_x = ABS ((priv->center_x - (tooltip_width/2 + WINDOW_PADDING)) / cos (medium_angle));

      /* and vertical one */
      max_radius_y = ABS ((priv->center_y - (tooltip_height/2 + WINDOW_PADDING)) / sin (medium_angle));

      /* get the minor radius */
      max_radius = MIN (max_radius_x, max_radius_y);

      /* there is an alternative way */
      /* max_radius = MIN( (priv->center_x - (tooltip_width/2 + WINDOW_PADDING)), (priv->center_y - (tooltip_height/2 + WINDOW_PADDING)) ); */

      /* now we get the final coordinates of the line */
      end_x = priv->center_x + (max_radius * cos (medium_angle));
      end_y = priv->center_y + (max_radius * sin (medium_angle));

      /* and the position on the tip */
      tooltip_x = end_x - tooltip_width*0.5;
      tooltip_y = end_y - tooltip_height*0.5;

      /* coordinates of the closest point to the center */
      closest_x = (ABS (priv->center_x - tooltip_x) < ABS (priv->center_x - tooltip_x - tooltip_width))? tooltip_x : tooltip_x + tooltip_width;
      closest_y = (ABS (priv->center_y - tooltip_y) < ABS (priv->center_y - tooltip_y - tooltip_height))? tooltip_y : tooltip_y + tooltip_height;

      /* check for overlaps: */
      if ((((tooltip_x <= previous_rectangle->x) && (tooltip_x + tooltip_width >= previous_rectangle->x) ||
          (tooltip_x >= previous_rectangle->x) && (tooltip_x <= previous_rectangle->x + previous_rectangle->width))
          &&
          ((tooltip_y <= previous_rectangle->y) && (tooltip_y + tooltip_height >= previous_rectangle->y) ||
              (tooltip_y >= previous_rectangle->y) && (tooltip_y <= previous_rectangle->y + previous_rectangle->height)))
              ||
              /* intersection between the rectangle and the circle at current_depth (or better, current_depth+1) */
              ( sqrt(pow(priv->center_x - closest_x, 2) + pow(priv->center_y - closest_y, 2)) < ((depth + 0.1) * priv->thickness + priv->min_radius))
              ||
              /* tooltip's width greater than half of the widget's */
              ( tooltip_width > priv->center_x))
        {
          continue;
        }
      else 
        {
          previous_rectangle->x = tooltip_x;
          previous_rectangle->y = tooltip_y;
          previous_rectangle->width = tooltip_width+TOOLTIP_SPACING;
          previous_rectangle->height = tooltip_height+TOOLTIP_SPACING;
          valid = TRUE;
        }
    }

  g_free (text);
  g_free (test_angles);
  
  if (!valid)
    {
      g_object_unref (layout);
      return;
    }

  /* Time to the drawing stuff */
  cairo_save (cr);
  
  if ((origin_x < tooltip_x) || (origin_x > tooltip_x + tooltip_width)
      || (origin_y < tooltip_y) || (origin_y > tooltip_y + tooltip_height))
    {
      gdouble t_h;
      gdouble t_w;
      gdouble c_x;
      gdouble c_y;
      gdouble t_x;
      gdouble t_y;
      gdouble x_intersection;
      gdouble y_intersection;

      cairo_set_line_width (cr, 1);
      cairo_move_to(cr, origin_x, origin_y);
      /* we have to calculate the intersection point */
      /* the space is divided in four sectors, like an X or so, and on
         each one of those the line intersects a different edge of the
         rectangle */
      /* it's quite likely that the following code can be greatly simplified */
      t_h = (gdouble)tooltip_height;
      t_w = (gdouble)tooltip_width;
      /* center of the tooltip */
      c_x = (gdouble)(end_x - priv->center_x);
      c_y = (gdouble)(priv->center_y - end_y);
      /* top left corner of the tooltip */
      t_x = (gdouble)(tooltip_x - priv->center_x);
      t_y = (gdouble)(priv->center_y - tooltip_y);
      /* intersection point */
      x_intersection = c_x;
      y_intersection = c_y;

      if (c_x > 0) {
        if (c_y > 0) 
          { /* top right */
            /* does it intersect the bottom edge? y = 
               t_y - tooltip_height  :  x_intersection = 
               (c_x/c_y)*(t_y - tooltip_height) */
            /* or the left one? x = t_x  :  y_intersection = (c_y/c_x)*(t_x) */
            gdouble y_left_intersection = (c_y/c_x)*(t_x);

            if ((y_left_intersection <= t_y) && (y_left_intersection > t_y - t_h)) 
              {  /* left edge */
                x_intersection = t_x;
                y_intersection = y_left_intersection;
              } 
            else 
              { /* bottom edge */
                y_intersection = t_y - t_h;
                x_intersection = (c_x/c_y)*(t_y - t_h);
              }
          } 
        else 
          { /* bottom right */
            /* top : y = t_y  ;  x = (c_x/c_y)*t_y */
            /* left :  x = t_x  ;  y_intersection = (c_y/c_x)*(t_x) */
            gdouble y_left_intersection = (c_y/c_x)*(t_x);

            if ((y_left_intersection <= t_y) && (y_left_intersection > t_y - t_h)) 
              {  /* left edge */
                x_intersection = t_x;
                y_intersection = y_left_intersection;
              } 
            else 
              { /* bottom edge */
                y_intersection = t_y;
                x_intersection = (c_x/c_y)*(t_y);
              }
          }
      }
      else  
        { 
          if (c_y > 0) 
            {/* top left */
              /* right edge : x = t_x + t_w ; y = (c_y/c_x)*(t_x + t_w) */
              /* bottom :  y = t_y - t_h  :  x = (c_x/c_y)*(t_y + t_h) */
              gdouble y_right_intersection = (c_y/c_x)*(t_x + t_w);

              if ((y_right_intersection <= t_y) && (y_right_intersection > t_y - t_h)) 
                {
                  x_intersection = t_x + t_w;
                  y_intersection = y_right_intersection;
                } 
              else 
                {
                  x_intersection =  (c_x/c_y)*(t_y - t_h);
                  y_intersection = t_y - t_h;
                }
            } 
          else 
            { /* bottom left */
              /* top : y = t_y  ;  x = (c_x/c_y)*t_y */
              /* right :  x = t_x + t_w  ;  y_intersection = (c_y/c_x)*(t_x + t_w) */
              gdouble y_left_intersection = (c_y/c_x)*(t_x + t_w);

              if ((y_left_intersection <= t_y) && (y_left_intersection > t_y - t_h)) 
                {  /* left edge */
                  x_intersection = t_x + t_w;
                  y_intersection = y_left_intersection;
                } 
              else 
                { /* bottom edge */
                  y_intersection = t_y;
                  x_intersection = (c_x/c_y)*(t_y);
                }
            }
        }

      end_x = (gint)((gdouble)priv->center_x + x_intersection);
      end_y = (gint)((gdouble)priv->center_y - y_intersection);

      cairo_set_source_rgb (cr, 0.8275, 0.8431, 0.8118); /* tango: #d3d7cf */
      cairo_line_to (cr, end_x, end_y);      
      cairo_stroke (cr);

      cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.6666 );
      cairo_arc (cr, origin_x, origin_y, 1.0, 0, 2*G_PI );
      cairo_stroke (cr);
    }
  
  cairo_set_line_width (cr, 0.5);

  /* avoid blurred lines */
  tooltip_x = round (tooltip_x) + 0.5;
  tooltip_y = round (tooltip_y) + 0.5;   

  cairo_rectangle (cr, tooltip_x, tooltip_y, tooltip_width, tooltip_height);
  cairo_set_source_rgb (cr, 0.8275, 0.8431, 0.8118);  /* tango: #d3d7cf */
  cairo_fill_preserve(cr);
  cairo_set_source_rgb (cr, 0.5333, 0.5412, 0.5216);  /* tango: #888a85 */
  cairo_stroke (cr);

  cairo_move_to (cr, tooltip_x+TOOLTIP_PADDING-PANGO_LBEARING (ink_rect), tooltip_y+TOOLTIP_PADDING);

  cairo_set_source_rgb (cr, 0, 0, 0);
  pango_cairo_show_layout (cr, layout);
  cairo_set_line_width (cr, 1);
  cairo_stroke (cr);
  cairo_restore (cr);

  g_object_unref (layout);
}

static void
interpolate_colors (Color *color,
                    Color colora,
                    Color colorb,
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

static void
get_color (Color *color,
           gdouble angle,
      	   gint circle_number,
      	   gboolean highlighted)
{
  gdouble intensity;
  gint color_number;
  gint next_color_number;

  intensity = 1-(((circle_number-1)*0.3)/MAX_DRAWABLE_DEPTH);

  if (circle_number == 0)
    {
      static const Color circle_color = {0.83, 0.84, 0.82};

      *color = circle_color;
    }
  else
    {
      color_number = angle / (G_PI/3);
      next_color_number = (color_number+1) % 6;

      interpolate_colors (color,
                          tango_colors[color_number],
                          tango_colors[next_color_number],
                          (angle-color_number*G_PI/3)/(G_PI/3));
      color->red = color->red * intensity;
      color->green = color->green * intensity;
      color->blue = color->blue * intensity;
    }
  
  if (highlighted)
    {
      gdouble maximum;
      
      maximum = MAX (color->red,
                     MAX (color->green,
                          color->blue));
      color->red /= maximum;
      color->green /= maximum;
      color->blue /= maximum;
    }
}

static void
tree_traverse (cairo_t *cr,
               BaobabRingschart *rchart,
               GtkTreeIter *iter,
               guint depth,
               gdouble radius,
               gdouble init_angle,
               gdouble final_angle,
               GSList **tooltips,
               gboolean real_draw)
{
  BaobabRingschartPrivate *priv;
  Color fill_color;
  gboolean highlighted = FALSE;

  priv = BAOBAB_RINGSCHART (rchart)->priv;

  if (final_angle - init_angle < MIN_DRAWABLE_ANGLE)
    return;
      
  if ((priv->current_depth == depth) && (priv->current_final_angle == 3*G_PI))
    {
      if (priv->point_angle < final_angle)
        {
          if (priv->point_angle >= init_angle)
            {
              GtkTreePath *path;
              GtkTreeRowReference *ref;
              
              path = gtk_tree_model_get_path (priv->model, iter);
              ref = gtk_tree_row_reference_new (priv->model, path);

              if (priv->button_pressed)
                {
                  if (priv->root) 
                    gtk_tree_row_reference_free (priv->root);

                  priv->root = ref;
                  priv->button_pressed = FALSE;
                  real_draw = TRUE;
                  depth = priv->init_depth;
                  radius = priv->min_radius;
                  init_angle = 0;
                  final_angle = 2*G_PI;
                  
                  g_signal_emit (BAOBAB_RINGSCHART (rchart),
                                 ringschart_signals[SECTOR_ACTIVATED],
                                 0, iter);
                }
              else 
                {
                  gchar *name;
                  gchar *size_column;
                  Tooltip *tip = g_new0 (Tooltip, 1);
             
                  priv->current_init_angle = init_angle;
                  priv->current_final_angle = final_angle;
                  priv->real_sector = depth >  priv->init_depth;
 
                  gtk_tree_model_get (priv->model, iter, 
                                      priv->name_column, &name,
                                      priv->size_column, &size_column, -1);             

                  tip->init_angle = init_angle;
                  tip->final_angle = final_angle;
                  tip->depth = depth-1;
                  tip->name = name;
                  tip->size = size_column;
                  tip->ref = ref;
                  
                  *tooltips = g_slist_prepend (*tooltips , tip);
                  highlighted = TRUE;
                }

              gtk_tree_path_free (path);
            }
          else 
            priv->current_final_angle = init_angle;            
        }
      else
        priv->current_init_angle = final_angle;
    }

  if (real_draw)
    {
      if ((depth != 0)||(priv->init_depth>0))
        {
          gboolean has_child_max_depth;

          /* FIXME: there is a problem when we are not in the
             max_depth but we are not going to paint any child because
             they are too small, the sector will not have a line but
             it should */
          has_child_max_depth = gtk_tree_model_iter_has_child (priv->model, iter) 
            && (depth >= priv->max_depth);

          get_color (&fill_color, init_angle, depth, highlighted);
          draw_sector(cr, priv->center_x, priv->center_y, radius, priv->thickness, 
                      init_angle, final_angle, fill_color, has_child_max_depth);

      	  /* Update drawn elements count */
      	  priv->drawn_elements++;
        }
    }

  if (gtk_tree_model_iter_has_child (priv->model, iter) && (depth < priv->max_depth))
    {
      GtkTreeIter child  = {0};
      gdouble _init_angle, _final_angle;

      gtk_tree_model_iter_children (priv->model, &child, iter);

      _init_angle = init_angle;

      do
        {
          gdouble size;
          guint next_radius;

          next_radius = depth == 0 ? radius : radius+priv->thickness;

          gtk_tree_model_get (priv->model, &child,
                              priv->percentage_column, &size, -1);

          if (size >= 0)
            {
              _final_angle = _init_angle + (final_angle - init_angle) * size/100;
              
              tree_traverse (cr, rchart, &child, depth+1, next_radius, 
                             _init_angle, _final_angle, 
                             tooltips, real_draw);

              highlighted = TRUE;
            }

          _init_angle = _final_angle;
        }
      while (gtk_tree_model_iter_next (priv->model, &child));
    }
}

static gint 
baobab_ringschart_button_release (GtkWidget *widget,
                                  GdkEventButton *event)
{
  BaobabRingschartPrivate *priv;
  GtkTreeIter parent_iter;

  priv = BAOBAB_RINGSCHART (widget)->priv;

  switch (event->button)
    {      
    case LEFT_BUTTON:
      /* Enter into a subdir */
      if (priv->real_sector)
	{
	  priv->button_pressed = TRUE;		  

	  gtk_widget_queue_draw (widget);
	}   	  
      break;
    case MIDDLE_BUTTON:
      /* Go back to the parent dir */

      if (priv->root != NULL) 
	{
          GtkTreePath *path = gtk_tree_row_reference_get_path (priv->root);
          GtkTreeIter root_iter;

          if (path != NULL)
            gtk_tree_model_get_iter (priv->model, &root_iter, path);
          else
            return FALSE;

	  if (gtk_tree_model_iter_parent (priv->model, &parent_iter, &root_iter)) 
	    {
              gint valid;
              GtkTreePath *parent_path;

              gtk_tree_model_get (priv->model, &parent_iter, priv->valid_column,
                                  &valid, -1);

              if (valid == -1)
                return FALSE;
              
              gtk_tree_row_reference_free (priv->root);
              parent_path = gtk_tree_model_get_path (priv->model, &parent_iter);
              priv->root = gtk_tree_row_reference_new (priv->model, parent_path);
              gtk_tree_path_free (parent_path);

	      g_signal_emit (BAOBAB_RINGSCHART (widget),
			     ringschart_signals[SECTOR_ACTIVATED],
			     0, &parent_iter);
            
              gtk_widget_queue_draw (widget);
	    }

          gtk_tree_path_free (path);
	}
      break;
    case RIGHT_BUTTON: 
      break;
    }

  return FALSE;
}

static gint 
baobab_ringschart_scroll (GtkWidget *widget,
                          GdkEventScroll *event)
{
  guint current_depth = baobab_ringschart_get_max_depth (widget);

  switch (event->direction)
    {
    case GDK_SCROLL_LEFT :
    case GDK_SCROLL_UP :
      baobab_ringschart_set_max_depth (widget, current_depth + 1);
      break;
    case GDK_SCROLL_RIGHT :
    case GDK_SCROLL_DOWN :
      baobab_ringschart_set_max_depth (widget, current_depth - 1);
      break;
    }

  /* change the selected sector when zooming */
  baobab_ringschart_motion_notify (widget, (GdkEventMotion *)event);

  return FALSE;
}

gboolean
baobab_ringschart_no_pointer_motion (GtkWidget *widget)
{
  BaobabRingschartPrivate *priv;
  
  priv = BAOBAB_RINGSCHART (widget)->priv;
  
  /* At this function we are sure the mouse pointer didn't move during
     TOOLTIP_TIMEOUT miliseconds, so we redraw the chart, asking to
     draw the tooltip also. */
  priv->draw_tooltip = TRUE;

  gtk_widget_queue_draw (widget);

  /* It's important to return FALSE to prevent the timeout from
     executing again */
  return FALSE;
}

static gint 
baobab_ringschart_motion_notify (GtkWidget        *widget,
                                 GdkEventMotion   *event)
{
  BaobabRingschartPrivate *priv;
  gdouble angle;
  gdouble x, y;
  guint depth;
  guint old_depth;
  gboolean redraw = FALSE;

  priv = BAOBAB_RINGSCHART (widget)->priv;
  
  /* First remove a previous timeout that never fired, if any */
  if (priv->timeout_event_source > 0)
    {
      g_source_remove (priv->timeout_event_source);
    }
      
  /* We will redraw the chart here only in one of these situations:

     1) If the tooltip window is being shown, to hide it (our
     behavior is "hide tooltip while moving pointer")

     2) If the mouse pointer is inside the maximun depth
     (priv->max_depth) of the chart and if it hasn't move out
     from the current sector */
  
  /* Situation 1 */
  if (priv->draw_tooltip)
    {
      priv->draw_tooltip = FALSE; /* To avoid drawing the tooltip in the
                                     chart redraw */
      redraw = TRUE;
      
      /* Add a new timeout for the tooltip */
      priv->timeout_event_source = 
        g_timeout_add (TOOLTIP_TIMEOUT,
                       (GSourceFunc) baobab_ringschart_no_pointer_motion,
                       widget);
    }
  else
    {
      /* Situation 2  */

      /* We need to calculate depth and angle first */
      priv->pointer_x = event->x;
      priv->pointer_y = event->y;

      x = priv->pointer_x - priv->center_x;
      y = priv->pointer_y - priv->center_y;

      /* FIXME: could we use cairo_in_stroke or cairo_in_fill? would it be
         a better solution */
      angle = atan2 (y, x);
      angle = (angle > 0) ? angle : angle + 2*G_PI;

      depth = sqrt (x*x + y*y) / (priv->thickness);

      old_depth = priv->current_depth; 
      priv->current_depth = depth;
      priv->point_angle = angle;

      if (depth <= priv->max_depth) {

        /* At this point we know the mouse pointer is potentially over a
           sector, so let's start the timeout to show the tooltip */

        /* Add a new timeout for the tooltip */
        priv->timeout_event_source = 
          g_timeout_add (TOOLTIP_TIMEOUT,
                         (GSourceFunc) baobab_ringschart_no_pointer_motion,
                         widget);

        /* Now we check we moved out from current sector */
        if ((depth != old_depth) || (angle < priv->current_init_angle) || 
            (angle > priv->current_final_angle))
          {
            redraw = TRUE;
          }
      }
    }

  /* Ask redrawing if so stated */
  if (redraw)
    {
      gtk_widget_queue_draw (widget);
    }

  return FALSE;
}

static inline void
baobab_ringschart_disconnect_signals (GtkWidget *rchart, 
                                      GtkTreeModel *model)
{
  g_signal_handlers_disconnect_by_func (model,
                                        baobab_ringschart_row_changed,
                                        rchart);
  g_signal_handlers_disconnect_by_func (model,
                                        baobab_ringschart_row_inserted,
                                        rchart);
  g_signal_handlers_disconnect_by_func (model,
                                        baobab_ringschart_row_has_child_toggled,
                                        rchart);
  g_signal_handlers_disconnect_by_func (model,
                                        baobab_ringschart_row_deleted,
                                        rchart);
  g_signal_handlers_disconnect_by_func (model,
                                        baobab_ringschart_rows_reordered,
                                        rchart);     
}

static inline void
baobab_ringschart_connect_signals (GtkWidget *rchart,
                                   GtkTreeModel *model)
{
  g_signal_connect (model,
                    "row_changed",
                    G_CALLBACK (baobab_ringschart_row_changed),
                    rchart);
  g_signal_connect (model,
                    "row_inserted",
                    G_CALLBACK (baobab_ringschart_row_inserted),
                    rchart);
  g_signal_connect (model,
                    "row_has_child_toggled",
                    G_CALLBACK (baobab_ringschart_row_has_child_toggled),
                    rchart);
  g_signal_connect (model,
                    "row_deleted",
                    G_CALLBACK (baobab_ringschart_row_deleted),
                    rchart);
  g_signal_connect (model,
                    "rows_reordered",
                    G_CALLBACK (baobab_ringschart_rows_reordered),
                    rchart);
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
 * baobab_ringschart_set_model_with_columns:
 * @rchart: the #BaobabRingschart whose model is going to be set
 * @model: the #GtkTreeModel which is going to set as the model of
 * @rchart
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
 * Sets @model as the #GtkTreeModel used by @rchart. Indicates the
 * columns inside @model where the values file name, file
 * size, file information, disk usage percentage and data correction are stored, and
 * the node which will be used as the root of @rchart.  Once
 * the model has been successfully set, a redraw of the window is
 * forced.
 * This function is intended to be used the first time a #GtkTreeModel
 * is assigned to @rchart, or when the columns containing the needed data
 * are going to change. In other cases, #baobab_ringschart_set_model should
 * be used.
 * This function does not change the state of the signals from the model, which
 * is controlled by he #baobab_ringschart_freeze_updates and the 
 * #baobab_ringschart_thaw_updates functions. 
 * 
 * Fails if @rchart is not a #BaobabRingschart or if @model is not a
 * #GtkTreeModel.
 **/
void 
baobab_ringschart_set_model_with_columns (GtkWidget *rchart, 
                                          GtkTreeModel *model,
                                          guint name_column,
                                          guint size_column,
                                          guint info_column,
                                          guint percentage_column,
                                          guint valid_column,
                                          GtkTreePath *root)
{
  BaobabRingschartPrivate *priv;

  g_return_if_fail (BAOBAB_IS_RINGSCHART (rchart));
  g_return_if_fail (GTK_IS_TREE_MODEL (model));

  priv = BAOBAB_RINGSCHART (rchart)->priv;

  baobab_ringschart_set_model (rchart, model);
  
  if (root != NULL)
    {
      priv->root = gtk_tree_row_reference_new (model, root);
      g_object_notify (G_OBJECT (rchart), "root");
    }

  priv->name_column = name_column;
  priv->size_column = size_column;
  priv->info_column = info_column;
  priv->percentage_column = percentage_column;
  priv->valid_column = valid_column;
}

/**
 * baobab_ringschart_set_model:
 * @rchart: the #BaobabRingschart whose model is going to be set
 * @model: the #GtkTreeModel which is going to set as the model of
 * @rchart
 *
 * Sets @model as the #GtkTreeModel used by @rchart, and takes the needed
 * data from the columns especified in the last call to 
 * #baobab_ringschart_set_model_with_colums.
 * This function does not change the state of the signals from the model, which
 * is controlled by he #baobab_ringschart_freeze_updates and the 
 * #baobab_ringschart_thaw_updates functions. 
 * 
 * Fails if @rchart is not a #BaobabRingschart or if @model is not a
 * #GtkTreeModel.
 **/
void 
baobab_ringschart_set_model (GtkWidget *rchart,
                             GtkTreeModel *model)
{
  BaobabRingschartPrivate *priv;

  g_return_if_fail (BAOBAB_IS_RINGSCHART (rchart));
  g_return_if_fail (GTK_IS_TREE_MODEL (model));

  priv = BAOBAB_RINGSCHART (rchart)->priv;

  if (model == priv->model)
    return;

  if (priv->model)
    {
      if (!priv->is_frozen)
        baobab_ringschart_disconnect_signals (rchart, 
                                              priv->model);
      g_object_unref (priv->model);
    }

  priv->model = model;
  g_object_ref (priv->model);

  if (!priv->is_frozen)
    baobab_ringschart_connect_signals (rchart, 
                                       priv->model);

  if (priv->root)
    {
      gtk_tree_row_reference_free (priv->root);
    }

  priv->root = NULL;

  g_object_notify (G_OBJECT (rchart), "model");

  gtk_widget_queue_draw (rchart);
}

/**
 * baobab_ringschart_get_model:
 * @rchart: a #BaobabRingschart whose model will be returned.
 *
 * Returns the #GtkTreeModel which is the model used by @rchart.
 *
 * Returns: %NULL if @rchart is not a #BaobabRingschart.
 **/ 
GtkTreeModel *
baobab_ringschart_get_model (GtkWidget *rchart)
{
  g_return_val_if_fail (BAOBAB_IS_RINGSCHART (rchart), NULL);

  return BAOBAB_RINGSCHART (rchart)->priv->model;
}

/**
 * baobab_ringschart_set_max_depth:
 * @rchart: a #BaobabRingschart 
 * @max_depth: the new maximum depth to show in the widget.
 *
 * Sets the maximum number of rings that are going to be show in the
 * wigdet, and causes a redraw of the widget to show the new maximum
 * depth. If max_depth is < 1 MAX_DRAWABLE_DEPTH is used.
 *
 * Fails if @rchart is not a #BaobabRingschart.
 **/
void 
baobab_ringschart_set_max_depth (GtkWidget *rchart, 
                                 guint max_depth)
{
  BaobabRingschartPrivate *priv;

  g_return_if_fail (BAOBAB_IS_RINGSCHART (rchart));

  priv = BAOBAB_RINGSCHART (rchart)->priv;

  if (max_depth != priv->max_depth)
    {
      priv->max_depth = MAX (1, MIN (max_depth, MAX_DRAWABLE_DEPTH));
      priv->max_radius = MIN (rchart->allocation.width / 2,
                              rchart->allocation.height / 2) - WINDOW_PADDING*2;
      priv->min_radius = priv->max_radius / (priv->max_depth + 1);
      priv->thickness = priv->min_radius;
      g_object_notify (G_OBJECT (rchart), "max-depth");

      priv->draw_tooltip = FALSE;

      gtk_widget_queue_draw (rchart);
    }
}

/**
 * baobab_ringschart_get_max_depth:
 * @rchart: a #BaobabRingschart.
 *
 * Returns the maximum number of circles that will be show in the
 * widget.
 *
 * Fails if @rchart is not a #BaobabRingschart.
 **/
guint
baobab_ringschart_get_max_depth (GtkWidget *rchart)
{
  g_return_val_if_fail (BAOBAB_IS_RINGSCHART (rchart), 0);

  return BAOBAB_RINGSCHART (rchart)->priv->max_depth;
}

/**
 * baobab_ringschart_set_root:
 * @rchart: a #BaobabRingschart 
 * @root: a #GtkTreePath indicating the node which will be used as
 * the widget root.
 *
 * Sets the node pointed by @root as the new root of the widget
 * @rchart.
 *
 * Fails if @rchart is not a #BaobabRingschart or if @rchart has not
 * a #GtkTreeModel set.
 **/
void 
baobab_ringschart_set_root (GtkWidget *rchart, 
                            GtkTreePath *root)
{
  BaobabRingschartPrivate *priv;

  g_return_if_fail (BAOBAB_IS_RINGSCHART (rchart));

  priv = BAOBAB_RINGSCHART (rchart)->priv;

  g_return_if_fail (priv->model != NULL);

  if (priv->root) 
    gtk_tree_row_reference_free (priv->root);
    
  priv->root = gtk_tree_row_reference_new (priv->model, root);
  
  g_object_notify (G_OBJECT (rchart), "root");

  gtk_widget_queue_draw (rchart); 
}

/**
 * baobab_ringschart_get_root:
 * @rchart: a #BaobabRingschart.
 *
 * Returns a #GtkTreePath pointing to the root of the widget. The
 * programmer has the responsability to free the used memory once
 * finished with the returned value. It returns NULL if there is no
 * root node defined
 *
 * Fails if @rchart is not a #BaobabRingschart.
 **/
GtkTreePath *
baobab_ringschart_get_root (GtkWidget *rchart)
{
  g_return_val_if_fail (BAOBAB_IS_RINGSCHART (rchart), NULL);

  if (BAOBAB_RINGSCHART (rchart)->priv->root) 
    return gtk_tree_row_reference_get_path (BAOBAB_RINGSCHART (rchart)->priv->root);
  else
    return NULL;
}

/**
 * baobab_ringschart_get_drawn_elements:
 * @rchart: a #BaobabRingschart.
 *
 * Returns the number of sectors (folders) that are been shown
 * by the @rchart with its current maximum depth.
 *
 * Fails if @rchart is not a #BaobabRingschart.
 **/
gint 
baobab_ringschart_get_drawn_elements (GtkWidget *rchart)
{
  g_return_val_if_fail (BAOBAB_IS_RINGSCHART (rchart), 0);

  return BAOBAB_RINGSCHART (rchart)->priv->drawn_elements;  
}

/**
 * baobab_ringschart_freeze_updates:
 * @rchart: the #BaobabRingschart whose model signals are going to be frozen.
 *
 * Disconnects @rchart from the signals emitted by its model, and sets
 * the window of @rchart to a "processing" state, so that the window 
 * ignores changes in the ringschart's model and mouse events.
 * In order to connect again the window to the model, a call to 
 * #baobab_ringschart_thaw_updates must be done. 
 *
 * Fails if @rchart is not a #BaobabRingschart.
 **/
void 
baobab_ringschart_freeze_updates (GtkWidget *rchart)
{
  BaobabRingschartPrivate *priv;

  g_return_if_fail (BAOBAB_IS_RINGSCHART (rchart));

  priv = BAOBAB_RINGSCHART (rchart)->priv;

  if (!priv->is_frozen) 
    {
      cairo_surface_t *surface = NULL;
      cairo_t *cr = NULL;

      if (priv->model)
        baobab_ringschart_disconnect_signals (rchart, 
                                              priv->model);

      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                            rchart->allocation.width, 
                                            rchart->allocation.height);

      if (cairo_surface_status (surface) == CAIRO_STATUS_SUCCESS)
        {
          cr = cairo_create (surface);

          /* we want to avoid the draw of the tips */
          priv->current_depth = priv->max_depth+1;

          baobab_ringschart_draw (rchart, cr);

          cairo_rectangle (cr,
                           0, 0,
                           rchart->allocation.width,
                           rchart->allocation.height);

          cairo_set_source_rgba (cr, 0.93, 0.93, 0.93, 0.5);  /* tango: eeeeec */
          cairo_fill_preserve (cr);

          cairo_clip (cr);
          
          priv->memento = surface;

          cairo_destroy (cr);

        }

      priv->is_frozen = TRUE;
      gtk_widget_queue_draw (rchart); 
    }
}

/**
 * baobab_ringschart_thaw_updates:
 * @rchart: the #BaobabRingschart whose model signals are frozen.
 *
 * Reconnects @rchart to the signals emitted by its model, which
 * were disconnected through a call to #baobab_ringschart_freeze_updates.
 * Takes the window out of its "processing" state and forces a redraw
 * of the widget.
 *
 * Fails if @rchart is not a #BaobabRingschart.
 **/
void 
baobab_ringschart_thaw_updates (GtkWidget *rchart)
{
  BaobabRingschartPrivate *priv;

  g_return_if_fail (BAOBAB_IS_RINGSCHART (rchart));

  priv = BAOBAB_RINGSCHART (rchart)->priv;

  if (priv->is_frozen)
    {
      if (priv->model)
        baobab_ringschart_connect_signals (rchart,
                                           priv->model);

      if (priv->memento)
        {
          cairo_surface_destroy (priv->memento);
          priv->memento = NULL;
        }

      priv->is_frozen = FALSE;
      gtk_widget_queue_draw (rchart); 
    }
}

/**
 * baobab_ringschart_set_init_depth:
 * @rchart: the #BaobabRingschart widget.
 * @depth: the depth we want the widget to paint the root node
 *
 * This function allows the developer to change the level we want to
 * paint the root node, in other words, if we want to paint the root
 * level in the center we have to set depth to 0. The default value is
 * 0.
 **/
void 
baobab_ringschart_set_init_depth (GtkWidget *rchart, 
                                  guint depth)
{  
  BaobabRingschartPrivate *priv;
  
  g_return_if_fail (BAOBAB_IS_RINGSCHART (rchart));

  priv = BAOBAB_RINGSCHART (rchart)->priv;

  priv->init_depth = depth;
  gtk_widget_queue_draw (rchart); 
}

/**
 * baobab_ringschart_draw_center:
 * @rchart: the #BaobabRingschart widget.
 * @draw_center: TRUE if you want the widget to paint the center
 * circle, no matter the selected init depth.
 *
 * If the draw_center is TRUE the widget will paint the center circle
 * even when you start painting in a level over the 0.
 **/
void baobab_ringschart_draw_center (GtkWidget *rchart, 
                                    gboolean draw_center)
{
  BaobabRingschartPrivate *priv;
  
  g_return_if_fail (BAOBAB_IS_RINGSCHART (rchart));

  priv = BAOBAB_RINGSCHART (rchart)->priv;

  priv->draw_center = draw_center;
  gtk_widget_queue_draw (rchart);   
}

void baobab_ringschart_set_subfoldertips_enabled (GtkWidget *rchart, 
                                                  gboolean enabled)
{
  BaobabRingschartPrivate *priv;

  g_return_if_fail (BAOBAB_IS_RINGSCHART (rchart));

  priv = BAOBAB_RINGSCHART (rchart)->priv;

  if (enabled != priv->subfoldertips_enabled)
    {
      priv->subfoldertips_enabled = enabled;
      gtk_widget_queue_draw (rchart);
    }
}

gboolean baobab_ringschart_get_subfoldertips_enabled (GtkWidget *rchart)
{
  BaobabRingschartPrivate *priv;

  g_return_if_fail (BAOBAB_IS_RINGSCHART (rchart));

  priv = BAOBAB_RINGSCHART (rchart)->priv;

  return priv->subfoldertips_enabled;  
}
