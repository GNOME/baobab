/* -*- indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* Baobab - disk usage analyzer
 *
 * Copyright (C) 2012  Paolo Borelli <pborelli@gnome.org>
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

    public class LocationWidget : Gtk.Grid {
        private static Gtk.SizeGroup name_size_group = null;
        private static Gtk.SizeGroup usage_size_group = null;
        private static Gtk.IconSize icon_size;

        public Location? location { get; private set; }

        void ensure_size_groups () {
            if (name_size_group == null) {
                name_size_group = new Gtk.SizeGroup (Gtk.SizeGroupMode.HORIZONTAL);
                usage_size_group = new Gtk.SizeGroup (Gtk.SizeGroupMode.HORIZONTAL);
                icon_size = Gtk.icon_size_register ("baobab", 64, 64);
            }
        }

        public LocationWidget (Location location_) {
            location = location_;

            orientation = Gtk.Orientation.HORIZONTAL;
            column_spacing = 12;
            margin = 6;

            ensure_size_groups ();

            var image = new Gtk.Image.from_gicon (location.icon, icon_size);
            image.set_pixel_size (64);
            attach (image, 0, 0, 1, 2);

            var escaped = GLib.Markup.escape_text (location.name, -1);
            var label = new Gtk.Label ("<b>%s</b>".printf (escaped));
            name_size_group.add_widget (label);
            label.use_markup = true;
            label.hexpand = true;
            label.halign = Gtk.Align.START;
            label.valign = Gtk.Align.END;
            label.xalign = 0;
            attach (label, 1, 0, 1, 1);

            escaped = location.file != null ? GLib.Markup.escape_text (location.file.get_parse_name (), -1) : "";
            label = new Gtk.Label ("<small>%s</small>".printf (escaped));
            name_size_group.add_widget (label);
            label.use_markup = true;
            label.hexpand = true;
            label.halign = Gtk.Align.START;
            label.valign = Gtk.Align.START;
            label.xalign = 0;
            label.get_style_context ().add_class ("dim-label");
            attach (label, 1, 1, 1, 1);

            if (location.used != null && location.size != null) {
                label = new Gtk.Label ("<small>%s / %s</small>".printf (format_size (location.used), format_size (location.size)));
                usage_size_group.add_widget (label);
                label.use_markup = true;
                label.halign = Gtk.Align.END;
                label.valign = Gtk.Align.END;
                attach (label, 2, 0, 1, 1);

                var usagebar = new Gtk.LevelBar ();
                usage_size_group.add_widget (usagebar);
                usagebar.set_max_value (location.size);
                // Set critical color at 90% of the size
                usagebar.add_offset_value (Gtk.LEVEL_BAR_OFFSET_LOW, 0.9 * location.size);
                usagebar.set_value (location.used);
                usagebar.hexpand = true;
                usagebar.halign = Gtk.Align.FILL;
                usagebar.valign = Gtk.Align.START;
                attach (usagebar, 2, 1, 1, 1);
            }

            attach (new Gtk.Arrow (Gtk.ArrowType.RIGHT, Gtk.ShadowType.NONE), 3, 0, 1, 2);

            show_all ();
        }
    }
}
