/* -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

    [GtkTemplate (ui = "/org/gnome/baobab/ui/baobab-location-widget.ui")]
    public class LocationWidget : Gtk.Grid {
        private static Gtk.SizeGroup name_size_group = null;
        private static Gtk.SizeGroup usage_size_group = null;

        [GtkChild]
        private Gtk.Image image;
        [GtkChild]
        private Gtk.Label name_label;
        [GtkChild]
        private Gtk.Label path_label;
        [GtkChild]
        private Gtk.Label usage_label;
        [GtkChild]
        private Gtk.LevelBar usage_bar;

        public Location? location { get; private set; }

        void ensure_size_groups () {
            if (name_size_group == null) {
                name_size_group = new Gtk.SizeGroup (Gtk.SizeGroupMode.HORIZONTAL);
                usage_size_group = new Gtk.SizeGroup (Gtk.SizeGroupMode.HORIZONTAL);
            }
        }

        public LocationWidget (Location l) {
            location = l;

            ensure_size_groups ();

            image.gicon = location.icon;
            image.set_pixel_size (64);

            var escaped = GLib.Markup.escape_text (location.name, -1);
            name_label.label = "<b>%s</b>".printf (escaped);
            name_size_group.add_widget (name_label);

            escaped = location.file != null ? GLib.Markup.escape_text (location.file.get_parse_name (), -1) : "";
            path_label.label = "<small>%s</small>".printf (escaped);
            name_size_group.add_widget (path_label);

            if (location.is_volume && location.used != null && location.size != null) {
                usage_label.label = "<small>%s / %s</small>".printf (format_size (location.used), format_size (location.size));
                usage_size_group.add_widget (usage_label);
                usage_label.show ();

                usage_size_group.add_widget (usage_bar);
                usage_bar.set_max_value (location.size);

                // Set critical color at 90% of the size
                usage_bar.add_offset_value (Gtk.LEVEL_BAR_OFFSET_LOW, 0.9 * location.size);
                usage_bar.set_value (location.used);
                usage_bar.show ();
            }
        }
    }
}
