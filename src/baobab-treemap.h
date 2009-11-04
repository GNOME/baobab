/*
 * baobab-treemap.h
 *
 * Copyright (C) 2008 igalia
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

#ifndef __BAOBAB_TREEMAP_H__
#define __BAOBAB_TREEMAP_H__

#include <gtk/gtk.h>
#include "baobab-chart.h"

G_BEGIN_DECLS

#define BAOBAB_TREEMAP_TYPE          (baobab_treemap_get_type ())
#define BAOBAB_TREEMAP(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), BAOBAB_TREEMAP_TYPE, BaobabTreemap))
#define BAOBAB_TREEMAP_CLASS(obj)    (G_TYPE_CHECK_CLASS_CAST ((obj), BAOBAB_TREEMAP, BaobabTreemapClass))
#define BAOBAB_IS_TREEMAP(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BAOBAB_TREEMAP_TYPE))
#define BAOBAB_IS_TREEMAP_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE ((obj), BAOBAB_TREEMAP_TYPE))
#define BAOBAB_TREEMAP_GET_CLASS     (G_TYPE_INSTANCE_GET_CLASS ((obj), BAOBAB_TREEMAP_TYPE, BaobabTreemapClass))

typedef struct _BaobabTreemap BaobabTreemap;
typedef struct _BaobabTreemapClass BaobabTreemapClass;
typedef struct _BaobabTreemapPrivate BaobabTreemapPrivate;

struct _BaobabTreemap
{
  BaobabChart parent;

  BaobabTreemapPrivate *priv;
};

struct _BaobabTreemapClass
{
  BaobabChartClass parent_class;
};

GType baobab_treemap_get_type (void) G_GNUC_CONST;
GtkWidget* baobab_treemap_new (void);

G_END_DECLS

#endif
