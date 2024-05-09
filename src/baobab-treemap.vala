/* -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* Baobab - disk usage analyzer
 *
 * Copyright (C) 2008  Igalia
 * Copyright (C) 2013  Stefano Facchini <stefano.facchini@gmail.com>
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
 * Authors of the original code:
 *   Fabio Marzocca  <thesaltydog@gmail.com>
 *   Paolo Borelli <pborelli@katamail.com>
 *   Miguel Gomez <magomez@igalia.com>
 *   Eduardo Lima Mitev <elima@igalia.com>
 */

namespace Baobab {

    class TreemapItem : ChartItem {
        public Cairo.Rectangle cr_rect;
    }

    public class Treemap : Chart {

        static construct {
            set_css_name ("treemap");
        }

        const int ITEM_BORDER_WIDTH = 1;
        const int ITEM_PADDING = 6;
        const int ITEM_TEXT_PADDING = 3;

        const int ITEM_MIN_WIDTH = 3;
        const int ITEM_MIN_HEIGHT = 3;

        uint max_visible_depth;
        bool more_visible_children;

        protected override ChartItem create_new_chartitem () {
            return (new TreemapItem () as ChartItem);
        }

        void draw_rectangle (Cairo.Context cr,
                             double x,
                             double y,
                             double width,
                             double height,
                             Gdk.RGBA fill_color,
                             string text,
                             bool show_text) {
            uint border = ITEM_BORDER_WIDTH;

            var context = get_style_context ();

            cr.set_line_width (border);
            cr.rectangle (x + border, y + border, width - border * 2, height - border * 2);
            Gdk.cairo_set_source_rgba (cr, fill_color);
            cr.fill ();
            context.render_frame (cr, x + 0.5, y + 0.5, width - 1, height - 1);

            if (show_text) {
                var layout = create_pango_layout (null);
                var markup = Markup.escape_text (text);
                layout.set_markup (markup, -1);

                Pango.Rectangle rect;
                layout.get_pixel_extents (null, out rect);

                if ((rect.width + ITEM_TEXT_PADDING * 2 <= width) &&
                    (rect.height + ITEM_TEXT_PADDING * 2 <= height)) {
                    context.render_layout (cr, x + width / 2 - rect.width / 2, y + height / 2 - rect.height / 2, layout);
                }
            }
        }

        protected override void draw_item (Cairo.Context cr, ChartItem item, bool highlighted) {
            Cairo.Rectangle rect;
            Gdk.RGBA fill_color = Gdk.RGBA ();
            Gtk.Allocation allocation;
            double width = 0, height = 0;

            rect = ((TreemapItem) item).cr_rect;
            get_allocation (out allocation);

            if ((item.depth % 2) != 0) {
                fill_color = get_item_color (rect.x / allocation.width * 200,
                                             item.depth, highlighted);
                width = rect.width - ITEM_PADDING;
                height = rect.height;
            } else {
                fill_color = get_item_color (rect.y / allocation.height * 200,
                                             item.depth, highlighted);
                width = rect.width;
                height = rect.height - ITEM_PADDING;
            }

            draw_rectangle (cr, rect.x, rect.y, width, height, fill_color, item.results.display_name, (!item.has_visible_children));
        }

        protected override void calculate_item_geometry (ChartItem item) {
            TreemapItem treemapitem = (TreemapItem) item;
            Cairo.Rectangle p_area = Cairo.Rectangle ();

            if (item.depth == 0) {
                max_visible_depth = 0;
                more_visible_children = false;
            }

            item.visible = false;
            if (item.parent == null) {
                Gtk.Allocation allocation;
                get_allocation (out allocation);
                p_area.x = -ITEM_PADDING / 2;
                p_area.y = -ITEM_PADDING / 2;
                p_area.width = allocation.width + ITEM_PADDING * 2;
                p_area.height = allocation.height + ITEM_PADDING * 2;
            } else {
                p_area = ((TreemapItem) item.parent.data).cr_rect;
            }

            if (item.depth % 2 != 0) {
                var width = p_area.width - ITEM_PADDING;

                treemapitem.cr_rect.x = p_area.x + (item.rel_start * width / 100) + ITEM_PADDING;
                treemapitem.cr_rect.y = p_area.y + ITEM_PADDING;
                treemapitem.cr_rect.width = width * item.rel_size / 100;
                treemapitem.cr_rect.height = p_area.height - ITEM_PADDING * 3;
            } else {
                var height = p_area.height - ITEM_PADDING;

                treemapitem.cr_rect.x = p_area.x + ITEM_PADDING;
                treemapitem.cr_rect.y = p_area.y + (item.rel_start * height / 100) + ITEM_PADDING;
                treemapitem.cr_rect.width = p_area.width - ITEM_PADDING * 3;
                treemapitem.cr_rect.height = height * item.rel_size / 100;
            }
            if ((treemapitem.cr_rect.width - ITEM_PADDING < ITEM_MIN_WIDTH) ||
                (treemapitem.cr_rect.height - ITEM_PADDING < ITEM_MIN_HEIGHT)) {
                return;
            }

            treemapitem.cr_rect.x = Math.floor (treemapitem.cr_rect.x) + 0.5;
            treemapitem.cr_rect.y = Math.floor (treemapitem.cr_rect.y) + 0.5;
            treemapitem.cr_rect.width = Math.floor (treemapitem.cr_rect.width);
            treemapitem.cr_rect.height = Math.floor (treemapitem.cr_rect.height);

            item.visible = true;

            if (item.parent != null) {
                item.parent.data.has_visible_children = true;
            }

            get_item_rectangle (item);

            if (item.depth == max_depth + 1) {
                more_visible_children = true;
            } else {
                max_visible_depth = uint.max (max_visible_depth, item.depth);
            }
        }

        protected override void get_item_rectangle (ChartItem item) {
            var crect = ((TreemapItem) item).cr_rect;

            item.rect.x = (int) crect.x;
            item.rect.y = (int) crect.y;

            if (item.depth % 2 != 0) {
                item.rect.width = (int) crect.width - ITEM_PADDING;
                item.rect.height = (int) crect.height;
            } else {
                item.rect.width = (int) crect.width;
                item.rect.height = (int) crect.height - ITEM_PADDING;
            }
        }

        protected override bool is_point_over_item (ChartItem item, double x, double y) {
            var rect = item.rect;
            return ((x >= rect.x) && (x <= (rect.x + rect.width)) &&
                    (y >= rect.y) && (y <= (rect.y + rect.height)));
        }

        protected override bool can_zoom_out () {
            return (max_depth < MAX_DEPTH) && more_visible_children;
        }

        protected override bool can_zoom_in () {
            return (max_visible_depth > 1);
        }
    }
}
