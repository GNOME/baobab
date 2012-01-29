/*
 * baobab-chart.h
 *
 * Copyright (C) 2006, 2007, 2008 Igalia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the licence, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
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

#ifndef __BAOBAB_CHART_H__
#define __BAOBAB_CHART_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BAOBAB_TYPE_CHART                                   (baobab_chart_get_type ())
#define BAOBAB_CHART(inst)                                  (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                             BAOBAB_TYPE_CHART, BaobabChart))
#define BAOBAB_CHART_CLASS(class)                           (G_TYPE_CHECK_CLASS_CAST ((class),                       \
                                                             BAOBAB_TYPE_CHART, BaobabChartClass))
#define BAOBAB_IS_CHART(inst)                               (G_TYPE_CHECK_INSTANCE_TYPE ((inst), BAOBAB_TYPE_CHART))
#define BAOBAB_IS_CHART_CLASS(class)                        (G_TYPE_CHECK_CLASS_TYPE ((class), BAOBAB_TYPE_CHART))
#define BAOBAB_CHART_GET_CLASS(inst)                        (G_TYPE_INSTANCE_GET_CLASS ((inst),                      \
                                                             BAOBAB_TYPE_CHART, BaobabChartClass))

typedef struct _BaobabChart                                 BaobabChart;
typedef struct _BaobabChartClass                            BaobabChartClass;
typedef struct _BaobabChartPrivate                          BaobabChartPrivate;
typedef struct _BaobabChartColor                            BaobabChartColor;
typedef struct _BaobabChartItem                             BaobabChartItem;

struct _BaobabChart
{
  GtkWidget parent;

  /*< private >*/
  BaobabChartPrivate *priv;
};

struct _BaobabChartColor
{
  gdouble red;
  gdouble green;
  gdouble blue;
};

struct _BaobabChartItem
{
  gchar *name;
  gchar *size;
  guint depth;
  gdouble rel_start;
  gdouble rel_size;
  GtkTreeIter iter;
  gboolean visible;
  gboolean has_any_child;
  gboolean has_visible_children;
  GdkRectangle rect;

  GList *parent;

  gpointer data;
};

struct _BaobabChartClass
{
  GtkWidgetClass parent_class;

  /* Signal prototypes */
  void       (* item_activated)                 (BaobabChart     *chart,
                                                 GtkTreeIter     *iter);

  /* Abstract methods */
  void       (* draw_item)                      (BaobabChart     *chart,
                                                 cairo_t         *cr,
                                                 BaobabChartItem *item,
                                                 gboolean         highlighted);

  void       (* pre_draw)                       (BaobabChart     *chart,
                                                 cairo_t         *cr);

  void       (* post_draw)                      (BaobabChart     *chart,
                                                 cairo_t         *cr);

  void       (* calculate_item_geometry)        (BaobabChart     *chart,
                                                 BaobabChartItem *item);

  gboolean   (* is_point_over_item)             (BaobabChart     *chart,
                                                 BaobabChartItem *item,
                                                 gdouble          x,
                                                 gdouble          y);

  void       (* get_item_rectangle)             (BaobabChart     *chart,
                                                 BaobabChartItem *item);

  guint      (* can_zoom_in)                    (BaobabChart     *chart);
  guint      (* can_zoom_out)                   (BaobabChart     *chart);
};

GType                   baobab_chart_get_type                           (void) G_GNUC_CONST;
BaobabChart *           baobab_chart_new                                (void);
void                    baobab_chart_set_model_with_columns             (BaobabChart  *chart,
                                                                         GtkTreeModel *model,
                                                                         guint         name_column,
                                                                         guint         size_column,
                                                                         guint         info_column,
                                                                         guint         percentage_column,
                                                                         guint         valid_column,
                                                                         GtkTreePath  *root);
void                    baobab_chart_set_model                          (BaobabChart *chart,
                                                                         GtkTreeModel *model);
GtkTreeModel *          baobab_chart_get_model                          (BaobabChart *chart);
void                    baobab_chart_set_max_depth                      (BaobabChart *chart,
                                                                         guint max_depth);
guint                   baobab_chart_get_max_depth                      (BaobabChart *chart);
void                    baobab_chart_set_root                           (BaobabChart *chart,
                                                                         GtkTreePath *root);
GtkTreePath *           baobab_chart_get_root                           (BaobabChart *chart);
void                    baobab_chart_freeze_updates                     (BaobabChart *chart);
void                    baobab_chart_thaw_updates                       (BaobabChart *chart);
void                    baobab_chart_get_item_color                     (BaobabChartColor *color,
                                                                         gdouble position,
                                                                         gint depth,
                                                                         gboolean highlighted);
void                    baobab_chart_move_up_root                       (BaobabChart *chart);
void                    baobab_chart_zoom_in                            (BaobabChart *chart);
void                    baobab_chart_zoom_out                           (BaobabChart *chart);
gboolean                baobab_chart_can_zoom_in                        (BaobabChart *chart);
gboolean                baobab_chart_can_zoom_out                       (BaobabChart *chart);
void                    baobab_chart_save_snapshot                      (BaobabChart *chart);
gboolean                baobab_chart_is_frozen                          (BaobabChart *chart);
BaobabChartItem *       baobab_chart_get_highlighted_item               (BaobabChart *chart);

G_END_DECLS

#endif /* __BAOBAB_CHART_H__ */
