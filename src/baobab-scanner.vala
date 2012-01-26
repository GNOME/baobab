/* Baobab - disk usage analyzer
 *
 * Copyright (C) 2012  Ryan Lortie <desrt@desrt.ca>
 * Copyright (C) 2012  Paolo Borelli <pborelli@gnome.org>
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
	abstract class Scanner : Gtk.TreeStore {
		public enum Columns {
			DISPLAY_NAME,
			PARSE_NAME,
			PERCENT,
			SIZE,
			ALLOC_SIZE,
			ELEMENTS,
			STATE,
			COLUMNS
		}

		public enum State {
			SCANNING,
			CANCELLED,
			NEED_PERCENT,
			DONE
		}

		protected struct HardLink {
			uint64 inode;
			uint32 device;

			public HardLink (FileInfo info) {
				this.inode = info.get_attribute_uint64 (FileAttribute.UNIX_INODE);
				this.device = info.get_attribute_uint32 (FileAttribute.UNIX_DEVICE);
			}
		}

		protected Cancellable cancellable;
		protected HashTable<File, unowned File> excluded_locations;
		protected HardLink[] hardlinks;

		protected static const string ATTRIBUTES =
			FileAttribute.STANDARD_NAME + "," +
			FileAttribute.STANDARD_DISPLAY_NAME + "," +
			FileAttribute.STANDARD_TYPE + "," +
			FileAttribute.STANDARD_SIZE +  "," +
			FileAttribute.STANDARD_ALLOCATED_SIZE +  "," +
			FileAttribute.UNIX_NLINK + "," +
			FileAttribute.UNIX_INODE + "," +
			FileAttribute.UNIX_DEVICE + "," +
			FileAttribute.ACCESS_CAN_READ;

		public File directory { get; protected set; }

		public abstract void scan (File directory);

		public int max_depth { get; protected set; }

		public virtual void stop () {
			cancellable.cancel ();
		}

		public Scanner () {
			cancellable = new Cancellable();
			set_column_types (new Type[] {
			                  typeof (string),  /* DIR_NAME */
			                  typeof (string),  /* PARSE_NAME */
			                  typeof (double),  /* PERCENT */
			                  typeof (uint64),  /* SIZE */
			                  typeof (uint64),  /* ALLOC_SIZE */
			                  typeof (int),     /* ELEMENTS */
			                  typeof (State)}); /* STATE */
			set_sort_column_id (Columns.SIZE, Gtk.SortType.DESCENDING);
			excluded_locations = Application.get_excluded_locations ();
		}
	}
}
