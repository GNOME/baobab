/* -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* Baobab - disk usage analyzer
 *
 * Copyright (C) 2006, 2007, 2008  Igalia
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

    public abstract class ChartItem {
        public string name;
        public string size;
        public uint depth;
        public double rel_start;
        public double rel_size;
        public Gtk.TreeIter iter;
        public bool visible;
        public bool has_any_child;
        public bool has_visible_children;
        public Gdk.Rectangle rect;

        public unowned List<ChartItem> parent;
    }

    public abstract class Chart : Gtk.Widget {

        protected const uint MAX_DEPTH = 8;
        protected const uint MIN_DEPTH = 1;

        const Gdk.RGBA TANGO_COLORS[] = {{0.94, 0.16, 0.16, 1.0}, /* tango: ef2929 */
                                         {0.68, 0.49, 0.66, 1.0}, /* tango: ad7fa8 */
                                         {0.45, 0.62, 0.82, 1.0}, /* tango: 729fcf */
                                         {0.54, 0.89, 0.20, 1.0}, /* tango: 8ae234 */
                                         {0.91, 0.73, 0.43, 1.0}, /* tango: e9b96e */
                                         {0.99, 0.68, 0.25, 1.0}}; /* tango: fcaf3e */

        uint name_column;
        uint size_column;
        uint info_column;
        uint percentage_column;
        uint valid_column;

        bool model_changed;

        Gtk.Menu context_menu = null;

        List<ChartItem> items;

        uint max_depth_ = MAX_DEPTH;
        public uint max_depth {
            set {
                var m = value.clamp (MIN_DEPTH, MAX_DEPTH);
                if (max_depth_ == m) {
                    return;
                }
                max_depth_ = m;
                model_changed = true;
                queue_draw ();
            }
            get {
                return max_depth_;
            }
        }

        Gtk.TreeModel model_;
        public Gtk.TreeModel model {
            set {
                if (model_ == value) {
                    return;
                }

                if (model_ != null) {
                    disconnect_model_signals (model_);
                }

                model_ = value;
                root = null;

                connect_model_signals (model_);

                queue_draw ();
            }
            get {
                return model_;
            }
        }

        public void set_model_with_columns (Gtk.TreeModel m,
                                            uint          name_column_,
                                            uint          size_column_,
                                            uint          info_column_,
                                            uint          percentage_column_,
                                            uint          valid_column_,
                                            Gtk.TreePath? r) {
            model = m;
            if (r != null) {
                root = r;
            }

            name_column = name_column_;
            size_column = size_column_;
            info_column = info_column_;
            percentage_column = percentage_column_;
            valid_column = valid_column_;
        }

        Gtk.TreeRowReference? root_;
        public Gtk.TreePath? root {
            set {
                if (root_ != null) {
                    var current_root = root_.get_path ();
                    if (current_root != null && value != null && current_root.compare (value) == 0) {
                        return;
                    }
                } else if (value == null) {
                    return;
                }
                root_ = (value != null) ? new Gtk.TreeRowReference (model, value) : null;

                queue_draw ();
            }
            owned get {
                if (root_ != null) {
                    var path = root_.get_path ();
                    if (path != null) {
                        return path;
                    }
                    root_ = null;
                }
                return new Gtk.TreePath.first ();
            }
        }

        ChartItem? highlighted_item_ = null;
        public ChartItem? highlighted_item {
            set {
                if (highlighted_item_ == value) {
                    return;
                }

                if (highlighted_item_ != null) {
                    get_window ().invalidate_rect (highlighted_item_.rect, true);
                }
                if (value != null) {
                    get_window ().invalidate_rect (value.rect, true);
                }

                highlighted_item_ = value;
            }
            get {
                return highlighted_item_;
            }
        }

        public virtual signal void item_activated (Gtk.TreeIter iter) {
            root = model.get_path (iter);
        }

        protected virtual void post_draw  (Cairo.Context cr) {
        }

        protected abstract void draw_item (Cairo.Context cr, ChartItem item, bool highlighted);
        protected abstract void calculate_item_geometry (ChartItem item);

        protected abstract bool is_point_over_item (ChartItem item, double x, double y);

        protected abstract void get_item_rectangle (ChartItem item);

        protected abstract bool can_zoom_in ();
        protected abstract bool can_zoom_out ();

        protected abstract ChartItem create_new_chartitem ();

        SimpleActionGroup action_group;

        const ActionEntry[] action_entries = {
            { "move-up", move_up_root },
            { "zoom-in", zoom_in },
            { "zoom-out", zoom_out }
        };

        construct {
            action_group = new SimpleActionGroup ();
            action_group.add_action_entries (action_entries, this);
            insert_action_group ("chart", action_group);
        }

        public override void realize () {
            Gtk.Allocation allocation;
            get_allocation (out allocation);
            set_realized (true);

            Gdk.WindowAttr attributes = {};
            attributes.window_type = Gdk.WindowType.CHILD;
            attributes.x = allocation.x;
            attributes.y = allocation.y;
            attributes.width = allocation.width;
            attributes.height = allocation.height;
            attributes.wclass = Gdk.WindowWindowClass.INPUT_OUTPUT;
            //attributes.visual = gtk_widget_get_visual (widget);
            attributes.event_mask = this.get_events () | Gdk.EventMask.EXPOSURE_MASK | Gdk.EventMask.ENTER_NOTIFY_MASK | Gdk.EventMask.LEAVE_NOTIFY_MASK | Gdk.EventMask.BUTTON_PRESS_MASK | Gdk.EventMask.POINTER_MOTION_MASK | Gdk.EventMask.POINTER_MOTION_HINT_MASK | Gdk.EventMask.SCROLL_MASK;

            var window = new Gdk.Window (get_parent_window (), attributes, Gdk.WindowAttributesType.X | Gdk.WindowAttributesType.Y);

            set_window (window);
            window.set_user_data (this);

            get_style_context ().set_background (window);
        }

        public override void size_allocate (Gtk.Allocation allocation) {
            set_allocation (allocation);
            if (get_realized ()) {
                get_window ().move_resize (allocation.x,
                                           allocation.y,
                                           allocation.width,
                                           allocation.height);

                foreach (ChartItem item in items) {
                    item.has_visible_children = false;
                    item.visible = false;
                    calculate_item_geometry (item);
                }
            }
        }

        public override bool motion_notify_event (Gdk.EventMotion event) {
            bool found = false;

            for (unowned List<ChartItem> node = items.last (); node != null; node = node.prev) {
                var item = node.data;
                if (item.visible && is_point_over_item (item, event.x, event.y)) {
                    highlighted_item = item;
                    has_tooltip = true;
                    found = true;
                    break;
                }
            }

            if (!found) {
                highlighted_item = null;
                has_tooltip = false;
            }

            Gdk.Event.request_motions (event);

            return false;
        }

        unowned List<ChartItem> add_item (uint depth, double rel_start, double rel_size, Gtk.TreeIter iter) {
            string name;
            uint64 size;
            model.get (iter, name_column, out name, size_column, out size, -1);

            var item = create_new_chartitem ();
            item.name = name;
            item.size = format_size (size);
            item.depth = depth;
            item.rel_start = rel_start;
            item.rel_size = rel_size;
            item.has_any_child = false;
            item.visible = false;
            item.has_visible_children = false;
            item.iter = iter;
            item.parent = null;

            items.prepend (item);

            unowned List<ChartItem> ret = items;
            return ret;
        }

        void get_items (Gtk.TreePath root_path) {
            unowned List<ChartItem> node = null;
            Gtk.TreeIter initial_iter = {0};
            double size;
            Gtk.TreePath model_root_path;
            Gtk.TreeIter model_root_iter;
            Gtk.TreeIter child_iter = {0};
            unowned List<ChartItem> child_node;
            double rel_start;

            items = null;

            if (!model.get_iter (out initial_iter, root_path)) {
                model_changed = false;
                return;
            }

            model_root_path = new Gtk.TreePath.first ();
            model.get_iter (out model_root_iter, model_root_path);
            model.get (model_root_iter, percentage_column, out size, -1);

            node = add_item (0, 0, 100, initial_iter);

            do {
                ChartItem item = node.data;
                item.has_any_child = model.iter_children (out child_iter, item.iter);

                calculate_item_geometry (item);

                if (!item.visible) {
                    node = node.prev;
                    continue;
                }

                if ((item.has_any_child) && (item.depth < max_depth + 1)) {
                    rel_start = 0;
                    do {
                        model.get (child_iter, percentage_column, out size, -1);
                        child_node = add_item (item.depth + 1, rel_start, size, child_iter);
                        var child = child_node.data;
                        child.parent = node;
                        rel_start += size;
                    } while (model.iter_next (ref child_iter));
                }

                node = node.prev;
            } while (node != null);

            items.reverse ();

            model_changed = false;
        }

        void draw_chart (Cairo.Context cr) {
            cr.save ();

            foreach (ChartItem item in items) {
                Gdk.Rectangle clip;
                if (Gdk.cairo_get_clip_rectangle (cr, out clip) &&
                    item.visible &&
                    clip.intersect (item.rect, null) &&
                    (item.depth <= max_depth)) {
                    bool highlighted = (item == highlighted_item);
                    draw_item (cr, item, highlighted);
                }
            }

            cr.restore ();

            post_draw (cr);
        }

        void update_draw (Gtk.TreePath path) {
            if (!get_realized ()) {
                return;
            }

            var root_depth = root.get_depth ();
            var node_depth = path.get_depth ();

            if (((node_depth - root_depth) <= max_depth) &&
                (root.is_ancestor (path) ||
                 root.compare (path) == 0)) {
                queue_draw ();
            }
        }

        void row_changed (Gtk.TreeModel model,
                          Gtk.TreePath path,
                          Gtk.TreeIter iter) {
            model_changed = true;
            update_draw (path);
        }

        void row_inserted (Gtk.TreeModel model,
                           Gtk.TreePath path,
                           Gtk.TreeIter iter) {
            model_changed = true;
            update_draw (path);
        }

        void row_deleted (Gtk.TreeModel model,
                          Gtk.TreePath path) {
            model_changed = true;
            update_draw (path);
        }

        void row_has_child_toggled (Gtk.TreeModel model,
                                    Gtk.TreePath path,
                                    Gtk.TreeIter iter) {
            model_changed = true;
            update_draw (path);
        }

        void rows_reordered (Gtk.TreeModel model,
                             Gtk.TreePath path,
                             Gtk.TreeIter? iter,
                             void *new_order) {
            model_changed = true;
            update_draw (path);
        }

        public override bool draw (Cairo.Context cr) {
            if (name_column == percentage_column) {
                // Columns not set
                return false;
            }

            if (model != null) {
                cr.set_source_rgb (1, 1, 1);
                cr.fill_preserve ();

                if (model_changed || items == null) {
                    get_items (root);
                } else {
                    var current_path = model.get_path (items.data.iter);
                    if (root.compare (current_path) != 0) {
                        get_items (root);
                    }
                }

                draw_chart (cr);
            }

            return false;
        }

        Gdk.RGBA interpolate_colors (Gdk.RGBA colora, Gdk.RGBA colorb, double percentage) {
            var color = Gdk.RGBA ();

            double diff;

            diff = colora.red - colorb.red;
            color.red = colora.red - diff * percentage;
            diff = colora.green - colorb.green;
            color.green = colora.green - diff * percentage;
            diff = colora.blue - colorb.blue;
            color.blue = colora.blue - diff * percentage;

            color.alpha = 1.0;

            return color;
        }

        protected Gdk.RGBA get_item_color (double rel_position, uint depth, bool highlighted) {
            const Gdk.RGBA level_color = {0.83, 0.84, 0.82, 1.0};
            const Gdk.RGBA level_color_hi = {0.88, 0.89, 0.87, 1.0};


            var color = Gdk.RGBA ();

            double intensity = 1 - (((depth - 1) * 0.3) / MAX_DEPTH);

            if (depth == 0) {
                color = level_color;
            } else {
                int color_number = (int) (rel_position / (100.0/3));
                int next_color_number = (color_number + 1) % 6;

                color = interpolate_colors (TANGO_COLORS[color_number],
                                            TANGO_COLORS[next_color_number],
                                            (rel_position - color_number * 100/3) / (100/3));
                color.red *= intensity;
                color.green *= intensity;
                color.blue *= intensity;
            }

            if (highlighted) {
                if (depth == 0) {
                    color = level_color_hi;
                } else {
                    double maximum = double.max (color.red, double.max (color.green, color.blue));
                    color.red /= maximum;
                    color.green /= maximum;
                    color.blue /= maximum;
                }
            }

            return color;
        }

        protected override bool button_press_event (Gdk.EventButton event) {
            if (event.type == Gdk.EventType.BUTTON_PRESS) {
                if (((Gdk.Event) (&event)).triggers_context_menu ()) {
                    show_popup_menu (event);
                    return true;
                }

                switch (event.button) {
                case 1:
                    if (highlighted_item != null) {
                        var path = model.get_path (highlighted_item.iter);
                        if (root.compare (path) == 0) {
                            move_up_root ();
                        } else {
                            item_activated (highlighted_item.iter);
                        }
                    }
                    break;
                case 2:
                    move_up_root ();
                    break;
                }

                return true;
            }

            return false;
        }

        protected override bool scroll_event (Gdk.EventScroll event) {
            Gdk.EventMotion *e = (Gdk.EventMotion *) (&event);
            switch (event.direction) {
            case Gdk.ScrollDirection.LEFT:
            case Gdk.ScrollDirection.UP:
                zoom_out ();
                motion_notify_event (*e);
                break;
            case Gdk.ScrollDirection.RIGHT:
            case Gdk.ScrollDirection.DOWN:
                zoom_in ();
                motion_notify_event (*e);
                break;
            case Gdk.ScrollDirection.SMOOTH:
                break;
            }

            return false;
        }

        public void move_up_root () {
            Gtk.TreeIter iter, parent_iter;

            model.get_iter (out iter, root);
            if (model.iter_parent (out parent_iter, iter)) {
                root = model.get_path (parent_iter);
                item_activated (parent_iter);
            }
        }

        public void zoom_in () {
            if (can_zoom_in ()) {
                max_depth--;
            }
       }

        public void zoom_out () {
            if (can_zoom_out ()) {
                max_depth++;
            }
        }

        void ensure_context_menu () {
            if (context_menu != null) {
                return;
            }

            var builder = new Gtk.Builder ();
            try {
                builder.add_from_resource ("/org/gnome/baobab/ui/baobab-menu.ui");
            } catch (Error e) {
                error ("loading context menu from resources: %s", e.message);
            }

            var menu_model = builder.get_object ("chartmenu") as MenuModel;
            context_menu = new Gtk.Menu.from_model (menu_model);
            context_menu.attach_to_widget (this, null);
        }

        void show_popup_menu (Gdk.EventButton? event) {
            ensure_context_menu ();

            if (event != null) {
                context_menu.popup (null, null, null, event.button, event.time);
            } else {
                context_menu.popup (null, null, null, 0, Gtk.get_current_event_time ());
            }
        }

        void connect_model_signals (Gtk.TreeModel m) {
            m.row_changed.connect (row_changed);
            m.row_inserted.connect (row_inserted);
            m.row_has_child_toggled.connect (row_has_child_toggled);
            m.row_deleted.connect (row_deleted);
            m.rows_reordered.connect (rows_reordered);
        }

        void disconnect_model_signals (Gtk.TreeModel m) {
            m.row_changed.disconnect (row_changed);
            m.row_inserted.disconnect (row_inserted);
            m.row_has_child_toggled.disconnect (row_has_child_toggled);
            m.row_deleted.disconnect (row_deleted);
            m.rows_reordered.disconnect (rows_reordered);
        }

        protected override bool query_tooltip (int x, int y, bool keyboard_tooltip, Gtk.Tooltip tooltip) {
            if (highlighted_item == null ||
                highlighted_item.name == null ||
                highlighted_item.size == null) {
                return false;
            }

            tooltip.set_tip_area (highlighted_item.rect);

            var markup = highlighted_item.name + "\n" + highlighted_item.size;
            tooltip.set_markup (Markup.escape_text (markup));

            return true;
        }
    }
}