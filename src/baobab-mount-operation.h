/* Baobab - (C) 2005-2008 Fabio Marzocca

	baobab-mount-operation.h

   Modified module from from eel-mount-operation.h
   Released under same licence
   
 */

/* eel-mount-operation.h - Gtk+ implementation for GMountOperation

   Copyright (C) 2007 Red Hat, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Alexander Larsson <alexl@redhat.com>
*/

#ifndef BAOBAB_MOUNT_OPERATION_H
#define BAOBAB_MOUNT_OPERATION_H

#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtkwindow.h>

G_BEGIN_DECLS

#define BAOBAB_TYPE_MOUNT_OPERATION         (baobab_mount_operation_get_type ())
#define BAOBAB_MOUNT_OPERATION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BAOBAB_TYPE_MOUNT_OPERATION, BaobabMountOperation))
#define BAOBAB_MOUNT_OPERATION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BAOBAB_TYPE_MOUNT_OPERATION, BaobabMountOperationClass))
#define BAOBAB_IS_MOUNT_OPERATION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BAOBAB_TYPE_MOUNT_OPERATION))
#define BAOBAB_IS_MOUNT_OPERATION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BAOBAB_TYPE_MOUNT_OPERATION))
#define BAOBAB_MOUNT_OPERATION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BAOBAB_TYPE_MOUNT_OPERATION, BaobabMountOperationClass))

typedef struct BaobabMountOperation	    BaobabMountOperation;
typedef struct BaobabMountOperationClass       BaobabMountOperationClass;
typedef struct BaobabMountOperationPrivate     BaobabMountOperationPrivate;

struct BaobabMountOperation
{
	GMountOperation parent_instance;
	
	BaobabMountOperationPrivate *priv;
};

struct BaobabMountOperationClass 
{
	GMountOperationClass parent_class;


	/* signals: */

	void (* active_changed) (BaobabMountOperation *operation,
				 gboolean is_active);
};

GType            baobab_mount_operation_get_type (void);
GMountOperation *baobab_mount_operation_new      (GtkWindow *parent);

G_END_DECLS

#endif /* BAOBAB_MOUNT_OPERATION_H */



