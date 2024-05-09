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

namespace Baobab {

    class RingschartItem : ChartItem {
        public double min_radius;
        public double max_radius;
        public double start_angle;
        public double angle;
        public bool continued;
    }

    public class Ringschart : Chart {

        const int ITEM_BORDER_WIDTH = 1;
        const double ITEM_MIN_ANGLE = 0.03;
        const double EDGE_ANGLE = 0.004;

        // Twice the GTK+ tooltip timeout
        const int SUBTIP_TIMEOUT = 1000;

        uint tips_timeout_id = 0;
        bool drawing_subtips = false;
        List<ChartItem> subtip_items;

        void subtips_update () {
            if (drawing_subtips) {
                queue_draw ();
            }
            drawing_subtips = false;

            if (tips_timeout_id != 0) {
                Source.remove (tips_timeout_id);
                tips_timeout_id = 0;
            }

            subtip_items = null;

            if (highlighted_item != null) {
                tips_timeout_id = Timeout.add (SUBTIP_TIMEOUT, () => {
                    drawing_subtips = true;
                    tips_timeout_id = 0;
                    queue_draw ();
                    return false;
                });
            }
        }

        static construct {
            set_css_name ("ringschart");
        }

        construct {
            notify["max-depth"].connect (subtips_update);
            notify["highlighted-item"].connect (subtips_update);
            notify["root"].connect (subtips_update);
        }

        protected override ChartItem create_new_chartitem () {
            return (new RingschartItem () as ChartItem);
        }

        protected override void post_draw (Cairo.Context cr) {
            if (!drawing_subtips) {
                return;
            }

            var context = get_style_context ();
            context.save ();
            context.add_class ("subfolder-tip");

            Gtk.Allocation allocation;
            get_allocation (out allocation);

            var q_width = allocation.width / 2;
            var q_height = allocation.height / 2;
            var q_angle = Math.atan2 (q_height, q_width);

            Gdk.Rectangle last_rect = Gdk.Rectangle ();

            var padding = context.get_padding ();
            var vpadding = padding.top + padding.bottom;
            var hpadding = padding.left + padding.right;

            foreach (ChartItem item in subtip_items) {
                RingschartItem ringsitem = item as RingschartItem;

                // get the middle angle
                var middle_angle = ringsitem.start_angle + ringsitem.angle / 2;
                // normalize the middle angle (take it to the first quadrant
                var middle_angle_n = middle_angle;
                while (middle_angle_n > Math.PI / 2) {
                    middle_angle_n -= Math.PI;
                }
                middle_angle_n = Math.fabs (middle_angle_n);

                // get the pango layout and its enclosing rectangle
                var layout = create_pango_layout (null);
                var markup = "<span size=\"small\">" + Markup.escape_text (item.results.display_name) + "</span>";
                layout.set_markup (markup, -1);
                layout.set_indent (0);
                layout.set_spacing (0);
                layout.set_width (Pango.SCALE * q_width / 2);
                layout.set_ellipsize (Pango.EllipsizeMode.END);
                Pango.Rectangle layout_rect;
                layout.get_pixel_extents (null, out layout_rect);

                // get the center point of the tooltip rectangle
                double tip_x, tip_y;
                if (middle_angle_n < q_angle) {
                    tip_x = q_width - layout_rect.width / 2 - hpadding;
                    tip_y = Math.tan (middle_angle_n) * tip_x;
                } else {
                    tip_y = q_height - layout_rect.height / 2 - vpadding;
                    tip_x = tip_y / Math.tan (middle_angle_n);
                }

                // get the tooltip rectangle
                Cairo.Rectangle tooltip_rect = Cairo.Rectangle ();
                tooltip_rect.x = q_width + tip_x - layout_rect.width / 2 - padding.left;
                tooltip_rect.y = q_height + tip_y - layout_rect.height / 2 - padding.top;
                tooltip_rect.width = layout_rect.width + hpadding;
                tooltip_rect.height = layout_rect.height + vpadding;

                // translate tooltip rectangle and edge angles to the original quadrant
                var a = middle_angle;
                int i = 0;
                while (a > Math.PI / 2) {
                    if (i % 2 == 0) {
                        tooltip_rect.x = allocation.width - tooltip_rect.x - tooltip_rect.width;
                    } else {
                        tooltip_rect.y = allocation.height - tooltip_rect.y - tooltip_rect.height;
                    }
                    i++;
                    a -= Math.PI / 2;
                }

                // get the Gdk.Rectangle of the tooltip (with a little padding)
                Gdk.Rectangle _rect = Gdk.Rectangle ();
                _rect.x = (int) (tooltip_rect.x - 1);
                _rect.y = (int) (tooltip_rect.y - 1);
                _rect.width = (int) (tooltip_rect.width + 2);
                _rect.height = (int) (tooltip_rect.height + 2);

                // check if tooltip overlaps
                if (!_rect.intersect (last_rect, null)) {
                    last_rect = _rect;

                    // finally draw the tooltip!

                    // avoid blurred lines
                    tooltip_rect.x = Math.floor (tooltip_rect.x);
                    tooltip_rect.y = Math.floor (tooltip_rect.y);

                    var middle_radius = ringsitem.min_radius + (ringsitem.max_radius - ringsitem.min_radius) / 2;
                    var sector_center_x = q_width + middle_radius * Math.cos (middle_angle);
                    var sector_center_y = q_height + middle_radius * Math.sin (middle_angle);

                    // draw line from sector center to tooltip center
                    cr.save ();

                    // clip to avoid drawing inside tooltip area (tooltip background
                    // could be transparent, depending on the theme)
                    cr.rectangle (0, 0, allocation.width, allocation.height);
                    cr.rectangle (tooltip_rect.x + tooltip_rect.width, tooltip_rect.y, -tooltip_rect.width, tooltip_rect.height);
                    cr.clip ();

                    Gdk.RGBA bg_color = { 0, 0, 0, (float) 0.8 };
                    cr.set_line_width (1);
                    cr.move_to (sector_center_x, sector_center_y);
                    Gdk.cairo_set_source_rgba (cr, bg_color);
                    cr.line_to (tooltip_rect.x + tooltip_rect.width / 2,
                                tooltip_rect.y + tooltip_rect.height / 2);
                    cr.stroke ();

                    cr.restore ();

                    // draw a tiny circle in sector center
                    cr.arc (sector_center_x, sector_center_y, 1.0, 0, 2 * Math.PI);
                    cr.stroke ();

                    // draw tooltip box
                    context.render_background (cr, tooltip_rect.x, tooltip_rect.y, tooltip_rect.width, tooltip_rect.height);
                    context.render_frame (cr, tooltip_rect.x, tooltip_rect.y, tooltip_rect.width, tooltip_rect.height);
                    context.render_layout (cr, tooltip_rect.x + padding.left, tooltip_rect.y + padding.top, layout);
                }
            }

            context.restore ();
        }

        protected override void draw_item (Cairo.Context cr, ChartItem item, bool highlighted) {
            RingschartItem ringsitem = item as RingschartItem;

            if (drawing_subtips) {
                if (highlighted_item != null &&
                    item.parent != null &&
                    item.parent.data == highlighted_item) {
                    subtip_items.append (item);
                }
            }

            cr.set_line_width (ITEM_BORDER_WIDTH);

            Gtk.Allocation allocation;
            get_allocation (out allocation);

            var context = get_style_context ();

            Gdk.RGBA border_color;
            Gdk.RGBA bg_color;
            context.lookup_color ("chart_borders", out border_color);
            context.lookup_color ("theme_bg_color", out bg_color);

            var center_x = allocation.width / 2;
            var center_y = allocation.height / 2;
            var final_angle = ringsitem.start_angle + ringsitem.angle;

            if (item.depth == 0) {
                // draw a label with the size of the folder in the middle
                // of the central disk
                var layout = create_pango_layout (null);
                var size = format_size (item.results.size);
                var markup = "<span size=\"small\">" + Markup.escape_text (size) + "</span>";
                layout.set_markup (markup, -1);
                layout.set_indent (0);
                layout.set_spacing (0);
                Pango.Rectangle layout_rect;
                layout.get_pixel_extents (null, out layout_rect);

                if (layout_rect.width < 2 * ringsitem.max_radius) {
                    context.render_layout (cr, center_x - layout_rect.width / 2, center_y - layout_rect.height / 2, layout);
                    cr.move_to (center_x + ringsitem.max_radius + 1, center_y);
                }

                cr.arc (center_x, center_y, ringsitem.max_radius + 1, 0, 2 * Math.PI);
                Gdk.cairo_set_source_rgba (cr, border_color);
                cr.stroke ();
            } else {
                var fill_color = get_item_color (ringsitem.start_angle / Math.PI * 99,
                                                 item.depth,
                                                 highlighted);

                cr.arc (center_x, center_y, ringsitem.min_radius, ringsitem.start_angle, final_angle);
                cr.arc_negative (center_x, center_y, ringsitem.max_radius, final_angle, ringsitem.start_angle);

                cr.close_path ();

                Gdk.cairo_set_source_rgba (cr, fill_color);
                cr.fill_preserve ();
                Gdk.cairo_set_source_rgba (cr, bg_color);
                cr.stroke ();

                if (ringsitem.continued) {
                    Gdk.cairo_set_source_rgba (cr, border_color);
                    cr.set_line_width (3);
                    cr.arc (center_x, center_y, ringsitem.max_radius + 4, ringsitem.start_angle + EDGE_ANGLE, final_angle - EDGE_ANGLE);
                    cr.stroke ();
                }
            }
        }

        protected override void calculate_item_geometry (ChartItem item) {
            RingschartItem ringsitem = item as RingschartItem;

            ringsitem.continued = false;
            ringsitem.visible = false;

            Gtk.Allocation allocation;
            get_allocation (out allocation);

            var context = get_style_context ();
            var padding = context.get_padding ();
            var max_radius = int.min (allocation.width / 2, allocation.height / 2) - padding.left; // Assuming that padding is the same for all sides
            var thickness = max_radius / (max_depth + 1);

            if (ringsitem.parent == null) {
                ringsitem.min_radius = 0;
                ringsitem.max_radius = thickness;
                ringsitem.start_angle = 0;
                ringsitem.angle = 2 * Math.PI;
            } else {
                var parent = item.parent.data as RingschartItem;
                ringsitem.min_radius = ringsitem.depth * thickness;
                if (ringsitem.depth > max_depth) {
                    return;
                } else {
                    ringsitem.max_radius = ringsitem.min_radius + thickness;
                }

                ringsitem.angle = parent.angle * ringsitem.rel_size / 100;
                if (ringsitem.angle < ITEM_MIN_ANGLE) {
                    return;
                }

                ringsitem.start_angle = parent.start_angle + parent.angle * ringsitem.rel_start / 100;
                ringsitem.continued = (!ringsitem.results.is_empty) && (ringsitem.depth == max_depth);
                parent.has_visible_children = true;
            }

            ringsitem.visible = true;
            get_item_rectangle (item);
        }

        void get_point_min_rect (double cx, double cy, double radius, double angle, ref Gdk.Rectangle r) {
            double x, y;
            x = cx + Math.cos (angle) * radius;
            y = cy + Math.sin (angle) * radius;

            r.x = int.min (r.x, (int)x);
            r.y = int.min (r.y, (int)y);
            r.width = int.max (r.width, (int)x);
            r.height = int.max (r.height, (int)y);
        }

        protected override void get_item_rectangle (ChartItem item) {
            RingschartItem ringsitem = item as RingschartItem;
            Gdk.Rectangle rect = Gdk.Rectangle ();
            double cx, cy, r1, r2, a1, a2;
            Gtk.Allocation allocation;

            get_allocation (out allocation);
            cx = allocation.width / 2;
            cy = allocation.height / 2;
            r1 = ringsitem.min_radius;
            r2 = ringsitem.max_radius;
            a1 = ringsitem.start_angle;
            a2 = ringsitem.start_angle + ringsitem.angle;

            rect.x = allocation.width;
            rect.y = allocation.height;
            rect.width = 0;
            rect.height = 0;

            get_point_min_rect (cx, cy, r1, a1, ref rect);
            get_point_min_rect (cx, cy, r2, a1, ref rect);
            get_point_min_rect (cx, cy, r1, a2, ref rect);
            get_point_min_rect (cx, cy, r2, a2, ref rect);

            if ((a1 <= Math.PI / 2) && (a2 >= Math.PI / 2)) {
                rect.height = (int) double.max (rect.height, cy + Math.sin (Math.PI / 2) * r2);
            }

            if ((a1 <= Math.PI) && (a2 >= Math.PI)) {
                rect.x = (int) double.min (rect.x, cx + Math.cos (Math.PI) * r2);
            }

            if ((a1 <= Math.PI * 1.5) && (a2 >= Math.PI * 1.5)) {
                rect.y = (int) double.min (rect.y, cy + Math.sin (Math.PI * 1.5) * r2);
            }

            if ((a1 <= Math.PI * 2) && (a2 >= Math.PI * 2)) {
                rect.width = (int) double.max (rect.width, cx + Math.cos (Math.PI * 2) * r2);
            }

            rect.width -= rect.x;
            rect.height -= rect.y;

            ringsitem.rect = rect;
        }

        protected override bool is_point_over_item (ChartItem item, double x, double y) {
            var ringsitem = item as RingschartItem;

            Gtk.Allocation allocation;
            get_allocation (out allocation);

            x -= allocation.width / 2;
            y -= allocation.height / 2;

            var radius = Math.sqrt (x * x + y * y);
            var angle = Math.atan2 (y, x);
            angle = (angle > 0) ? angle : (angle + 2 * Math.PI);

            return ((radius >= ringsitem.min_radius) &&
                    (radius <= ringsitem.max_radius) &&
                    (angle >= ringsitem.start_angle) &&
                    (angle <= (ringsitem.start_angle + ringsitem.angle)));
        }

        protected override bool can_zoom_out () {
            return (max_depth < MAX_DEPTH);
        }

        protected override bool can_zoom_in () {
            return (max_depth > MIN_DEPTH);
        }
    }
}
