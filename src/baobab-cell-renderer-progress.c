/* baobab-cell-renderer-progress.c
 *
 * Copyright (C) 2006 Paolo Borelli
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include <stdlib.h>

#include "baobab-cell-renderer-progress.h"

#define BAOBAB_CELL_RENDERER_PROGRESS_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object),                        \
                                                           BAOBAB_TYPE_CELL_RENDERER_PROGRESS, \
                                                           BaobabCellRendererProgressPrivate))

enum
{
  PROP_0,
  PROP_PERC
}; 

struct _BaobabCellRendererProgressPrivate
{
  double perc;
};

G_DEFINE_TYPE (BaobabCellRendererProgress, baobab_cell_renderer_progress, GTK_TYPE_CELL_RENDERER)

static void
baobab_cell_renderer_progress_init (BaobabCellRendererProgress *cellprogress)
{
  cellprogress->priv = BAOBAB_CELL_RENDERER_PROGRESS_GET_PRIVATE (cellprogress);
  cellprogress->priv->perc = 0;

  GTK_CELL_RENDERER(cellprogress)->mode = GTK_CELL_RENDERER_MODE_INERT;
  GTK_CELL_RENDERER(cellprogress)->xpad = 4;
  GTK_CELL_RENDERER(cellprogress)->ypad = 4;
}

GtkCellRenderer*
baobab_cell_renderer_progress_new (void)
{
  return g_object_new (BAOBAB_TYPE_CELL_RENDERER_PROGRESS, NULL);
}

static void
baobab_cell_renderer_progress_get_property (GObject *object,
					    guint param_id,
					    GValue *value,
					    GParamSpec *pspec)
{
  BaobabCellRendererProgress *cellprogress = BAOBAB_CELL_RENDERER_PROGRESS (object);
  
  switch (param_id)
    {
    case PROP_PERC:
      g_value_set_double (value, cellprogress->priv->perc);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
baobab_cell_renderer_progress_set_property (GObject *object,
					    guint param_id,
					    const GValue *value,
					    GParamSpec   *pspec)
{
  BaobabCellRendererProgress *cellprogress = BAOBAB_CELL_RENDERER_PROGRESS (object);

  switch (param_id)
    {
    case PROP_PERC:
      cellprogress->priv->perc = g_value_get_double (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

/* we simply have a fixed size */

#define FIXED_WIDTH   70
#define FIXED_HEIGHT  8

static void
baobab_cell_renderer_progress_get_size (GtkCellRenderer *cell,
					GtkWidget       *widget,
					GdkRectangle    *cell_area,
					gint            *x_offset,
					gint            *y_offset,
					gint            *width,
					gint            *height)
{
  gint calc_width;
  gint calc_height;

  calc_width  = (gint) cell->xpad * 2 + FIXED_WIDTH;
  calc_height = (gint) cell->ypad * 2 + FIXED_HEIGHT;

  if (width)
    *width = calc_width;

  if (height)
    *height = calc_height;

  if (cell_area)
  {
    if (x_offset)
    {
      *x_offset = cell->xalign * (cell_area->width - calc_width);
      *x_offset = MAX (*x_offset, 0);
    }

    if (y_offset)
    {
      *y_offset = cell->yalign * (cell_area->height - calc_height);
      *y_offset = MAX (*y_offset, 0);
    }
  }
}

static void
set_color_according_to_perc (cairo_t *cr, double value)
{
  static GdkColor red;
  static GdkColor yellow;
  static GdkColor green;
  static gboolean colors_initialized = FALSE;

  if (!colors_initialized)
    {
      /* hardcoded tango colors */
      gdk_color_parse ("#cc0000", &red);
      gdk_color_parse ("#edd400", &yellow);
      gdk_color_parse ("#73d216", &green);

      colors_initialized = TRUE;
    }

  if (value <= 0)
    {
      cairo_set_source_rgb (cr, 1, 1, 1);
      return;
    }
  else if (value <= 33.33)
    {
      gdk_cairo_set_source_color (cr, &green);
      return;
    }
  else if (value <= 66.66)
    {
      gdk_cairo_set_source_color (cr, &yellow);
      return;
    }
  else if (value <= 100.0)
    {
      gdk_cairo_set_source_color (cr, &red);
      return;
    }
  else
    g_assert_not_reached ();
}

static void
baobab_cell_renderer_progress_render (GtkCellRenderer *cell,
				      GdkWindow       *window,
				      GtkWidget       *widget,
				      GdkRectangle    *background_area,
				      GdkRectangle    *cell_area,
				      GdkRectangle    *expose_area,
				      guint            flags)
{
  BaobabCellRendererProgress *cellprogress = BAOBAB_CELL_RENDERER_PROGRESS (cell);
  gint x, y, w, h, perc_w, pos;
  gboolean is_rtl;
  cairo_t *cr;

  is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
  
  cr = gdk_cairo_create (window);
  
  x = cell_area->x + cell->xpad;
  y = cell_area->y + cell->ypad;
  
  w = cell_area->width - cell->xpad * 2;
  h = cell_area->height - cell->ypad * 2;

  /*
   * we always use a white bar with black
   * border and green/yellow/red progress...
   * I know it's not theme friendly, but we don't
   * want a plain progress bar
   */

  cairo_rectangle (cr, x, y, w, h);
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_fill (cr);
  
  x += widget->style->xthickness;
  y += widget->style->ythickness;
  w -= widget->style->xthickness * 2;
  h -= widget->style->ythickness * 2;
  
  cairo_rectangle (cr, x, y, w, h);
  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_fill (cr);

  perc_w = w * MAX (0, cellprogress->priv->perc) / 100;

  cairo_rectangle (cr, is_rtl ? (x + w - perc_w) : x, y, perc_w, h);
  set_color_according_to_perc (cr, cellprogress->priv->perc);
  cairo_fill (cr);
  
  cairo_destroy (cr);
}

static void
baobab_cell_renderer_progress_class_init (BaobabCellRendererProgressClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (klass);
  
  object_class->get_property = baobab_cell_renderer_progress_get_property;
  object_class->set_property = baobab_cell_renderer_progress_set_property;
  
  cell_class->get_size = baobab_cell_renderer_progress_get_size;
  cell_class->render = baobab_cell_renderer_progress_render;

  g_object_class_install_property (object_class,
				   PROP_PERC,
				   g_param_spec_double ("perc",
						        "percentage",
						        "precentage",
						        -1, 100, 0,
						        G_PARAM_READWRITE));

  g_type_class_add_private (object_class,
			    sizeof (BaobabCellRendererProgressPrivate));
}
