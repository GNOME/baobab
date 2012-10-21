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
 *      Emmanuele Bassi <ebassi@linux.intel.com>
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "cc-notebook.h"

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CC_TYPE_NOTEBOOK, CcNotebookPrivate))

/*
 * Structure:
 *
 *   Notebook
 *   +---- GtkClutterEmbed
 *         +---- ClutterStage
 *               +---- ClutterScrollActor:scroll
 *                     +---- ClutterActor:bin
 *                           +---- ClutterActor:frame<ClutterBinLayout>
 *                                 +---- GtkClutterActor:embed<GtkWidget>
 *
 * the frame element is needed to make the GtkClutterActor contents fill the allocation
 */

struct _CcNotebookPrivate
{
        GtkWidget *embed;

        ClutterActor *stage;
        ClutterActor *scroll;
        ClutterActor *bin;

        int last_width;

        GtkWidget *selected_page;
        GList *pages; /* GList of GtkWidgets */
        GList *removed_pages; /* GList of RemoveData, see setup_delayed_remove() */
};

enum
{
        PROP_0,
        PROP_CURRENT_PAGE,
        LAST_PROP
};

static GParamSpec *obj_props[LAST_PROP] = { NULL, };

static void
cc_notebook_buildable_add_child (GtkBuildable  *buildable,
                                 GtkBuilder    *builder,
                                 GObject       *child,
                                 const gchar   *type);

static void
cc_notebook_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (CcNotebook, cc_notebook, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                cc_notebook_buildable_init))

static void
cc_notebook_get_property (GObject    *gobject,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
        CcNotebookPrivate *priv = CC_NOTEBOOK (gobject)->priv;

        switch (prop_id) {
        case PROP_CURRENT_PAGE:
                g_value_set_pointer (value, priv->selected_page);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
        }
}

static void
cc_notebook_set_property (GObject      *gobject,
			  guint         prop_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
        CcNotebook *self = CC_NOTEBOOK (gobject);

        switch (prop_id) {
        case PROP_CURRENT_PAGE:
                cc_notebook_select_page (self, g_value_get_pointer (value), TRUE);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
        }
}

static void
cc_notebook_finalize (GObject *gobject)
{
        CcNotebook *self = CC_NOTEBOOK (gobject);

	g_list_free_full (self->priv->removed_pages, (GDestroyNotify) g_free);
	self->priv->removed_pages = NULL;

	g_list_free (self->priv->pages);
	self->priv->pages = NULL;

	G_OBJECT_CLASS (cc_notebook_parent_class)->finalize (gobject);
}

static GtkSizeRequestMode
cc_notebook_get_request_mode (GtkWidget *widget)
{
	CcNotebook *notebook;
	GtkWidget *target;

	notebook = CC_NOTEBOOK (widget);

	target = notebook->priv->selected_page ? notebook->priv->selected_page : notebook->priv->embed;

	return gtk_widget_get_request_mode (target);
}

static void
cc_notebook_get_preferred_height (GtkWidget       *widget,
				  gint            *minimum_height,
				  gint            *natural_height)
{
	CcNotebook *notebook;
	GList *l;

	notebook = CC_NOTEBOOK (widget);

	if (notebook->priv->selected_page == NULL) {
		gtk_widget_get_preferred_height (notebook->priv->embed, minimum_height, natural_height);
		return;
	}

	*minimum_height = 0;
	*natural_height = 0;
	for (l = notebook->priv->pages; l != NULL; l = l->next) {
		GtkWidget *page = l->data;
		int page_min, page_nat;

		gtk_widget_get_preferred_height (page, &page_min, &page_nat);
		*minimum_height = MAX(page_min, *minimum_height);
		*natural_height = MAX(page_nat, *natural_height);
	}
}

static void
cc_notebook_get_preferred_width_for_height (GtkWidget       *widget,
					    gint             height,
					    gint            *minimum_width,
					    gint            *natural_width)
{
	CcNotebook *notebook;
	GList *l;

	notebook = CC_NOTEBOOK (widget);

	if (notebook->priv->selected_page == NULL) {
		gtk_widget_get_preferred_width_for_height (notebook->priv->embed, height, minimum_width, natural_width);
		return;
	}

	*minimum_width = 0;
	*natural_width = 0;
	for (l = notebook->priv->pages; l != NULL; l = l->next) {
		GtkWidget *page = l->data;
		int page_min, page_nat;

		gtk_widget_get_preferred_width_for_height (page, height, &page_min, &page_nat);
		*minimum_width = MAX(page_min, *minimum_width);
		*natural_width = MAX(page_nat, *natural_width);
	}
}

static void
cc_notebook_get_preferred_width (GtkWidget       *widget,
				 gint            *minimum_width,
				 gint            *natural_width)
{
	CcNotebook *notebook;
	GList *l;

	notebook = CC_NOTEBOOK (widget);

	if (notebook->priv->selected_page == NULL) {
		gtk_widget_get_preferred_width (notebook->priv->embed, minimum_width, natural_width);
		return;
	}

	*minimum_width = 0;
	*natural_width = 0;
	for (l = notebook->priv->pages; l != NULL; l = l->next) {
		GtkWidget *page = l->data;
		int page_min, page_nat;

		gtk_widget_get_preferred_width (page, &page_min, &page_nat);
		*minimum_width = MAX(page_min, *minimum_width);
		*natural_width = MAX(page_nat, *natural_width);
	}
}

static void
cc_notebook_get_preferred_height_for_width (GtkWidget       *widget,
					    gint             width,
					    gint            *minimum_height,
					    gint            *natural_height)
{
	CcNotebook *notebook;
	GList *l;

	notebook = CC_NOTEBOOK (widget);

	if (notebook->priv->selected_page == NULL) {
		gtk_widget_get_preferred_height_for_width (notebook->priv->embed, width, minimum_height, natural_height);
		return;
	}

	*minimum_height = 0;
	*natural_height = 0;
	for (l = notebook->priv->pages; l != NULL; l = l->next) {
		GtkWidget *page = l->data;
		int page_min, page_nat;

		gtk_widget_get_preferred_height_for_width (page, width, &page_min, &page_nat);
		*minimum_height = MAX(page_min, *minimum_height);
		*natural_height = MAX(page_nat, *natural_height);
	}
}

static gboolean
cc_notebook_focus (GtkWidget        *widget,
		   GtkDirectionType  direction)
{
	CcNotebook *notebook;
	GtkWidget *child;

	notebook = CC_NOTEBOOK (widget);
	child = notebook->priv->selected_page;

	if (child == NULL)
		return FALSE;

	/* HACK: the default GtkContainer implementation is fine by us
	 * and there's no way to get to it without excessive copy/paste */
	return GTK_WIDGET_GET_CLASS (child)->focus (child, direction);
}

static void
cc_notebook_class_init (CcNotebookClass *klass)
{
        GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        g_type_class_add_private (klass, sizeof (CcNotebookPrivate));

        obj_props[PROP_CURRENT_PAGE] =
                g_param_spec_pointer (g_intern_static_string ("current-page"),
				      "Current Page",
				      "The currently selected page widget",
				      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

        gobject_class->get_property = cc_notebook_get_property;
        gobject_class->set_property = cc_notebook_set_property;
        gobject_class->finalize = cc_notebook_finalize;
        g_object_class_install_properties (gobject_class, LAST_PROP, obj_props);

	widget_class->get_request_mode = cc_notebook_get_request_mode;
	widget_class->get_preferred_height = cc_notebook_get_preferred_height;
	widget_class->get_preferred_width_for_height = cc_notebook_get_preferred_width_for_height;
	widget_class->get_preferred_width = cc_notebook_get_preferred_width;
	widget_class->get_preferred_height_for_width = cc_notebook_get_preferred_height_for_width;
	widget_class->focus = cc_notebook_focus;
}

static void
on_embed_size_allocate (GtkWidget     *embed,
                        GtkAllocation *allocation,
                        CcNotebook  *self)
{
        ClutterActorIter iter;
        ClutterActor *child;
        ClutterActor *frame;
        float page_w, page_h;
        float offset = 0.f;
        ClutterPoint pos;

        if (self->priv->selected_page == NULL)
		return;

        self->priv->last_width = allocation->width;

        page_w = allocation->width;
        page_h = allocation->height;

        clutter_actor_iter_init (&iter, self->priv->bin);
        while (clutter_actor_iter_next (&iter, &child)) {
                clutter_actor_set_x (child, offset);
                clutter_actor_set_size (child, page_w, page_h);

                offset += page_w;
        }

	/* This stops the non-animated scrolling from happening
	 * if we're still scrolling there */
	if (clutter_actor_get_transition (self->priv->scroll, "scroll-to") != NULL)
		return;

	frame = g_object_get_data (G_OBJECT (self->priv->selected_page),
				   "cc-notebook-frame");

        pos.y = 0;
        pos.x = clutter_actor_get_x (frame);
        g_debug ("Scrolling to (%lf,%lf) in allocation", pos.x, pos.y);
        clutter_scroll_actor_scroll_to_point (CLUTTER_SCROLL_ACTOR (self->priv->scroll), &pos);
}

static void
cc_notebook_init (CcNotebook *self)
{
        self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, CC_TYPE_NOTEBOOK, CcNotebookPrivate);

        gtk_widget_set_can_focus (GTK_WIDGET (self), TRUE);

        self->priv->embed = gtk_clutter_embed_new ();
        gtk_widget_push_composite_child ();
        gtk_container_add (GTK_CONTAINER (self), self->priv->embed);
        gtk_widget_pop_composite_child ();
        g_signal_connect (self->priv->embed, "size-allocate", G_CALLBACK (on_embed_size_allocate), self);
        gtk_widget_show (self->priv->embed);

        self->priv->stage = gtk_clutter_embed_get_stage (GTK_CLUTTER_EMBED (self->priv->embed));

        self->priv->scroll = clutter_scroll_actor_new ();
        clutter_scroll_actor_set_scroll_mode (CLUTTER_SCROLL_ACTOR (self->priv->scroll),
                                                CLUTTER_SCROLL_HORIZONTALLY);
        clutter_actor_add_constraint (self->priv->scroll, clutter_bind_constraint_new (self->priv->stage, CLUTTER_BIND_SIZE, 0.f));
        clutter_actor_add_child (self->priv->stage, self->priv->scroll);

        self->priv->bin = clutter_actor_new ();
        clutter_actor_add_child (self->priv->scroll, self->priv->bin);

        self->priv->selected_page = NULL;
        gtk_widget_set_name (GTK_WIDGET (self), "GtkBox");
}

GtkWidget *
cc_notebook_new (void)
{
        return g_object_new (CC_TYPE_NOTEBOOK, NULL);
}

static void
remove_transition_on_complete (ClutterTransition *transition,
                               gpointer           user_data)
{
        ClutterActor *actor = CLUTTER_ACTOR (user_data);
        clutter_actor_remove_transition (actor, "scroll-to");
}

static void
_cc_notebook_select_page (CcNotebook *self,
			  GtkWidget  *widget,
			  int         index,
			  gboolean    animate)
{
        ClutterPoint pos;
        ClutterTransition *transition;

        g_return_if_fail (CC_IS_NOTEBOOK (self));
        g_return_if_fail (GTK_IS_WIDGET (widget));

        pos.y = 0;
        pos.x = self->priv->last_width * index;

        if (clutter_actor_get_transition (self->priv->scroll, "scroll-to") != NULL) {
                g_debug ("Cancelling previous scroll animation");
                clutter_actor_remove_transition (self->priv->scroll, "scroll-to");
        }

        clutter_actor_save_easing_state (self->priv->scroll);
        if (animate)
		clutter_actor_set_easing_duration (self->priv->scroll, 500);
	else
		clutter_actor_set_easing_duration (self->priv->scroll, 0);

        g_debug ("Scrolling to (%lf,%lf) %s animation in page selection", pos.x, pos.y,
		 animate ? "with" : "without");
        clutter_scroll_actor_scroll_to_point (CLUTTER_SCROLL_ACTOR (self->priv->scroll), &pos);

        transition = clutter_actor_get_transition (self->priv->scroll, "scroll-to");
        if (transition != NULL) {
                g_signal_connect (transition, "completed",
                                  G_CALLBACK (remove_transition_on_complete), self->priv->scroll);
        }

	clutter_actor_restore_easing_state (self->priv->scroll);

        /* Remember the last selected page */
        self->priv->selected_page = widget;

        g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_CURRENT_PAGE]);
}

void
cc_notebook_select_page (CcNotebook *self,
                         GtkWidget  *widget,
                         gboolean    animate)
{
	int i, n_children;
	GList *children, *l;
	ClutterActor *frame;
	gboolean found;

        if (widget == self->priv->selected_page)
		return;

	found = FALSE;
	frame = g_object_get_data (G_OBJECT (widget), "cc-notebook-frame");

        n_children = clutter_actor_get_n_children (self->priv->bin);
        children = clutter_actor_get_children (self->priv->bin);
        for (i = 0, l = children; i < n_children; i++, l = l->next) {
		if (frame == l->data) {
			_cc_notebook_select_page (self, widget, i, animate);
			found = TRUE;
			break;
		}
	}
	g_list_free (children);
	if (found == FALSE)
		g_warning ("Could not find widget '%p' in CcNotebook '%p'", widget, self);
}

static void
widget_destroyed (GtkWidget *widget,
                  gpointer   user_data)
{
        CcNotebook *notebook = g_object_get_data (G_OBJECT (widget), "cc-notebook");
        cc_notebook_remove_page (notebook, widget);
}

void
cc_notebook_add_page (CcNotebook *self,
                      GtkWidget  *widget)
{
        ClutterActor *frame;
        ClutterActor *embed;
        int res;

        g_return_if_fail (CC_IS_NOTEBOOK (self));
        g_return_if_fail (GTK_IS_WIDGET (widget));

        frame = clutter_actor_new ();
        clutter_actor_set_layout_manager (frame, clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_FILL,
                                                                         CLUTTER_BIN_ALIGNMENT_FILL));

        embed = gtk_clutter_actor_new_with_contents (widget);
        g_object_set_data (G_OBJECT (widget), "cc-notebook-frame", frame);
        g_object_set_data (G_OBJECT (widget), "cc-notebook", self);
        g_signal_connect (widget, "destroy", G_CALLBACK (widget_destroyed), NULL);
        clutter_actor_add_child (frame, embed);
        gtk_widget_show (widget);

        res = clutter_actor_get_n_children (self->priv->bin);
        clutter_actor_insert_child_at_index (self->priv->bin, frame, res);

        self->priv->pages = g_list_prepend (self->priv->pages, widget);

        if (self->priv->selected_page == NULL)
		_cc_notebook_select_page (self, widget, res, FALSE);

        gtk_widget_queue_resize (GTK_WIDGET (self));
}

typedef struct {
	CcNotebook   *notebook;
	ClutterActor *frame;
} RemoveData;

static void
remove_on_complete (ClutterTimeline *timeline,
		    RemoveData      *data)
{
	data->notebook->priv->removed_pages = g_list_remove (data->notebook->priv->removed_pages, data);
	clutter_actor_destroy (data->frame);
	g_free (data);
}

static gboolean
setup_delayed_remove (CcNotebook   *self,
		      ClutterActor *frame)
{
        ClutterTransition *transition;
        RemoveData *data;

	transition = clutter_actor_get_transition (self->priv->scroll, "scroll-to");
	if (transition == NULL)
		return FALSE;

	data = g_new0 (RemoveData, 1);
	data->notebook = self;
	data->frame = frame;

	self->priv->removed_pages = g_list_prepend (self->priv->removed_pages, data);
	g_signal_connect (transition, "completed",
			  G_CALLBACK (remove_on_complete), data);

	return TRUE;
}

void
cc_notebook_remove_page (CcNotebook *self,
                         GtkWidget  *widget)
{
        ClutterActorIter iter;
        ClutterActor *child, *frame;
        int index;

        g_return_if_fail (CC_IS_NOTEBOOK (self));
        g_return_if_fail (GTK_IS_WIDGET (widget));

        frame = g_object_get_data (G_OBJECT (widget), "cc-notebook-frame");

	index = 0;
        clutter_actor_iter_init (&iter, self->priv->bin);
        while (clutter_actor_iter_next (&iter, &child)) {
                if (frame == child) {
			if (setup_delayed_remove (self, frame) == FALSE)
				clutter_actor_iter_remove (&iter);
                        break;
		}

		index++;
        }

        self->priv->pages = g_list_remove (self->priv->pages, widget);

        if (widget == self->priv->selected_page)
                self->priv->selected_page = NULL;

        gtk_widget_queue_resize (GTK_WIDGET (self));
}

GtkWidget *
cc_notebook_get_selected_page (CcNotebook *self)
{
        g_return_val_if_fail (CC_IS_NOTEBOOK (self), NULL);

        return self->priv->selected_page;
}

static void
cc_notebook_buildable_add_child (GtkBuildable  *buildable,
                                 GtkBuilder    *builder,
                                 GObject       *child,
                                 const gchar   *type)
{
        CcNotebook *notebook = CC_NOTEBOOK (buildable);
        cc_notebook_add_page (notebook, GTK_WIDGET (child));
}

static void
cc_notebook_buildable_init (GtkBuildableIface *iface)
{
        iface->add_child = cc_notebook_buildable_add_child;
}
