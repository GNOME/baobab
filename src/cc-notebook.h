/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright © 2012 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *      Bastien Nocera <hadess@hadess.net>
 */

#ifndef _CC_NOTEBOOK_H_
#define _CC_NOTEBOOK_H_

#include <gtk/gtk.h>
#include <clutter-gtk/clutter-gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_NOTEBOOK                (cc_notebook_get_type ())
#define CC_NOTEBOOK(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), CC_TYPE_NOTEBOOK, CcNotebook))
#define CC_IS_NOTEBOOK(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CC_TYPE_NOTEBOOK))
#define CC_NOTEBOOK_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CC_TYPE_NOTEBOOK, CcNotebookClass))
#define CC_IS_NOTEBOOK_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CC_TYPE_NOTEBOOK))
#define CC_NOTEBOOK_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CC_TYPE_NOTEBOOK, CcNotebookClass))

typedef struct _CcNotebook            CcNotebook;
typedef struct _CcNotebookPrivate     CcNotebookPrivate;
typedef struct _CcNotebookClass       CcNotebookClass;

struct _CcNotebook
{
        GtkBox parent_class;

        CcNotebookPrivate *priv;
};

struct _CcNotebookClass
{
        GtkBoxClass parent_class;
};

GType           cc_notebook_get_type                    (void) G_GNUC_CONST;

GtkWidget *     cc_notebook_new                         (void);

void            cc_notebook_add_page                    (CcNotebook *self,
                                                         GtkWidget  *widget);
void            cc_notebook_remove_page                 (CcNotebook *self,
                                                         GtkWidget  *widget);

void            cc_notebook_select_page                 (CcNotebook *self,
                                                         GtkWidget  *widget,
                                                         gboolean    animate);

GtkWidget *     cc_notebook_get_selected_page           (CcNotebook *self);

G_END_DECLS

#endif /* _CC_NOTEBOOK_H_ */
