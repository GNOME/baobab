/* -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* Baobab - disk usage analyzer
 *
 * Copyright (C) 2012  Ryan Lortie <desrt@desrt.ca>
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

    public class CellRendererName : Gtk.CellRendererText {
        public Scanner.State state { set; get; }

        public string name {
            set {
                var escaped = Markup.escape_text (value);
                switch (state) {
                case Scanner.State.ERROR:
                    markup = "<b>%s</b>".printf (escaped);
                    break;
                case Scanner.State.CHILD_ERROR:
                    markup = "<b>%s</b>".printf (escaped);
                    break;
                default:
                    markup = escaped;
                    break;
                }
            }
        }

        protected override void render (Cairo.Context cr, Gtk.Widget widget, Gdk.Rectangle background_area, Gdk.Rectangle cell_area, Gtk.CellRendererState flags) {
            var context = widget.get_style_context ();

            context.save ();

            switch (state) {
            case Scanner.State.ERROR:
                context.add_class ("baobab-cell-error");
                break;
            case Scanner.State.CHILD_ERROR:
                context.add_class ("baobab-cell-warning");
                break;
            }

            base.render (cr, widget, background_area, cell_area, flags);

            context.restore ();
        }

    }

    public class CellRendererSize : Gtk.CellRendererText {
        public Scanner.State state { set; get; }

        public new uint64 size {
            set {
                if (!show_allocated_size) {
                    text = (state != Scanner.State.ERROR ? format_size (value) : "");
                }
            }
        }

        public uint64 alloc_size {
            set {
                if (show_allocated_size) {
                    text = (state != Scanner.State.ERROR ? format_size (value) : "");
                }
            }
        }

        public bool show_allocated_size { private get; set; }
    }

    public class CellRendererItems : Gtk.CellRendererText {
        public Scanner.State state { set; get; }

        public int items {
            set {
                text = (value >= 0 && state != Scanner.State.ERROR) ? ngettext ("%d item", "%d items", value).printf (value) : "";
            }
        }
    }

    public class CellRendererProgress : Gtk.CellRendererProgress {
        public Scanner.State state { set; get; }

        public override void render (Cairo.Context cr, Gtk.Widget widget, Gdk.Rectangle background_area, Gdk.Rectangle cell_area, Gtk.CellRendererState flags) {
            if (state == Scanner.State.ERROR) {
                return;
            }

            int xpad;
            int ypad;
            get_padding (out xpad, out ypad);

            var x = cell_area.x + xpad;
            var y = cell_area.y + ypad;
            var w = cell_area.width - xpad * 2;
            var h = cell_area.height - ypad * 2;

            var context = widget.get_style_context ();

            context.save ();
            context.add_class ("baobab-level-cell");

            context.render_background (cr, x, y, w, h);
            context.render_frame (cr, x, y, w, h);

            var border = context.get_border (Gtk.StateFlags.NORMAL);
            x += border.left;
            y += border.top;
            w -= border.left + border.right;
            h -= border.top + border.bottom;

            var percent = value;
            var perc_w = (w * percent) / 100;
            var x_bar = x;
            if (widget.get_direction () == Gtk.TextDirection.RTL) {
                x_bar += w - perc_w;
            }

            context.add_class ("fill-block");

            if (percent <= 33) {
                context.add_class ("level-low");
            } else if (percent > 66) {
                context.add_class ("level-high");
            }

            context.render_background (cr, x_bar, y, perc_w, h);

            context.restore ();
        }
    }
}
