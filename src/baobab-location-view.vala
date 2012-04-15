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

    public class LocationView : Gtk.Grid {
        private LocationFlowContainer volume_container;
        private LocationFlowContainer folder_container;

        public LocationView () {
            orientation = Gtk.Orientation.VERTICAL;
            hexpand = true;
            vexpand = true;
            margin = 10;

            Gtk.Label label;

            label = new Gtk.Label (_("Volumes"));
            label.set_markup ("<b>" + _("Volumes") + "</b>");
            label.halign = Gtk.Align.START;
            label.margin_bottom = 10;
            add (label);
            label.show ();

            volume_container = new LocationFlowContainer ();
            add (volume_container);
            volume_container.show ();

            label = new Gtk.Label (_("Folders"));
            label.set_markup ("<b>" + _("Folders") + "</b>");
            label.halign = Gtk.Align.START;
            label.margin_top = 24;
            label.margin_bottom = 10;
            add (label);
            label.show ();

            folder_container = new LocationFlowContainer ();
            add (folder_container);
            folder_container.show ();
        }

        public void add_to_volumes (LocationWidget location_widget) {
            volume_container.add (location_widget);
        }

        public void add_to_folders (LocationWidget location_widget) {
            folder_container.add (location_widget);
        }

        public void clear () {
            volume_container.clear ();
            folder_container.clear ();
        }
    }

    public class LocationFlowContainer : Gtk.Container {
        private int _column_spacing = 20;
        public int column_spacing {
            get {
                return _column_spacing;
            }

            set {
                _column_spacing = value;
                queue_resize ();
            }
        }

        private int _row_spacing = 20;
        public int row_spacing {
            get {
                return _row_spacing;
            }
            set {
                _row_spacing = value;
                queue_resize ();
            }
        }

        int columns;
        int rows;

        List<Gtk.Widget> children = null;

        public LocationFlowContainer () {
            hexpand = true;

            set_has_window (false);
        }

        public void clear () {
            this.foreach ((widget) => { widget.destroy (); });
        }

        protected override Gtk.SizeRequestMode get_request_mode () {
            return Gtk.SizeRequestMode.HEIGHT_FOR_WIDTH;
        }

        protected override void get_preferred_width (out int minimum, out int natural) {
            int width = 0;

            if (children.length () > 0) {
                children.data.get_preferred_width (null, out width);
            }

            minimum = natural = width;
        }

        protected override void get_preferred_height_for_width (int width, out int minimum, out int natural) {
            int height = 0;

            var n_children = children.length ();
            if (n_children > 0) {
                int child_width, child_height;
                children.data.get_preferred_width (null, out child_width);
                children.data.get_preferred_height (null, out child_height);

                int n_columns = (width + column_spacing) / (child_width + column_spacing);
                int n_rows = (int) Math.ceil((double) n_children / n_columns);
                height = (child_height + row_spacing) * n_rows - row_spacing;
            }

            minimum = natural = height;
        }

        protected override void size_allocate (Gtk.Allocation alloc) {
            set_allocation (alloc);

            if (children.length () == 0) {
                return;
            }

            int child_width, child_height;
            children.data.get_preferred_width (null, out child_width);
            children.data.get_preferred_height (null, out child_height);

            columns = (alloc.width + column_spacing) / (child_width + column_spacing);
            rows = 0;

            int col = 0;
            foreach (var child in children) {
                var child_alloc = Gtk.Allocation ();
                child_alloc.x = alloc.x + col * (child_width + column_spacing);
                child_alloc.y = alloc.y + rows * (child_height + row_spacing);
                child_alloc.width = child_width;
                child_alloc.height = child_height;

                child.size_allocate (child_alloc);

                if (++col >= columns) {
                    col = 0;
                    rows++;
                }
            }
        }

        protected override void forall_internal (bool include_internals, Gtk.Callback callback) {
            unowned List<Gtk.Widget> list = children;
            while (list != null) {
                Gtk.Widget child = list.data;
                list = list.next;

                callback (child);
            }
        }

        protected override void remove (Gtk.Widget widget) {
            foreach (var child in children) {
                if (child == widget) {
                    widget.unparent ();
                    children.remove (widget);

                    queue_resize ();

                    break;
                }
            }
        }

        protected override void add (Gtk.Widget widget) {
            widget.set_parent (this);
            widget.show ();
            children.append (widget);
        }
    }
}
