/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * GNOME Disk Usage Analyzer
 *
 *  File:  baobab-spinner.h
 *
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  Nautilus is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Author: Andy Hertzfeld <andy@eazel.com>
 *
 *  Ephy port by Marco Pesenti Gritti <marco@it.gnome.org>
 *  Gsearchtool port by Dennis Cranston <dennis_cranston@yahoo.com>
 *  Baobab port by Dennis Cranston <dennis_cranston@yahoo.com>
 *
 */

#ifndef _BAOBAB_SPINNER_H
#define _BAOBAB_SPINNER_H

#include <gtk/gtkeventbox.h>
#include <gtk/gtkenums.h>

G_BEGIN_DECLS

#define BAOBAB_TYPE_SPINNER		(baobab_spinner_get_type ())
#define BAOBAB_SPINNER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), BAOBAB_TYPE_SPINNER, BaobabSpinner))
#define BAOBAB_SPINNER_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), BAOBAB_TYPE_SPINNER, BaobabSpinnerClass))
#define BAOBAB_IS_SPINNER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), BAOBAB_TYPE_SPINNER))
#define BAOBAB_IS_SPINNER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), BAOBAB_TYPE_SPINNER))
#define BAOBAB_SPINNER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), BAOBAB_TYPE_SPINNER, BaobabSpinnerClass))

typedef struct _BaobabSpinner		BaobabSpinner;
typedef struct _BaobabSpinnerClass	BaobabSpinnerClass;
typedef struct _BaobabSpinnerDetails	BaobabSpinnerDetails;

struct _BaobabSpinner
{
	GtkEventBox parent;

	/*< private >*/
	BaobabSpinnerDetails *details;
};

struct _BaobabSpinnerClass
{
	GtkEventBoxClass parent_class;
};

GType		baobab_spinner_get_type	(void);

GtkWidget      *baobab_spinner_new	(void);

void		baobab_spinner_start	(BaobabSpinner *throbber);

void		baobab_spinner_stop	(BaobabSpinner *throbber);

void		baobab_spinner_set_size	(BaobabSpinner *spinner,
					 GtkIconSize size);

G_END_DECLS

#endif /* _BAOBAB_SPINNER_H */
