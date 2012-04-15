/* -*- indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* Baobab - disk usage analyzer
 *
 * Copyright (C) 2012  Stefano Facchini <stefano.facchini@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

namespace Baobab {

    public abstract class BaseLocationWidget : Gtk.Bin {

        protected static Gtk.SizeGroup widget_size_group = null;

        protected void ensure_size_group () {
            if (widget_size_group != null)
                return;

            widget_size_group = new Gtk.SizeGroup (Gtk.SizeGroupMode.HORIZONTAL | Gtk.SizeGroupMode.VERTICAL);
        }

        public delegate void ActionOnClick (Location? location = null);

        protected override void get_preferred_width (out int minimum, out int natural) {
            get_child ().get_preferred_width (out minimum, out natural);

            var state = get_state_flags ();
            var border = get_style_context ().get_padding (state);
            minimum = minimum + border.left + border.right;
            natural = natural + border.left + border.right;
        }

        protected override void get_preferred_height (out int minimum, out int natural) {
            get_child ().get_preferred_height (out minimum, out natural);

            var state = get_state_flags ();
            var border = get_style_context ().get_padding (state);
            minimum = minimum + border.top + border.bottom;
            natural = natural + border.top + border.bottom;
        }

        protected override void size_allocate (Gtk.Allocation alloc) {
            set_allocation (alloc);

            var state = get_state_flags ();
            var border = get_style_context ().get_padding (state);

            var adjusted_alloc = Gtk.Allocation ();
            adjusted_alloc.x = alloc.x + border.left;
            adjusted_alloc.y = alloc.y + border.top;
            adjusted_alloc.width = alloc.width - border.left - border.right;
            adjusted_alloc.height = alloc.height - border.top - border.bottom;

            get_child ().size_allocate (adjusted_alloc);
        }

        protected override bool draw (Cairo.Context cr) {
            Gtk.Allocation alloc;
            get_allocation (out alloc);

            get_style_context ().render_background (cr, 0, 0, alloc.width, alloc.height);
            get_style_context ().render_frame (cr, 0, 0, alloc.width, alloc.height);

            base.draw (cr);

            return false;
        }
    }

    public class LocationWidget : BaseLocationWidget {

        const int ICON_SIZE = 96;

        Gtk.Grid grid;

        public LocationWidget (Location location, BaseLocationWidget.ActionOnClick action) {
            base ();

            grid = new Gtk.Grid () {
                orientation = Gtk.Orientation.VERTICAL,
                row_spacing = 20,
                column_spacing = 10
            };

            add (grid);

            ensure_size_group ();

            var icon_theme = Gtk.IconTheme.get_default ();
            var icon_info = icon_theme.lookup_by_gicon (location.icon, ICON_SIZE, 0);

            var first_row = new Gtk.Grid () {
                orientation = Gtk.Orientation.HORIZONTAL
            };

            try {
                var pixbuf = icon_info.load_icon ();
                var image = new Gtk.Image.from_pixbuf (pixbuf);
                first_row.add (image);
            } catch (Error e) {
                warning ("Failed to load icon %s: %s", location.icon.to_string(), e.message);
            }

            var name_grid = new Gtk.Grid () { orientation = Gtk.Orientation.VERTICAL,
                    valign = Gtk.Align.CENTER,
                    margin_left = 20, margin_right = 20
                                            };
            var label = new Gtk.Label (location.name);
            label.set_markup ("<b>" + location.name + "</b>");
            label.halign = Gtk.Align.START;
            name_grid.add (label);

            if (location.size != null) {
                label = new Gtk.Label (format_size (location.size));
                label.halign = Gtk.Align.START;
                name_grid.add (label);
            } else {
                label = new Gtk.Label (location.mount_point != null ? location.mount_point : "not mounted");
                label.halign = Gtk.Align.START;
                label.xalign = 0;
                label.max_width_chars = 20;
                label.ellipsize = Pango.EllipsizeMode.END;
                if (location.mount_point != null)
                    label.set_tooltip_text (location.mount_point);
                name_grid.add (label);
            }

            first_row.add (name_grid);

            grid.add (first_row);

            var second_row =  new Gtk.Grid () {
                orientation = Gtk.Orientation.HORIZONTAL,
                column_spacing = 10
            };

            if (location.used != null) {
                var progress = new Gtk.ProgressBar ();
                progress.valign = Gtk.Align.CENTER;
                progress.set_fraction ((double) location.used / location.size);
                progress.show_text = true;
                second_row.add (progress);
            }

            var button_label = location.mount_point != null ? _("Scan") : _("Mount and scan");
            var button = new Gtk.Button.with_label (button_label);
            button.clicked.connect(() => { action (location); });
            button.valign = Gtk.Align.CENTER;
            button.halign = Gtk.Align.END;
            button.hexpand = true;
            second_row.add (button);

            grid.add (second_row);

            grid.show_all ();

            widget_size_group.add_widget (this);
        }
    }
}
