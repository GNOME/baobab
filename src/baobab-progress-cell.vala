/* -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* Baobab - disk usage analyzer
 *
 * Copyright © 2024 Adrien Plazas <aplazas@gnome.org>
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

    // This is a reimplementation of Systprof's progress cell widget:
    // https://gitlab.gnome.org/GNOME/sysprof/-/blob/master/src/sysprof/sysprof-progress-cell.c
    public class ProgressCell : Gtk.Widget {
        private double _fraction;
        [CCode(notify = false)]
        public double fraction {
            set {
                var clamped_value = value.clamp (0.0, 1.0);
                if (_fraction == clamped_value)
                    return;

                _fraction = clamped_value;

                update_text ();

                notify_property ("fraction");
                queue_allocate ();
            }
            get {
                return _fraction;
            }
        }

        private Adw.Bin trough;
        private Adw.Bin progress;
        private Gtk.Label label;
        private Gtk.Label alt_label;

        static construct {
            set_css_name ("progresscell");
            set_accessible_role (Gtk.AccessibleRole.PROGRESS_BAR);
        }

        construct {
            overflow = Gtk.Overflow.HIDDEN;

            label = GLib.Object.new (typeof (Gtk.Label),
                                     "xalign", (float) 1.0,
                                     "valign", Gtk.Align.CENTER,
                                     "width-chars", 7,
                                     null) as Gtk.Label;
            alt_label = GLib.Object.new (typeof (Gtk.Label),
                                         "xalign", (float) 1.0,
                                         "valign", Gtk.Align.CENTER,
                                         "width-chars", 7,
                                         null) as Gtk.Label;
            trough = GLib.Object.new (typeof (Adw.Bin),
                                      "css-name", "trough",
                                      null) as Adw.Bin;
            progress = GLib.Object.new (typeof (Adw.Bin),
                                        "css-name", "progress",
                                        "visible", false,
                                        null) as Adw.Bin;

            alt_label.add_css_class ("in-progress");

            trough.set_parent (this);
            label.set_parent (this);
            progress.set_parent (this);
            alt_label.set_parent (this);

            update_text ();
        }

        ~ProgressCell () {
            trough.unparent ();
            label.unparent ();
            progress.unparent ();
            alt_label.unparent ();
        }

        protected override void size_allocate (int width, int height, int baseline) {
            Gtk.Allocation all = { 0, 0, width, height };

            label.allocate_size (all, baseline);
            alt_label.allocate_size (all, baseline);
            trough.allocate_size (all, baseline);

            if (progress.visible) {
                all.width = int.max (1, (int) (width * fraction));
                progress.allocate_size (all, baseline);
            }
        }

        protected override void snapshot (Gtk.Snapshot snapshot) {
            snapshot_child (trough, snapshot);
            snapshot_child (label, snapshot);
            snapshot_child (progress, snapshot);

            Graphene.Rect rect = {};
            rect.init (0, 0, (float) (get_width () * fraction), get_height ());

            snapshot.push_clip (rect);
            snapshot_child (alt_label, snapshot);
            snapshot.pop ();
        }

        protected override void measure (Gtk.Orientation orientation,
                                         int for_size,
                                         out int minimum,
                                         out int natural,
                                         out int minimum_baseline,
                                         out int natural_baseline) {
            Gtk.Widget[] children = { trough, progress, label, alt_label };

            minimum = 0;
            natural = 0;
            minimum_baseline = -1;
            natural_baseline = -1;

            foreach (var child in children) {
                int child_min = 0;
                int child_nat = 0;

                child.measure (orientation, for_size, out child_min, out child_nat, null, null);

                if (child_min > minimum)
                    minimum = child_min;

                if (child_nat > natural)
                    natural = child_nat;
            }
        }

        private void update_text () {
                var text = "%6.2lf%%".printf (fraction * 100.0);
                label.set_text (text);
                alt_label.set_text (text);

                progress.visible = _fraction > 0.0;

                update_property (Gtk.AccessibleProperty.VALUE_MAX, 1.0,
                                 Gtk.AccessibleProperty.VALUE_MIN, 0.0,
                                 Gtk.AccessibleProperty.VALUE_NOW, fraction,
                                 Gtk.AccessibleProperty.VALUE_TEXT, text);
        }
    }
}
