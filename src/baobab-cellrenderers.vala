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
				text = ngettext ("%d item", "%d items", value).printf (value);
			}
		}
	}
}
