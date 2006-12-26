/*
 * baobab-ringschart.h
 *
 * Copyright (C) 2006 igalia
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
 *   Mario Sanchez <msanchez@igalia.com>
 *   Miguel Gomez <mgomez@igalia.com>
 *   Henrique Ferreiro <hferreiro@igalia.com>
 *   Alejandro Pinheiro <apinheiro@igalia.com>
 *   Alejandro Garcia <alex@igalia.com>
 */

#ifndef __BAOBAB_RINGSCHART_H__
#define __BAOBAB_RINGSCHART_H__

#include <gtk/gtk.h>
#include <gtk/gtktreemodel.h>

G_BEGIN_DECLS

#define BAOBAB_RINGSCHART_TYPE		(baobab_ringschart_get_type ())
#define BAOBAB_RINGSCHART(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), BAOBAB_RINGSCHART_TYPE, BaobabRingschart))
#define BAOBAB_RINGSCHART_CLASS(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), BAOBAB_RINGSCHART, BaobabRingschartClass))
#define BAOBAB_IS_RINGSCHART(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), BAOBAB_RINGSCHART_TYPE))
#define BAOBAB_IS_RINGSCHART_CLASS(obj)	(G_TYPE_CHECK_CLASS_TYPE ((obj), BAOBAB_RINGSCHART_TYPE))
#define BAOBAB_RINGSCHART_GET_CLASS	(G_TYPE_INSTANCE_GET_CLASS ((obj), BAOBAB_RINGSCHART_TYPE, BaobabRingschartClass))

typedef struct _BaobabRingschart BaobabRingschart;
typedef struct _BaobabRingschartClass BaobabRingschartClass;
typedef struct _BaobabRingschartPrivate BaobabRingschartPrivate;

struct _BaobabRingschart
{
  GtkWidget parent;
  
  /* < private > */
  BaobabRingschartPrivate *priv;
};

struct _BaobabRingschartClass
{
  GtkWidgetClass parent_class;

  void (* sector_activated) (BaobabRingschart *rchart,
                             GtkTreeIter *iter);
  
};

GType baobab_ringschart_get_type (void) G_GNUC_CONST;

GtkWidget *baobab_ringschart_new (void);
void baobab_ringschart_set_model_with_columns (GtkWidget *rchart,
                                               GtkTreeModel *model,
                                               guint name_column,
                                               guint size_column,
                                               guint info_column,
                                               guint percentage_column,
                                               guint valid_column,
                                               GtkTreePath *root);
void baobab_ringschart_set_model (GtkWidget *rchart,
                                  GtkTreeModel *model);
GtkTreeModel *baobab_ringschart_get_model (GtkWidget *rchart);
void baobab_ringschart_set_max_depth (GtkWidget *rchart, 
                                      guint max_depth);
guint baobab_ringschart_get_max_depth (GtkWidget *rchart);
void baobab_ringschart_set_root (GtkWidget *rchart, 
                                 GtkTreePath *root);
GtkTreePath *baobab_ringschart_get_root (GtkWidget *rchart);
gint baobab_ringschart_get_drawn_elements (GtkWidget *rchart);
void baobab_ringschart_freeze_updates (GtkWidget *rchart);
void baobab_ringschart_thaw_updates (GtkWidget *rchart);
void baobab_ringschart_set_init_depth (GtkWidget *rchart, 
                                       guint depth);
void baobab_ringschart_draw_center (GtkWidget *rchart, 
                                    gboolean draw_center);

G_END_DECLS

#endif
