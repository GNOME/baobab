/* -*- indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* Baobab - disk usage analyzer
 *
 * Copyright (C) 2012  Paolo Borelli <pborelli@gnome.org>
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

    public class LocationList : Egg.ListBox {
        LocationMonitor location_monitor;
        LocationWidget.LocationAction? location_action;

        construct {
            location_monitor = LocationMonitor.get ();
            location_monitor.changed.connect (() => { update (); });

            set_separator_funcs (update_separator);
        }

        void update_separator (ref Gtk.Widget? separator, Gtk.Widget widget, Gtk.Widget? before_widget) {
            if (before_widget != null && separator == null) {
                separator = new Gtk.Separator (Gtk.Orientation.HORIZONTAL);
            } else {
                separator = null;
            }
        }

        public void set_action (owned LocationWidget.LocationAction? action) {
            location_action = (owned)action;
        }

        public void update () {
            this.foreach ((widget) => { widget.destroy (); });

            foreach (var location in location_monitor.get_locations ()) {
                add (new LocationWidget (location, location_action));
            }

            show_all ();
        }
    }
}
