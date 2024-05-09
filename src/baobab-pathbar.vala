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

        public PathButton (string name, Icon? gicon, bool is_current_dir) {
            int min_chars = 7;
            label.label = name;
            tooltip_text = name;

            if (!is_current_dir) {
                icon.add_css_class ("dim-label");
                label.add_css_class ("dim-label");
            } else {
                // We want to avoid ellipsizing the current directory name, but
                // still need to set a limit.
                min_chars = 4 * min_chars;
                add_css_class ("current-dir");
            }

            // Labels can ellipsize until they become a single ellipsis character.
            // We don't want that, so we must set a minimum.
            //
            // However, for labels shorter than the minimum, setting this minimum
            // width would make them unnecessarily wide. In that case, just make it
            // not ellipsize instead.
            //
            // Due to variable width fonts, labels can be shorter than the space
            // that would be reserved by setting a minimum amount of characters.
            // Compensate for this with a tolerance of +50% characters.
            if (name.length > min_chars * 1.5) {
                label.width_chars = min_chars;
                label.ellipsize = Pango.EllipsizeMode.MIDDLE;
            }

            icon.hide ();
            if (gicon != null) {
                icon.gicon = gicon;
                icon.show ();
            }
        }
    }

    [GtkTemplate (ui = "/org/gnome/baobab/ui/baobab-pathbar.ui")]
    public class Pathbar : Gtk.Box {
        [GtkChild]
        private unowned Gtk.Box button_box;

        public signal void item_activated (Scanner.Results path);

        [GtkCallback]
        private void on_adjustment_changed (Gtk.Adjustment adjusment) {
            const uint DURATION = 800;
            var target = new Adw.PropertyAnimationTarget (adjusment, "value");
            var animation = new Adw.TimedAnimation (this, adjusment.value, adjusment.upper, DURATION, target);
            animation.easing = Adw.Easing.EASE_OUT_CUBIC;
            animation.play ();
        }

        [GtkCallback]
        private void on_page_size_changed (Object o, ParamSpec spec) {
            var adjustment = (Gtk.Adjustment) o;
            // When window is resized, immediately set new value, otherwise we would get
            // an underflow gradient for an moment.
            adjustment.value = adjustment.upper;
        }

        Location location_;
        public Location location {
            set {
                location_ = value;
                path = location.scanner.root;
            }

            get {
                return location_;
            }
        }

        public new Scanner.Results path {
            set {
                if (location == null || location.scanner == null) {
                    return;
                }

                clear ();

                Scanner.Results path_tmp = value;

                List<PathButton> buttons = null;

                // The path will be set to null while scanning the location, but
                // we still want to display a button for the location's root.
                bool is_current_dir = true;
                do {
                    buttons.prepend (make_button (path_tmp, is_current_dir));
                    path_tmp = path_tmp.parent;
                    is_current_dir = false;
                } while (path_tmp != null);

                bool first_directory = true;
                foreach (var button in buttons) {
                    Gtk.Box box = new Gtk.Box(Gtk.Orientation.HORIZONTAL, 0);
                    if (!first_directory) {
                        Gtk.Label label = new Gtk.Label (GLib.Path.DIR_SEPARATOR_S);
                        label.add_css_class ("dim-label");
                        box.append (label);
                    }
                    box.append (button);
                    button_box.append (box);
                    first_directory = false;
                }
            }
        }

        void clear () {
            for (Gtk.Widget? child = button_box.get_first_child (); child != null; child = button_box.get_first_child ()) {
                button_box.remove (child);
            }
        }

        PathButton make_button (Scanner.Results? path, bool is_current_dir) {
            string label;
            Icon? gicon = null;

            if (path == null || path.get_depth () == 1) {
                label = location.name;
                gicon = location.symbolic_icon;
            } else {
                label = path.display_name;
            }

            var button = new PathButton (label, gicon, is_current_dir);
            if (path != null && !is_current_dir) {
                button.clicked.connect (() => {
                    item_activated (path);
                });
            }

            return button;
        }
    }
}
