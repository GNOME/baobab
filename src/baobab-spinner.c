/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * GNOME Disk Usage Analyzer
 *
 *  File:  baobab-spinner.c
 *
 *  Copyright (C) 2004 Dennis Cranston
 *  Copyright (C) 2002 Marco Pesenti Gritti
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

#include "baobab-spinner.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkicontheme.h>
#include <gtk/gtkiconfactory.h>
#include <gtk/gtksettings.h>

/* Spinner cache implementation */

#define BAOBAB_TYPE_SPINNER_CACHE		(baobab_spinner_cache_get_type())
#define BAOBAB_SPINNER_CACHE(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), BAOBAB_TYPE_SPINNER_CACHE, BaobabSpinnerCache))
#define BAOBAB_SPINNER_CACHE_CLASS(klass) 	(G_TYPE_CHECK_CLASS_CAST((klass), BAOBAB_TYPE_SPINNER_CACHE, BaobabSpinnerCacheClass))
#define BAOBAB_IS_SPINNER_CACHE(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), BAOBAB_TYPE_SPINNER_CACHE))
#define BAOBAB_IS_SPINNER_CACHE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), BAOBAB_TYPE_SPINNER_CACHE))
#define BAOBAB_SPINNER_CACHE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), BAOBAB_TYPE_SPINNER_CACHE, BaobabSpinnerCacheClass))

typedef struct _BaobabSpinnerCache		BaobabSpinnerCache;
typedef struct _BaobabSpinnerCacheClass		BaobabSpinnerCacheClass;
typedef struct _BaobabSpinnerCachePrivate	BaobabSpinnerCachePrivate;

struct _BaobabSpinnerCacheClass
{
	GObjectClass parent_class;
};

struct _BaobabSpinnerCache
{
	GObject parent_object;

	/*< private >*/
	BaobabSpinnerCachePrivate *priv;
};

#define BAOBAB_SPINNER_CACHE_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), BAOBAB_TYPE_SPINNER_CACHE, BaobabSpinnerCachePrivate))

typedef struct
{
	GtkIconSize size;
	int width;
	int height;
	GdkPixbuf *quiescent_pixbuf;
	GList *images;
} BaobabSpinnerImages;

typedef struct
{
	GdkScreen *screen;
	GtkIconTheme *icon_theme;
	BaobabSpinnerImages *originals;
	/* List of BaobabSpinnerImages scaled to different sizes */
	GList *images;
} BaobabSpinnerCacheData;

struct _BaobabSpinnerCachePrivate
{
	/* Hash table of GdkScreen -> BaobabSpinnerCacheData */
	GHashTable *hash;
};

static void baobab_spinner_cache_class_init (BaobabSpinnerCacheClass *klass);
static void baobab_spinner_cache_init (BaobabSpinnerCache *cache);

static GObjectClass *cache_parent_class = NULL;

static GType
baobab_spinner_cache_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (BaobabSpinnerCacheClass),
			NULL,
			NULL,
			(GClassInitFunc) baobab_spinner_cache_class_init,
			NULL,
			NULL,
			sizeof (BaobabSpinnerCache),
			0,
			(GInstanceInitFunc) baobab_spinner_cache_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "BaobabSpinnerCache",
					       &our_info, 0);
	}

	return type;
}

static void
baobab_spinner_images_free (BaobabSpinnerImages *images)
{
	if (images != NULL)
	{
		g_list_foreach (images->images, (GFunc) g_object_unref, NULL);
		g_object_unref (images->quiescent_pixbuf);

		g_free (images);
	}
}

static BaobabSpinnerImages *
baobab_spinner_images_copy (BaobabSpinnerImages *images)
{
	BaobabSpinnerImages *copy = g_new (BaobabSpinnerImages, 1);

 	copy->size = images->size;
	copy->width = images->width;
	copy->height = images->height;

	copy->quiescent_pixbuf = g_object_ref (images->quiescent_pixbuf);
	copy->images = g_list_copy (images->images);
	g_list_foreach (copy->images, (GFunc) g_object_ref, NULL);

	return copy;
}

static void
baobab_spinner_cache_data_unload (BaobabSpinnerCacheData *data)
{
	g_return_if_fail (data != NULL);

	g_list_foreach (data->images, (GFunc) baobab_spinner_images_free, NULL);
	data->images = NULL;
	data->originals = NULL;
}

static GdkPixbuf *
extract_frame (GdkPixbuf *grid_pixbuf,
	       int x,
	       int y,
	       int size)
{
	GdkPixbuf *pixbuf;

	if (x + size > gdk_pixbuf_get_width (grid_pixbuf) ||
	    y + size > gdk_pixbuf_get_height (grid_pixbuf))
	{
		return NULL;
	}

	pixbuf = gdk_pixbuf_new_subpixbuf (grid_pixbuf,
					   x, y,
					   size, size);
	g_return_val_if_fail (pixbuf != NULL, NULL);

	return pixbuf;
}

static void
baobab_spinner_cache_data_load (BaobabSpinnerCacheData *data)
{
	BaobabSpinnerImages *images;
	GdkPixbuf *icon_pixbuf, *pixbuf;
	GtkIconInfo *icon_info;
	int grid_width, grid_height, x, y, size, h, w;
	const char *icon;

	g_return_if_fail (data != NULL);

	baobab_spinner_cache_data_unload (data);

	/* Load the animation */
	icon_info = gtk_icon_theme_lookup_icon (data->icon_theme,
						"gnome-spinner", -1, 0);
	if (icon_info == NULL)
	{
		g_warning ("Throbber animation not found\n");
		return;
	}

	size = gtk_icon_info_get_base_size (icon_info);
	icon = gtk_icon_info_get_filename (icon_info);
	g_return_if_fail (icon != NULL);

	icon_pixbuf = gdk_pixbuf_new_from_file (icon, NULL);
	if (icon_pixbuf == NULL)
	{
		g_warning ("Could not load the spinner file\n");
		gtk_icon_info_free (icon_info);
		return;
	}

	grid_width = gdk_pixbuf_get_width (icon_pixbuf);
	grid_height = gdk_pixbuf_get_height (icon_pixbuf);

	images = g_new (BaobabSpinnerImages, 1);
	data->images = g_list_prepend (NULL, images);
	data->originals = images;

	images->size = GTK_ICON_SIZE_INVALID;
	images->width = images->height = size;
	images->images = NULL;
	images->quiescent_pixbuf = NULL;

	for (y = 0; y < grid_height; y += size)
	{
		for (x = 0; x < grid_width ; x += size)
		{
			pixbuf = extract_frame (icon_pixbuf, x, y, size);

			if (pixbuf)
			{
				images->images =
					g_list_prepend (images->images, pixbuf);
			}
			else
			{
				g_warning ("Cannot extract frame from the grid\n");
			}
		}
	}
	images->images = g_list_reverse (images->images);

	gtk_icon_info_free (icon_info);
	g_object_unref (icon_pixbuf);

	/* Load the rest icon */
	icon_info = gtk_icon_theme_lookup_icon (data->icon_theme,
						"gnome-spinner-rest", -1, 0);
	if (icon_info == NULL)
	{
		g_warning ("Throbber rest icon not found\n");
		return;
	}

	size = gtk_icon_info_get_base_size (icon_info);
	icon = gtk_icon_info_get_filename (icon_info);
	g_return_if_fail (icon != NULL);

	icon_pixbuf = gdk_pixbuf_new_from_file (icon, NULL);
	gtk_icon_info_free (icon_info);

	if (icon_pixbuf == NULL)
	{
		g_warning ("Could not load spinner rest icon\n");
		baobab_spinner_images_free (images);
		return;
	}

	images->quiescent_pixbuf = icon_pixbuf;

	w = gdk_pixbuf_get_width (icon_pixbuf);
	h = gdk_pixbuf_get_height (icon_pixbuf);
	images->width = MAX (images->width, w);
	images->height = MAX (images->height, h);
}

static BaobabSpinnerCacheData *
baobab_spinner_cache_data_new (GdkScreen *screen)
{
	BaobabSpinnerCacheData *data;

	data = g_new0 (BaobabSpinnerCacheData, 1);

	data->screen = screen;
	data->icon_theme = gtk_icon_theme_get_for_screen (screen);
	g_signal_connect_swapped (data->icon_theme, "changed",
				  G_CALLBACK (baobab_spinner_cache_data_load),
				  data);

	baobab_spinner_cache_data_load (data);

	return data;
}

static void
baobab_spinner_cache_data_free (BaobabSpinnerCacheData *data)
{
	g_return_if_fail (data != NULL);
	g_return_if_fail (data->icon_theme != NULL);

	g_signal_handlers_disconnect_by_func
		(data->icon_theme,
		 G_CALLBACK (baobab_spinner_cache_data_load), data);

	baobab_spinner_cache_data_unload (data);

	g_free (data);
}

static int
compare_size (gconstpointer images_ptr,
	      gconstpointer size_ptr)
{
	const BaobabSpinnerImages *images = (const BaobabSpinnerImages *) images_ptr;
	GtkIconSize size = GPOINTER_TO_INT (size_ptr);

	if (images->size == size)
	{
		return 0;
	}

	return -1;
}

static GdkPixbuf *
scale_to_size (GdkPixbuf *pixbuf,
	       int dw,
	       int dh)
{
	GdkPixbuf *result;
	int pw, ph;

	pw = gdk_pixbuf_get_width (pixbuf);
	ph = gdk_pixbuf_get_height (pixbuf);

	if (pw != dw || ph != dh)
	{
		result = gdk_pixbuf_scale_simple (pixbuf, dw, dh,
						  GDK_INTERP_BILINEAR);
	}
	else
	{
		result = g_object_ref (pixbuf);
	}

	return result;
}

static BaobabSpinnerImages *
baobab_spinner_cache_get_images (BaobabSpinnerCache *cache,
			       GdkScreen *screen,
			       GtkIconSize size)
{
	BaobabSpinnerCachePrivate *priv = cache->priv;
	BaobabSpinnerCacheData *data;
	BaobabSpinnerImages *images;
	GtkSettings *settings;
	GdkPixbuf *pixbuf, *scaled_pixbuf;
	GList *element, *l;
	int h, w;

	data = g_hash_table_lookup (priv->hash, screen);
	if (data == NULL)
	{
		data = baobab_spinner_cache_data_new (screen);
		g_hash_table_insert (priv->hash, screen, data);
	}

	if (data->images == NULL || data->originals == NULL)
	{
		/* Load failed, but don't try endlessly again! */
		return NULL;
	}

	element = g_list_find_custom (data->images,
				      GINT_TO_POINTER (size),
				      (GCompareFunc) compare_size);
	if (element != NULL)
	{
		return baobab_spinner_images_copy ((BaobabSpinnerImages *) element->data);
	}

	settings = gtk_settings_get_for_screen (screen);
	if (!gtk_icon_size_lookup_for_settings (settings, size, &w, &h))
	{
		g_warning ("Failed to look up icon size\n");
		return NULL;
	}

	images = g_new (BaobabSpinnerImages, 1);
	images->size = size;
	images->width = w;
	images->height = h;
	images->images = NULL;

	for (l = data->originals->images; l != NULL; l = l->next)
	{
		pixbuf = (GdkPixbuf *) l->data;
		scaled_pixbuf = scale_to_size (pixbuf, w, h);

		images->images = g_list_prepend (images->images, scaled_pixbuf);
	}
	images->images = g_list_reverse (images->images);

	images->quiescent_pixbuf =
		scale_to_size (data->originals->quiescent_pixbuf, w, h);

	/* store in cache */
	data->images = g_list_prepend (data->images, images);

	return baobab_spinner_images_copy (images);
}

static void
baobab_spinner_cache_init (BaobabSpinnerCache *cache)
{
	BaobabSpinnerCachePrivate *priv;

	priv = cache->priv = BAOBAB_SPINNER_CACHE_GET_PRIVATE (cache);

	priv->hash = g_hash_table_new_full (g_direct_hash, g_direct_equal,
					    NULL,
					    (GDestroyNotify) baobab_spinner_cache_data_free);
}

static void
baobab_spinner_cache_finalize (GObject *object)
{
	BaobabSpinnerCache *cache = BAOBAB_SPINNER_CACHE (object); 
	BaobabSpinnerCachePrivate *priv = cache->priv;

	g_hash_table_destroy (priv->hash);

	G_OBJECT_CLASS (cache_parent_class)->finalize (object);
}

static void
baobab_spinner_cache_class_init (BaobabSpinnerCacheClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	cache_parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = baobab_spinner_cache_finalize;

	g_type_class_add_private (object_class, sizeof (BaobabSpinnerCachePrivate));
}

static BaobabSpinnerCache *spinner_cache = NULL;

static BaobabSpinnerCache *
baobab_spinner_cache_ref (void)
{
	if (spinner_cache == NULL)
	{
		BaobabSpinnerCache **cache_ptr;

		spinner_cache = g_object_new (BAOBAB_TYPE_SPINNER_CACHE, NULL);
		cache_ptr = &spinner_cache;
		g_object_add_weak_pointer (G_OBJECT (spinner_cache),
					   (gpointer *) cache_ptr);

		return spinner_cache;
	}
	else
	{
		return g_object_ref (spinner_cache);
	}
}

/* Spinner implementation */

#define SPINNER_TIMEOUT 125 /* ms */

#define BAOBAB_SPINNER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), BAOBAB_TYPE_SPINNER, BaobabSpinnerDetails))

struct _BaobabSpinnerDetails
{
	GtkIconTheme *icon_theme;
	BaobabSpinnerCache *cache;
	GtkIconSize size;
	BaobabSpinnerImages *images;
	GList *current_image;
	guint timeout;
	guint timer_task;
	guint spinning : 1;
};

static void baobab_spinner_class_init (BaobabSpinnerClass *class);
static void baobab_spinner_init	    (BaobabSpinner *spinner);

static GObjectClass *parent_class = NULL;

GType
baobab_spinner_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (BaobabSpinnerClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) baobab_spinner_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (BaobabSpinner),
			0, /* n_preallocs */
			(GInstanceInitFunc) baobab_spinner_init
		};

		type = g_type_register_static (GTK_TYPE_EVENT_BOX,
					       "BaobabSpinner",
					       &our_info, 0);
	}

	return type;
}

static gboolean
baobab_spinner_load_images (BaobabSpinner *spinner)
{
	BaobabSpinnerDetails *details = spinner->details;

	if (details->images == NULL)
	{
		details->images =
			baobab_spinner_cache_get_images
				(details->cache,
				 gtk_widget_get_screen (GTK_WIDGET (spinner)),
				 details->size);

		if (details->images != NULL)
		{
			details->current_image = details->images->images;
		}

	}

	return details->images != NULL;
}

static void
baobab_spinner_unload_images (BaobabSpinner *spinner)
{
	baobab_spinner_images_free (spinner->details->images);
	spinner->details->images = NULL;
	spinner->details->current_image = NULL;
}

static void
icon_theme_changed_cb (GtkIconTheme *icon_theme,
		       BaobabSpinner *spinner)
{
	baobab_spinner_unload_images (spinner);
	gtk_widget_queue_resize (GTK_WIDGET (spinner));
}

static void
baobab_spinner_init (BaobabSpinner *spinner)
{
	BaobabSpinnerDetails *details = spinner->details;
	GtkWidget *widget = GTK_WIDGET (spinner);

	gtk_widget_set_events (widget,
			       gtk_widget_get_events (widget)
			       | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
			       | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);

	details = spinner->details = BAOBAB_SPINNER_GET_PRIVATE (spinner);

	details->cache = baobab_spinner_cache_ref ();
	details->size = GTK_ICON_SIZE_INVALID;
	details->spinning = FALSE;
	details->timeout = SPINNER_TIMEOUT;
}

static GdkPixbuf *
select_spinner_image (BaobabSpinner *spinner)
{
	BaobabSpinnerDetails *details = spinner->details;
	BaobabSpinnerImages *images = details->images;

	g_return_val_if_fail (images != NULL, NULL);

	if (spinner->details->timer_task == 0)
	{
		if (images->quiescent_pixbuf != NULL)
		{
			return g_object_ref (details->images->quiescent_pixbuf);
		}

		return NULL;
	}

	g_return_val_if_fail (details->current_image != NULL, NULL);

	return g_object_ref (details->current_image->data);
}

static int
baobab_spinner_expose (GtkWidget *widget,
		     GdkEventExpose *event)
{
	BaobabSpinner *spinner = BAOBAB_SPINNER (widget);
	GdkPixbuf *pixbuf;
	GdkGC *gc;
	int x_offset, y_offset, width, height;
	GdkRectangle pix_area, dest;

	if (!GTK_WIDGET_DRAWABLE (spinner))
	{
		return TRUE;
	}

	if (!baobab_spinner_load_images (spinner))
	{
		return TRUE;
	}

	pixbuf = select_spinner_image (spinner);
	if (pixbuf == NULL)
	{
		return FALSE;
	}

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	/* Compute the offsets for the image centered on our allocation */
	x_offset = (widget->allocation.width - width) / 2;
	y_offset = (widget->allocation.height - height) / 2;

	pix_area.x = x_offset + widget->allocation.x;
	pix_area.y = y_offset + widget->allocation.y;
	pix_area.width = width;
	pix_area.height = height;

	if (!gdk_rectangle_intersect (&event->area, &pix_area, &dest))
	{
		g_object_unref (pixbuf);
		return FALSE;
	}

	gc = gdk_gc_new (widget->window);
	gdk_draw_pixbuf (widget->window, gc, pixbuf,
			 dest.x - x_offset - widget->allocation.x,
			 dest.y - y_offset - widget->allocation.y,
			 dest.x, dest.y,
			 dest.width, dest.height,
			 GDK_RGB_DITHER_MAX, 0, 0);
	g_object_unref (gc);

	g_object_unref (pixbuf);

	return FALSE;
}

static gboolean
bump_spinner_frame_cb (BaobabSpinner *spinner)
{
	BaobabSpinnerDetails *details = spinner->details;
	GList *frame;

	if (!GTK_WIDGET_DRAWABLE (spinner)) return TRUE;

	/* This can happen when we've unloaded the images on a theme
	 * change, but haven't been in the queued size request yet.
	 * Just skip this update.
	 */
	if (details->images == NULL) return TRUE;

	frame = details->current_image;
	g_assert (frame != NULL);

	if (frame->next != NULL)
	{
		frame = frame->next;
	}
	else
	{
		frame = g_list_first (frame);
	}

	details->current_image = frame;

	gtk_widget_queue_draw (GTK_WIDGET (spinner));

	/* run again */
	return TRUE;
}

/**
 * baobab_spinner_start:
 * @spinner: a #BaobabSpinner
 *
 * Start the spinner animation.
 **/
void
baobab_spinner_start (BaobabSpinner *spinner)
{
	BaobabSpinnerDetails *details = spinner->details;

	details->spinning = TRUE;

	if (GTK_WIDGET_MAPPED (GTK_WIDGET (spinner)) &&
	    details->timer_task == 0 &&
	    baobab_spinner_load_images (spinner))
	{
		if (details->images != NULL)
		{
			/* reset to first frame */
			details->current_image = details->images->images;
		}

		details->timer_task =
			g_timeout_add (details->timeout,
				       (GSourceFunc) bump_spinner_frame_cb,
				       spinner);
	}
}

static void
baobab_spinner_remove_update_callback (BaobabSpinner *spinner)
{
	if (spinner->details->timer_task != 0)
	{
		g_source_remove (spinner->details->timer_task);
		spinner->details->timer_task = 0;
	}
}

/**
 * baobab_spinner_stop:
 * @spinner: a #BaobabSpinner
 *
 * Stop the spinner animation.
 **/
void
baobab_spinner_stop (BaobabSpinner *spinner)
{
	BaobabSpinnerDetails *details = spinner->details;

	details->spinning = FALSE;

	if (spinner->details->timer_task != 0)
	{
		baobab_spinner_remove_update_callback (spinner);

		if (GTK_WIDGET_MAPPED (GTK_WIDGET (spinner)))
		{
			gtk_widget_queue_draw (GTK_WIDGET (spinner));
		}
	}
}

/*
 * baobab_spinner_set_size:
 * @spinner: a #BaobabSpinner
 * @size: the size of type %GtkIconSize
 *
 * Set the size of the spinner. Use %GTK_ICON_SIZE_INVALID to use the
 * native size of the icons.
 **/
void
baobab_spinner_set_size (BaobabSpinner *spinner,
		       GtkIconSize size)
{
	if (size != spinner->details->size)
	{
		baobab_spinner_unload_images (spinner);

		spinner->details->size = size;

		gtk_widget_queue_resize (GTK_WIDGET (spinner));
	}
}

#if 0
/*
 * baobab_spinner_set_timeout:
 * @spinner: a #BaobabSpinner
 * @timeout: time delay between updates to the spinner.
 *
 * Sets the timeout delay for spinner updates.
 **/
void
baobab_spinner_set_timeout (BaobabSpinner *spinner,
			  guint timeout)
{
	BaobabSpinnerDetails *details = spinner->details;

	if (timeout != details->timeout)
	{
		baobab_spinner_stop (spinner);

		details->timeout = timeout;

		if (details->spinning)
		{
			baobab_spinner_start (spinner);
		}
	}
}
#endif

static void
baobab_spinner_size_request (GtkWidget *widget,
			   GtkRequisition *requisition)
{
	BaobabSpinner *spinner = BAOBAB_SPINNER (widget);

	if (!baobab_spinner_load_images (spinner))
	{
		requisition->width = requisition->height = 0;
		gtk_icon_size_lookup_for_settings (gtk_widget_get_settings (widget),
						   spinner->details->size,
						   &requisition->width,
					           &requisition->height);
		return;
	}

	requisition->width = spinner->details->images->width;
	requisition->height = spinner->details->images->height;

	/* allocate some extra margin so we don't butt up against toolbar edges */
	if (spinner->details->size != GTK_ICON_SIZE_MENU)
	{
		requisition->width += 4;
		requisition->height += 4;
	}
}

static void
baobab_spinner_map (GtkWidget *widget)
{
	BaobabSpinner *spinner = BAOBAB_SPINNER (widget);
	BaobabSpinnerDetails *details = spinner->details;

	GTK_WIDGET_CLASS (parent_class)->map (widget);

	if (details->spinning)
	{
		baobab_spinner_start (spinner);
	}
}

static void
baobab_spinner_unmap (GtkWidget *widget)
{
	BaobabSpinner *spinner = BAOBAB_SPINNER (widget);

	baobab_spinner_remove_update_callback (spinner);

	GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}

static void
baobab_spinner_dispose (GObject *object)
{
	BaobabSpinner *spinner = BAOBAB_SPINNER (object);

	g_signal_handlers_disconnect_by_func
			(spinner->details->icon_theme,
		 G_CALLBACK (icon_theme_changed_cb), spinner);

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
baobab_spinner_finalize (GObject *object)
{
	BaobabSpinner *spinner = BAOBAB_SPINNER (object);

	baobab_spinner_remove_update_callback (spinner);
	baobab_spinner_unload_images (spinner);

	g_object_unref (spinner->details->cache);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
baobab_spinner_screen_changed (GtkWidget *widget,
			     GdkScreen *old_screen)
{
	BaobabSpinner *spinner = BAOBAB_SPINNER (widget);
	BaobabSpinnerDetails *details = spinner->details;
	GdkScreen *screen;

	if (GTK_WIDGET_CLASS (parent_class)->screen_changed)
	{
		GTK_WIDGET_CLASS (parent_class)->screen_changed (widget, old_screen);
	}

	screen = gtk_widget_get_screen (widget);

	/* FIXME: this seems to be happening when then spinner is destroyed!? */
	if (old_screen == screen) return;

	/* We'll get mapped again on the new screen, but not unmapped from
	 * the old screen, so remove timeout here.
	 */
	baobab_spinner_remove_update_callback (spinner);

	baobab_spinner_unload_images (spinner);

	if (old_screen != NULL)
	{
		g_signal_handlers_disconnect_by_func
			(gtk_icon_theme_get_for_screen (old_screen),
			 G_CALLBACK (icon_theme_changed_cb), spinner);
	}

	details->icon_theme = gtk_icon_theme_get_for_screen (screen);
	g_signal_connect (details->icon_theme, "changed",
			  G_CALLBACK (icon_theme_changed_cb), spinner);
}

static void
baobab_spinner_class_init (BaobabSpinnerClass *class)
{
	GObjectClass *object_class =  G_OBJECT_CLASS (class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = baobab_spinner_dispose;
	object_class->finalize = baobab_spinner_finalize;

	widget_class->expose_event = baobab_spinner_expose;
	widget_class->size_request = baobab_spinner_size_request;
	widget_class->map = baobab_spinner_map;
	widget_class->unmap = baobab_spinner_unmap;
	widget_class->screen_changed = baobab_spinner_screen_changed;

	g_type_class_add_private (object_class, sizeof (BaobabSpinnerDetails));
}

/*
 * baobab_spinner_new:
 *
 * Create a new #BaobabSpinner. The spinner is a widget
 * that gives the user feedback about network status with
 * an animated image.
 *
 * Return Value: the spinner #GtkWidget
 **/
GtkWidget *
baobab_spinner_new (void)
{
	return GTK_WIDGET (g_object_new (BAOBAB_TYPE_SPINNER,
					 "visible-window", FALSE,
					 NULL));
}
