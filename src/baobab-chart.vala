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

    public abstract class Chart : Gtk.DrawingArea {

        protected const uint MAX_DEPTH = 5;
        protected const uint MIN_DEPTH = 1;

        // Keep in sync with colors defined in CSS
        const int NUM_COLORS = 6;

        bool model_changed;

        Gtk.Menu context_menu = null;

        List<ChartItem> items;

        Location location_;
        public Location location {
            set {
                location_ = value;
                model = location_.scanner;
                model.bind_property ("max-depth", this, "max-depth", BindingFlags.SYNC_CREATE);
            }

            get {
                return location_;
            }
        }

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
        protected Gtk.TreeModel model {
            set {
                if (model_ == value) {
                    return;
                }

                if (model_ != null) {
                    disconnect_model_signals (model_);
                }

                model_ = value;
                model_changed = true;

                root = null;

                connect_model_signals (model_);

                queue_draw ();
            }
            get {
                return model_;
            }
        }

        Gtk.TreeRowReference? root_;
        public Gtk.TreePath? root {
            set {
                if (model == null) {
                    return;
                }

                if (root_ != null) {
                    var current_root = root_.get_path ();
                    if (current_root != null && value != null && current_root.compare (value) == 0) {
                        return;
                    }
                } else if (value == null) {
                    return;
                }
                root_ = (value != null) ? new Gtk.TreeRowReference (model, value) : null;

                highlighted_item = null;

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

        public signal void item_activated (Gtk.TreeIter iter);

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
            { "open-file", open_file },
            { "copy-path", copy_path },
            { "trash-file", trash_file },
            { "move-up", move_up_root },
            { "zoom-in", zoom_in },
            { "zoom-out", zoom_out }
        };

        construct {
            add_events (Gdk.EventMask.EXPOSURE_MASK | Gdk.EventMask.ENTER_NOTIFY_MASK | Gdk.EventMask.LEAVE_NOTIFY_MASK | Gdk.EventMask.BUTTON_PRESS_MASK | Gdk.EventMask.POINTER_MOTION_MASK | Gdk.EventMask.SCROLL_MASK);

            action_group = new SimpleActionGroup ();
            action_group.add_action_entries (action_entries, this);
            insert_action_group ("chart", action_group);

            build_context_menu ();
        }

        public override void size_allocate (Gtk.Allocation allocation) {
            base.size_allocate (allocation);
            foreach (ChartItem item in items) {
                item.has_visible_children = false;
                item.visible = false;
                calculate_item_geometry (item);
            }
        }

        bool highlight_item_at_point (double x, double y) {
            for (unowned List<ChartItem> node = items.last (); node != null; node = node.prev) {
                var item = node.data;
                if (item.visible && is_point_over_item (item, x, y)) {
                    highlighted_item = item;
                    return true;
                }
            }

            highlighted_item = null;
            return false;
        }

        public override bool motion_notify_event (Gdk.EventMotion event) {
            has_tooltip = highlight_item_at_point (event.x, event.y);

            Gdk.Event.request_motions (event);

            return false;
        }

        public override bool leave_notify_event (Gdk.EventCrossing event) {
            if (!context_menu.visible) {
                highlighted_item = null;
            }

            return false;
        }

        unowned List<ChartItem> add_item (uint depth, double rel_start, double rel_size, Gtk.TreeIter iter) {
            string name;
            string display_name;
            uint64 size;
            model.get (iter,
                       Scanner.Columns.NAME, out name,
                       Scanner.Columns.DISPLAY_NAME, out display_name,
                       Scanner.Columns.SIZE, out size);

            var item = create_new_chartitem ();
            item.name = format_name (display_name, name);
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
            double percent;
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
            model.get (model_root_iter, Scanner.Columns.PERCENT, out percent);

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
                        model.get (child_iter, Scanner.Columns.PERCENT, out percent);
                        child_node = add_item (item.depth + 1, rel_start, percent, child_iter);
                        var child = child_node.data;
                        child.parent = node;
                        rel_start += percent;
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
            if (model != null) {
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
            var context = get_style_context ();

            var color = Gdk.RGBA ();

            double intensity = 1 - (((depth - 1) * 0.3) / MAX_DEPTH);

            if (depth == 0) {
                context.lookup_color ("level_color", out color);
            } else {
                Gdk.RGBA color_a, color_b;

                int color_number = (int) (rel_position / (100.0/3));
                int next_color_number = (color_number + 1) % NUM_COLORS;

                context.lookup_color ("color_" + color_number.to_string (), out color_a);
                context.lookup_color ("color_" + next_color_number.to_string (), out color_b);

                color = interpolate_colors (color_a, color_b, (rel_position - color_number * 100/3) / (100/3));

                color.red *= intensity;
                color.green *= intensity;
                color.blue *= intensity;
            }

            if (highlighted) {
                if (depth == 0) {
                    context.lookup_color ("level_color_hi", out color);
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
                if (event.triggers_context_menu ()) {
                    show_popup_menu (event);
                    return true;
                }

                switch (event.button) {
                case Gdk.BUTTON_PRIMARY:
                    if (highlight_item_at_point (event.x, event.y)) {
                        var path = model.get_path (highlighted_item.iter);
                        if (root.compare (path) == 0) {
                            move_up_root ();
                        } else {
                            item_activated (highlighted_item.iter);
                        }
                    }
                    break;
                case Gdk.BUTTON_MIDDLE:
                    move_up_root ();
                    break;
                }

                return true;
            }

            return false;
        }

        protected override bool scroll_event (Gdk.EventScroll event) {
            Gdk.EventMotion e = (Gdk.EventMotion) event;
            switch (event.direction) {
            case Gdk.ScrollDirection.LEFT:
            case Gdk.ScrollDirection.UP:
                zoom_out ();
                motion_notify_event (e);
                break;
            case Gdk.ScrollDirection.RIGHT:
            case Gdk.ScrollDirection.DOWN:
                zoom_in ();
                motion_notify_event (e);
                break;
            case Gdk.ScrollDirection.SMOOTH:
                break;
            }

            return false;
        }

        public void open_file () {
            ((Window) get_toplevel ()).open_item (highlighted_item.iter);
        }

        public void copy_path () {
            ((Window) get_toplevel ()).copy_path (highlighted_item.iter);
        }

        public void trash_file () {
            ((Window) get_toplevel ()).trash_file (highlighted_item.iter);
        }

        protected bool can_move_up_root () {
            Gtk.TreeIter iter, parent_iter;

            model.get_iter (out iter, root);
            return model.iter_parent (out parent_iter, iter);
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

        void build_context_menu () {
            var menu_model = Application.get_default ().get_menu_by_id ("chartmenu");
            context_menu = new Gtk.Menu.from_model (menu_model);
            context_menu.attach_to_widget (this, null);
        }

        void show_popup_menu (Gdk.EventButton? event) {
            var enable = highlighted_item != null;
            var action = action_group.lookup_action ("open-file") as SimpleAction;
            action.set_enabled (enable);
            action = action_group.lookup_action ("copy-path") as SimpleAction;
            action.set_enabled (enable);
            action = action_group.lookup_action ("trash-file") as SimpleAction;
            action.set_enabled (enable);

            action = action_group.lookup_action ("move-up") as SimpleAction;
            action.set_enabled (can_move_up_root ());

            action = action_group.lookup_action ("zoom-in") as SimpleAction;
            action.set_enabled (can_zoom_in ());
            action = action_group.lookup_action ("zoom-out") as SimpleAction;
            action.set_enabled (can_zoom_out ());

            context_menu.popup_at_pointer (event);
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
