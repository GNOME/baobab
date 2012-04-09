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

    public class LocationWidget : Gtk.Grid {
        private static Gtk.SizeGroup name_size_group = null;
        private static Gtk.SizeGroup mount_point_size_group = null;
        private static Gtk.SizeGroup size_size_group = null;
        private static Gtk.SizeGroup used_size_group = null;
        private static Gtk.SizeGroup button_size_group = null;

        public delegate void ActionOnClick (Location location);

        void ensure_size_groups () {
            if (name_size_group != null)
                return;

            name_size_group = new Gtk.SizeGroup (Gtk.SizeGroupMode.HORIZONTAL);
            mount_point_size_group = new Gtk.SizeGroup (Gtk.SizeGroupMode.HORIZONTAL);
            size_size_group = new Gtk.SizeGroup (Gtk.SizeGroupMode.HORIZONTAL);
            used_size_group = new Gtk.SizeGroup (Gtk.SizeGroupMode.HORIZONTAL);
            button_size_group = new Gtk.SizeGroup (Gtk.SizeGroupMode.HORIZONTAL);
        }

        public LocationWidget (Location location, ActionOnClick action) {
            orientation = Gtk.Orientation.HORIZONTAL;
            column_spacing = 10;
            margin = 6;

            ensure_size_groups ();

            var icon_theme = Gtk.IconTheme.get_default ();
            var icon_info = icon_theme.lookup_by_gicon (location.icon, 64, 0);

            try {
                var pixbuf = icon_info.load_icon ();
                var image = new Gtk.Image.from_pixbuf (pixbuf);
                add (image);
            } catch (Error e) {
                warning ("Failed to load icon %s: %s", location.icon.to_string(), e.message);
            }

            var label = new Gtk.Label (location.name);
            label.xalign = 0;
            name_size_group.add_widget (label);
            add (label);

            label = new Gtk.Label (location.mount_point != null ? location.mount_point : "");
            label.hexpand = true;
            label.halign = Gtk.Align.CENTER;
            label.xalign = 0;
            label.get_style_context ().add_class ("dim-label");
            mount_point_size_group.add_widget (label);
            add (label);

            label = new Gtk.Label (location.size != null ? format_size (location.size) : "");
            size_size_group.add_widget (label);
            add (label);

            if (location.used != null) {
                var progress = new Gtk.ProgressBar ();
                progress.valign = Gtk.Align.CENTER;
                progress.set_fraction ((double) location.used / location.size);
                used_size_group.add_widget (progress);
                add (progress);
            } else {
                label = new Gtk.Label (_("Usage unknown"));
                used_size_group.add_widget (label);
                add (label);
            }

            string button_label = location.mount_point != null ? _("Scan") : _("Mount and scan");
            var button = new Gtk.Button.with_label (button_label);
            button.valign = Gtk.Align.CENTER;
            button_size_group.add_widget (button);
            add (button);

            button.clicked.connect(() => { action (location); });

            show_all ();
        }
    }
}
