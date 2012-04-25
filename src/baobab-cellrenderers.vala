/* -*- indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

    public class CellRendererPercent : Gtk.CellRendererText {
        public double percent {
            set {
                text = "%.1f %%".printf (value);
            }
        }
    }

    public class CellRendererSize : Gtk.CellRendererText {
        public new uint64 size {
            set {
                if (!show_allocated_size) {
                    text = format_size (value);
                }
            }
        }

        public uint64 alloc_size {
            set {
                if (show_allocated_size) {
                    text = format_size (value);
                }
            }
        }

        public bool show_allocated_size { private get; set; }
    }

    public class CellRendererItems : Gtk.CellRendererText {
        public int items {
            set {
                text = value >= 0 ? ngettext ("%d item", "%d items", value).printf (value) : "";
            }
        }
    }

    public class CellRendererProgress : Gtk.CellRendererProgress {
        public override void render (Cairo.Context cr, Gtk.Widget widget, Gdk.Rectangle background_area, Gdk.Rectangle cell_area, Gtk.CellRendererState flags) {
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
            cr.rectangle (x_bar, y, perc_w, h);
            if (percent <= 33) {
                cr.set_source_rgb (0x73 / 255.0, 0xd2 / 255.0, 0x16 / 255.0);
            } else if (percent <= 66) {
                cr.set_source_rgb (0xed / 255.0, 0xd4 / 255.0, 0x00 / 255.0);
            } else {
                cr.set_source_rgb (0xcc / 255.0, 0x00 / 255.0, 0x00 / 255.0);
            }
            cr.fill ();

            context.restore ();
        }
    }
}
