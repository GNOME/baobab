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
	class Scanner : Gtk.TreeStore {
		public enum Columns {
			DISPLAY_NAME,
			PARSE_NAME,
			PERCENT,
			SIZE,
			ALLOC_SIZE,
			ELEMENTS,
			COLUMNS
		}

		struct HardLink {
			uint64 inode;
			uint32 device;

			public HardLink (FileInfo info) {
				this.inode = info.get_attribute_uint64 (FILE_ATTRIBUTE_UNIX_INODE);
				this.device = info.get_attribute_uint32 (FILE_ATTRIBUTE_UNIX_DEVICE);
			}
		}

		Cancellable? cancellable;
		HardLink[] hardlinks;

		static const string ATTRIBUTES =
			FILE_ATTRIBUTE_STANDARD_NAME + "," +
			FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME + "," +
			FILE_ATTRIBUTE_STANDARD_TYPE + "," +
			FILE_ATTRIBUTE_STANDARD_SIZE +  "," +
			FILE_ATTRIBUTE_UNIX_BLOCKS + "," +
			FILE_ATTRIBUTE_UNIX_NLINK + "," +
			FILE_ATTRIBUTE_UNIX_INODE + "," +
			FILE_ATTRIBUTE_UNIX_DEVICE + "," +
			FILE_ATTRIBUTE_ACCESS_CAN_READ;

		void add_directory (File directory, FileInfo info, Gtk.TreeIter? parent_iter = null) {
			Gtk.TreeIter iter;
			uint64 alloc_size;
			uint64 size;
			int elements;

			elements = 0;

			if (Application.is_excluded_location (directory)) {
				return;
			}

			var display_name = info.get_display_name ();
			var parse_name = directory.get_parse_name ();

			if (info.has_attribute (FILE_ATTRIBUTE_STANDARD_SIZE)) {
				size = info.get_size ();
			} else {
				size = 0;
			}

			if (info.has_attribute (FILE_ATTRIBUTE_UNIX_BLOCKS)) {
				alloc_size = 512 * info.get_attribute_uint64 (FILE_ATTRIBUTE_UNIX_BLOCKS);
			} else {
				alloc_size = 0;
			}

			append (out iter, parent_iter);
			set (iter, 0, display_name);
			set (iter, 1, parse_name);
			set (iter, 2, 0.0);
			set (iter, 3, size);
			set (iter, 4, alloc_size);
			set (iter, 5, elements);

			try {
				var children = directory.enumerate_children (ATTRIBUTES, FileQueryInfoFlags.NOFOLLOW_SYMLINKS, null);
				FileInfo? child_info;
				while ((child_info = children.next_file (cancellable)) != null) {
					if (cancellable.is_cancelled ()) {
						return;
					}

					switch (child_info.get_file_type ()) {
						case FileType.DIRECTORY:
							var child = directory.get_child (child_info.get_name ());
							add_directory (child, child_info, iter);
							elements++;
							break;

						case FileType.REGULAR:
							if (child_info.has_attribute (FILE_ATTRIBUTE_UNIX_NLINK)) {
								if (child_info.get_attribute_uint32 (FILE_ATTRIBUTE_UNIX_NLINK) > 1) {
									var hl = HardLink (child_info);

									/* check if we've already encountered this file */
									if (hl in hardlinks) {
										continue;
									}

									hardlinks += hl;
								}
							}

							if (child_info.has_attribute (FILE_ATTRIBUTE_UNIX_BLOCKS)) {
								alloc_size += 512 * child_info.get_attribute_uint64 (FILE_ATTRIBUTE_UNIX_BLOCKS);
							}
							size += child_info.get_size ();
							elements++;
							break;

						default:
							/* ignore other types (symlinks, sockets, devices, etc) */
							break;
					}
				}
			} catch (IOError.PERMISSION_DENIED e) {
			} catch (Error e) {
				warning ("couldn't iterate %s: %s", parse_name, e.message);
			}

		}

		void scan (File directory) {
			try {var info = directory.query_info (ATTRIBUTES, 0, cancellable);
			add_directory (directory, info);} catch { }
		}

		public Scanner (File directory) {
			set_column_types (new Type[] {
			                  typeof (string), /* DIR_NAME */
			                  typeof (string), /* PARSE_NAME */
			                  typeof (double), /* PERCENT */
			                  typeof (uint64), /* SIZE */
			                  typeof (uint64), /* ALLOC_SIZE */
			                  typeof (int)});  /* ELEMENTS */
			scan (directory);
		}
	}
}
