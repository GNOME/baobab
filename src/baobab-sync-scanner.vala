/* -*- indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* Baobab - disk usage analyzer
 *
 * Copyright (C) 2012  Ryan Lortie <desrt@desrt.ca>
 * Copyright (C) 2012  Paolo Borelli <pborelli@gnome.org>
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

    class SyncScanner : Scanner {
        struct Results {
            uint64 size;
            uint64 alloc_size;
            uint64 elements;
            int max_depth;
        }

        Results add_directory (File directory, FileInfo info, Gtk.TreeIter? parent_iter = null) {
            var results = Results ();
            Gtk.TreeIter iter;

            if (directory in excluded_locations) {
                return results;
            }

            var display_name = info.get_display_name ();
            var parse_name = directory.get_parse_name ();

            append (out iter, parent_iter);
            set (iter,
                 Columns.DISPLAY_NAME, display_name,
                 Columns.PARSE_NAME,   parse_name);

            results.size = info.get_size ();
            if (info.has_attribute (FileAttribute.STANDARD_ALLOCATED_SIZE)) {
                results.alloc_size = info.get_attribute_uint64 (FileAttribute.STANDARD_ALLOCATED_SIZE);
            }
            results.elements = 1;

            try {
                var children = directory.enumerate_children (ATTRIBUTES, FileQueryInfoFlags.NOFOLLOW_SYMLINKS, null);
                FileInfo? child_info;
                while ((child_info = children.next_file (cancellable)) != null) {
                    if (cancellable.is_cancelled ()) {
                        break;
                    }

                    switch (child_info.get_file_type ()) {
                        case FileType.DIRECTORY:
                            var child = directory.get_child (child_info.get_name ());
                            var child_results = add_directory (child, child_info, iter);

                            results.size += child_results.size;
                            results.alloc_size += child_results.alloc_size;
                            results.elements += child_results.elements;
                            results.max_depth = int.max (results.max_depth, child_results.max_depth + 1);
                            break;

                        case FileType.REGULAR:
                            if (child_info.has_attribute (FileAttribute.UNIX_NLINK)) {
                                if (child_info.get_attribute_uint32 (FileAttribute.UNIX_NLINK) > 1) {
                                    var hl = HardLink (child_info);

                                    /* check if we've already encountered this file */
                                    if (hl in hardlinks) {
                                        continue;
                                    }

                                    hardlinks += hl;
                                }
                            }

                            results.size += child_info.get_size ();
                            if (child_info.has_attribute (FileAttribute.STANDARD_ALLOCATED_SIZE)) {
                                results.alloc_size += child_info.get_attribute_uint64 (FileAttribute.STANDARD_ALLOCATED_SIZE);
                            }
                            results.elements++;
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

            add_percent (results.size, iter);

            if (!cancellable.is_cancelled ()) {
                set (iter,
                     Columns.SIZE,       results.size,
                     Columns.ALLOC_SIZE, results.alloc_size,
                     Columns.ELEMENTS,   results.elements,
                     Columns.STATE,      State.NEED_PERCENT);
            } else {
                set (iter,
                     Columns.STATE,      State.CANCELLED);
            }

            return results;
        }

        void add_percent (uint64 parent_size, Gtk.TreeIter? parent = null) {
            Gtk.TreeIter iter;

            if (iter_children (out iter, parent)) {
                do {
                    uint64 size;
                    get (iter, Columns.SIZE, out size);
                    set (iter,
                         Columns.PERCENT, 100 * ((double) size) / ((double) parent_size),
                         Columns.STATE,   State.DONE);
                } while (iter_next (ref iter));
            }
        }

        public override void scan () {
            try {
                var info = directory.query_info (ATTRIBUTES, 0, cancellable);
                var results = add_directory (directory, info);
                add_percent (results.size);
                max_depth = results.max_depth;
            } catch { }
        }

        public SyncScanner (File directory, ScanFlags flags) {
            base (directory, flags);
        }
    }
}
