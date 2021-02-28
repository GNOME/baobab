/* Baobab - disk usage analyzer
 *
 * Copyright (C) 2020 Stefano Facchini <stefano.facchini@gmail.com>
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

    [GtkTemplate (ui = "/org/gnome/baobab/ui/baobab-pathbutton.ui")]
    public class PathButton : Gtk.Button {
        [GtkChild]
        new unowned Gtk.Label label;
        [GtkChild]
        unowned Gtk.Image icon;

        public PathButton (string name, Icon? gicon) {
            label.label = name;

            icon.hide ();
            if (gicon != null) {
                icon.gicon = gicon;
                icon.show ();
            }
        }
    }

    public class Pathbar : Gtk.Box {
        public signal void item_activated (Gtk.TreePath path);

        Location location_;
        public Location location {
            set {
                location_ = value;
                path = new Gtk.TreePath.first ();
            }

            get {
                return location_;
            }
        }

        public new Gtk.TreePath path {
            set {
                if (location == null || location.scanner == null) {
                    return;
                }

                clear ();

                Gtk.TreePath path_tmp = value;

                List<PathButton> buttons = null;

                while (path_tmp.get_depth () > 0) {
                    buttons.append (make_button (path_tmp));
                    path_tmp.up ();
                }

                buttons.reverse ();
                foreach (var button in buttons) {
                    add (button);
                }
            }
        }

        void clear () {
            this.foreach ((widget) => { widget.destroy (); });
        }

        PathButton make_button (Gtk.TreePath path) {
            string label;
            Icon? gicon = null;

            if (path.get_depth () == 1) {
                label = location.name;
                gicon = location.symbolic_icon;
            } else {
                Gtk.TreeIter iter;
                string name;
                string display_name;
                location.scanner.get_iter (out iter, path);
                location.scanner.get (iter,
                                      Scanner.Columns.NAME, out name,
                                      Scanner.Columns.DISPLAY_NAME, out display_name);
                label = display_name != null ? display_name : name;
            }

            var button = new PathButton (label, gicon);
            button.clicked.connect (() => {
                item_activated (path);
            });

            return button;
        }
    }
}
