/*
 * baobab-ringschart.h
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

#ifndef __BAOBAB_RINGSCHART_H__
#define __BAOBAB_RINGSCHART_H__

#include <gtk/gtk.h>
#include "baobab-chart.h"

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
  BaobabChart parent;
  
  BaobabRingschartPrivate *priv;
};

struct _BaobabRingschartClass
{
  BaobabChartClass parent_class;
};

GType baobab_ringschart_get_type (void) G_GNUC_CONST;
GtkWidget *baobab_ringschart_new (void);
void baobab_ringschart_set_subfoldertips_enabled (GtkWidget *chart, 
                                                  gboolean enabled);

G_END_DECLS

#endif
