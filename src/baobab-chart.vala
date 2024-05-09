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
        public uint depth;
        public double rel_start;
        public double rel_size;
        public Scanner.Results results;
        public bool visible;
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

        Gtk.PopoverMenu context_menu = null;

        Gtk.EventControllerScroll scroll_controller;
        Gtk.EventControllerMotion motion_controller;
        Gtk.GestureClick primary_click_gesture;
        Gtk.GestureClick secondary_click_gesture;
        Gtk.GestureClick middle_click_gesture;

        Gdk.RGBA chart_colors[NUM_COLORS];

        List<ChartItem> items;

        Location location_;
        public Location location {
            set {
                location_ = value;
                model = location_.scanner.root.create_tree_model ();
                location_.scanner.bind_property ("max-depth", this, "max-depth", BindingFlags.SYNC_CREATE);
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

        Gtk.TreeListModel model_;
        protected Gtk.TreeListModel model {
            set {
                if (model_ == value) {
                    return;
                }

                if (model_ != null) {
                    disconnect_model_signals (model_);
                }

                model_ = value;
                model_changed = true;

                tree_root = null;

                connect_model_signals (model_);

                queue_draw ();
            }
            get {
                return model_;
            }
        }

        Scanner.Results? root_;
        public Scanner.Results? tree_root {
            set {
                if (model == null) {
                    return;
                }

                if (root_ == value) {
                    return;
                }
                root_ = value;

                highlighted_item = null;

                queue_draw ();
            }
            owned get {
                if (root_ != null) {
                    return root_;
                }

                // FIXME This feels dirty, I probably should find a cleaner way to get the model's root
                if (location != null && location_.scanner != null) {
                    return location_.scanner.root;
                }

                return null;
            }
        }

        ChartItem? highlighted_item_ = null;
        public ChartItem? highlighted_item {
            set {
                if (highlighted_item_ == value) {
                    return;
                }

                if (highlighted_item_ != null) {
                    queue_draw ();
                }
                if (value != null) {
                    queue_draw ();
                }

                highlighted_item_ = value;
            }
            get {
                return highlighted_item_;
            }
        }

        public signal void item_activated (Scanner.Results item);

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
            scroll_controller = new Gtk.EventControllerScroll (Gtk.EventControllerScrollFlags.BOTH_AXES);
            scroll_controller.scroll.connect (scroll_cb);
            add_controller (scroll_controller);

            motion_controller = new Gtk.EventControllerMotion ();
            motion_controller.motion.connect (motion_cb);
            motion_controller.enter.connect (enter_cb);
            add_controller (motion_controller);

            primary_click_gesture = new Gtk.GestureClick ();
            primary_click_gesture.button = Gdk.BUTTON_PRIMARY;
            primary_click_gesture.pressed.connect ((_, x, y) => {
                if (highlight_item_at_point (x, y)) {
                    if (tree_root == highlighted_item.results) {
                        move_up_root ();
                    } else {
                        item_activated (highlighted_item.results);
                    }
                }
            });
            add_controller (primary_click_gesture);

            secondary_click_gesture = new Gtk.GestureClick ();
            secondary_click_gesture.button = Gdk.BUTTON_SECONDARY;
            secondary_click_gesture.pressed.connect ((_, x, y) => {
                show_popover_at ((int) x, (int) y);
            });
            add_controller (secondary_click_gesture);

            middle_click_gesture = new Gtk.GestureClick ();
            middle_click_gesture.button = Gdk.BUTTON_MIDDLE;
            middle_click_gesture.pressed.connect ((_, x, y) => {
                move_up_root ();
            });
            add_controller (middle_click_gesture);

            action_group = new SimpleActionGroup ();
            action_group.add_action_entries (action_entries, this);
            insert_action_group ("chart", action_group);

            build_context_menu ();

            chart_colors[0].parse("#e01b24");
            chart_colors[1].parse("#ff7800");
            chart_colors[2].parse("#f6d32d");
            chart_colors[3].parse("#33d17a");
            chart_colors[4].parse("#3584e4");
            chart_colors[5].parse("#9141ac");

            set_draw_func (draw_func);
        }

        public override void size_allocate (int width, int height, int baseline) {
            base.size_allocate (width, height, baseline);
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

        void motion_cb (double x, double y) {
            has_tooltip = highlight_item_at_point (x, y);
        }

        void enter_cb (double x, double y) { 
            if (!context_menu.visible) {
                has_tooltip = highlight_item_at_point (x, y);
            }
        }

        unowned List<ChartItem> add_item (uint depth, double rel_start, double rel_size, Scanner.Results results) {
            var item = create_new_chartitem ();
            item.depth = depth;
            item.rel_start = rel_start;
            item.rel_size = rel_size;
            item.visible = false;
            item.has_visible_children = false;
            item.results = results;
            item.parent = null;

            items.prepend (item);

            unowned List<ChartItem> ret = items;
            return ret;
        }

        void get_items (Scanner.Results? root_path) {
            items = null;

            if (root_path == null) {
                model_changed = false;
                return;
            }

            unowned List<ChartItem> node = add_item (0, 0, 100, root_path);

            do {
                ChartItem item = node.data;

                calculate_item_geometry (item);

                if (!item.visible) {
                    node = node.prev;
                    continue;
                }

                if ((!item.results.is_empty) && (item.depth < max_depth + 1)) {
                    double rel_start = 0;
                    CompareDataFunc<Scanner.Results> reverse_size_cmp = (a, b) => {
                        return a.size < b.size ? 1 :
                               a.size > b.size ? -1 :
                               0;
                    };
                    var sorter = new Gtk.CustomSorter (reverse_size_cmp);
                    var sorted = new Gtk.SortListModel (item.results.children_list_store, sorter);
                    for (var i = 0; i < sorted.n_items; i++) {
                        var child_iter = sorted.get_object (i) as Scanner.Results;
                        unowned List<ChartItem> child_node = add_item (item.depth + 1, rel_start, child_iter.percent, child_iter);
                        var child = child_node.data;
                        child.parent = node;
                        rel_start += child_iter.percent;
                    }
                }

                node = node.prev;
            } while (node != null);

            items.reverse ();

            model_changed = false;
        }

        void draw_chart (Cairo.Context cr) {
            cr.save ();

            foreach (ChartItem item in items) {
                if (item.visible && (item.depth <= max_depth)) {
                    bool highlighted = (item == highlighted_item);
                    draw_item (cr, item, highlighted);
                }
            }

            cr.restore ();

            post_draw (cr);
        }

        void update_draw () {
            if (!get_realized ()) {
                return;
            }

            queue_draw ();
        }

        void items_changed (uint position, uint removed, uint added) {
            model_changed = true;
            update_draw ();
        }

        void draw_func (Gtk.DrawingArea da, Cairo.Context cr, int width, int height) {
            if (model != null) {
                if (model_changed || items == null) {
                    get_items (tree_root);
                } else {
                    var current_path = items.data.results;
                    if (tree_root != current_path) {
                        get_items (tree_root);
                    }
                }

                draw_chart (cr);
            }
        }

        Gdk.RGBA interpolate_colors (Gdk.RGBA colora, Gdk.RGBA colorb, float percentage) {
            var color = Gdk.RGBA ();

            float diff;

            diff = colora.red - colorb.red;
            color.red = colora.red - diff * percentage;
            diff = colora.green - colorb.green;
            color.green = colora.green - diff * percentage;
            diff = colora.blue - colorb.blue;
            color.blue = colora.blue - diff * percentage;

            color.alpha = (float) 1.0;

            return color;
        }

        protected Gdk.RGBA get_item_color (double rel_position, uint depth, bool highlighted) {
            var color = Gdk.RGBA ();

            float intensity = (float) (1 - (((depth - 1) * 0.3) / MAX_DEPTH));

            if (depth == 0) {
                color.parse("#d3d6d1");
            } else {
                Gdk.RGBA color_a = Gdk.RGBA ();
                Gdk.RGBA color_b = Gdk.RGBA ();

                int color_number = (int) (rel_position / (100.0/3));
                int next_color_number = (color_number + 1) % NUM_COLORS;

                color_a = chart_colors[color_number];
                color_b = chart_colors[next_color_number];

                color = interpolate_colors (color_a, color_b, (float) (rel_position - color_number * 100/3) / (100/3));

                color.red *= intensity;
                color.green *= intensity;
                color.blue *= intensity;
            }

            if (highlighted) {
                if (depth == 0) {
                    color.parse("#e0e2dd");
                } else {
                    float maximum = float.max (color.red, float.max (color.green, color.blue));
                    color.red /= maximum;
                    color.green /= maximum;
                    color.blue /= maximum;
                }
            }

            return color;
        }

        bool scroll_cb (double dx, double dy) {
            // Up or to the left
            if (dx > 0.0 || dy < 0.0) {
                zoom_out ();
                return true;
            // Down or to the right
            } else if (dx < 0.0 || dy > 0.0) {
                zoom_in ();
                return true;
            }

            return false;
        }

        public void open_file () {
            ((Window) get_root ()).open_item (highlighted_item.results);
        }

        public void copy_path () {
            ((Window) get_root ()).copy_path (highlighted_item.results);
        }

        public void trash_file () {
            ((Window) get_root ()).trash_file (highlighted_item.results);
        }

        protected bool can_move_up_root () {
            return tree_root.parent != null;
        }

        public void move_up_root () {
            if (tree_root.parent != null) {
                tree_root = tree_root.parent;
                item_activated (tree_root);
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
            context_menu = new Gtk.PopoverMenu.from_model (menu_model);
            context_menu.set_parent (this);
            context_menu.set_position (Gtk.PositionType.BOTTOM);
        }

        void show_popover_at (int x, int y) {
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

            Gdk.Rectangle rect = { x, y, 0, 0 };
            context_menu.set_pointing_to (rect);
            context_menu.popup ();
        }

        void connect_model_signals (Gtk.TreeListModel m) {
            m.items_changed.connect (items_changed);
        }

        void disconnect_model_signals (Gtk.TreeListModel m) {
            m.items_changed.disconnect (items_changed);
        }

        protected override bool query_tooltip (int x, int y, bool keyboard_tooltip, Gtk.Tooltip tooltip) {
            if (highlighted_item == null) {
                return false;
            }

            tooltip.set_tip_area (highlighted_item.rect);

            var size = format_size (highlighted_item.results.size);
            var markup = highlighted_item.results.display_name + "\n" + size;
            tooltip.set_markup (Markup.escape_text (markup));

            return true;
        }
    }
}
