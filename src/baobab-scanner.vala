/* -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

    [Flags]
    public enum ScanFlags {
        NONE,
        EXCLUDE_MOUNTS
    }

    public class Scanner : Gtk.TreeStore {
        public enum Columns {
            DISPLAY_NAME,
            PARSE_NAME,
            PERCENT,
            SIZE,
            ALLOC_SIZE,
            TIME_MODIFIED,
            ELEMENTS,
            STATE,
            ERROR,
            COLUMNS
        }

        public enum State {
            SCANNING,
            CANCELLED,
            NEED_PERCENT,
            ERROR,
            CHILD_ERROR,
            DONE
        }

        public File directory { get; private set; }

        public ScanFlags scan_flags { get; private set; }

        public int max_depth { get; protected set; }

        public signal void completed();

        const string ATTRIBUTES =
            FileAttribute.STANDARD_NAME + "," +
            FileAttribute.STANDARD_DISPLAY_NAME + "," +
            FileAttribute.STANDARD_TYPE + "," +
            FileAttribute.STANDARD_SIZE +  "," +
            FileAttribute.STANDARD_ALLOCATED_SIZE + "," +
            FileAttribute.TIME_MODIFIED + "," +
            FileAttribute.UNIX_NLINK + "," +
            FileAttribute.UNIX_INODE + "," +
            FileAttribute.UNIX_DEVICE + "," +
            FileAttribute.ACCESS_CAN_READ;

        [Compact]
        class HardLink {
            internal uint64 inode;
            internal uint32 device;

            public HardLink (FileInfo info) {
                this.inode = info.get_attribute_uint64 (FileAttribute.UNIX_INODE);
                this.device = info.get_attribute_uint32 (FileAttribute.UNIX_DEVICE);
            }

            public uint hash () {
                return direct_hash ((void*) this.inode) ^ direct_hash ((void*) this.device);
            }

            public bool equal (HardLink other) {
                return this.inode == other.inode && this.device == other.device;
            }
        }

        Thread<void*>? thread = null;
        uint process_result_idle = 0;

        GenericSet<HardLink> hardlinks;
        GenericSet<File> excluded_locations;
        uint32 unix_device = 0;

        bool successful = false;

        /* General overview:
         *
         * We cannot directly modify the treemodel from the worker thread, so we have to have a way to dispatch
         * the results back to the main thread.
         *
         * Each scanned directory gets a 'Results' struct created for it.  If the directory has a parent
         * directory, then the 'parent' pointer is set.  The 'display_name' and 'parse_name' fields are filled
         * in as soon as the struct is created.  This part is done as soon as the directory is encountered.
         *
         * In order to determine all of the information for a particular directory (and finish filling in the
         * results structure), we must scan it and all of its children.  We must also scan all of the siblings
         * of the directory so that we know what percentage of the total size of the parent directory the
         * directory in question is responsible for.
         *
         * After a directory, all of its children and all of its siblings have been scanned, we can do the
         * percentage calculation.  We do this from the iteration that takes care of the parent directory: we
         * collect an array of all of the child directory result structs and when we have them all, we assign
         * the proper percentage to each.  At this point we can report this array of result structs back to the
         * main thread to be added to the treemodel.
         *
         * Back in the main thread, we receive a Results object.  If the results object has not yet had a
         * TreeIter assigned to it, we create it one.  We use the parent results object to determine the correct
         * place in the tree (assigning the parent an iter if required, recursively).  When we create the iter,
         * we fill in the data that existed from the start (ie: display name and parse name) and mark the status
         * of the iter as 'scanning'.
         *
         * For the iter that was actually directly reported (ie: the one that's ready) we record the information
         * into the treemodel and free the results structure (or Vala does it for us).
         *
         * We can be sure that the 'parent' field always points to valid memory because of the nature of the
         * recursion and the queue.  At the time we queue a Results struct for dispatch back to the main thread,
         * its 'parent' is held on the stack by a higher invocation of add_directory().  This invocation will
         * never finish without first pushing its own Results struct onto the queue -- after ours.  It is
         * therefore guaranteed that the 'parent' Results object will not be freed before each child.
         */

        AsyncQueue<ResultsArray> results_queue;
        Scanner? self;
        Cancellable cancellable;
        Error? scan_error;

        [Compact]
        class ResultsArray {
            internal Results[] results;
        }

        [Compact]
        class Results {
            // written in the worker thread on creation
            // read from the main thread at any time
            internal unowned Results? parent;
            internal string display_name;
            internal string parse_name;

            // written in the worker thread before dispatch
            // read from the main thread only after dispatch
            internal uint64 size;
            internal uint64 alloc_size;
            internal uint64 time_modified;
            internal int elements;
            internal double percent;
            internal int max_depth;
            internal Error? error;
            internal bool child_error;

            // accessed only by the main thread
            internal Gtk.TreeIter iter;
            internal bool iter_is_set;
        }

        Results? add_directory (File directory, FileInfo info, Results? parent = null) {
            var results_array = new ResultsArray ();

            var current_unix_device = info.get_attribute_uint32 (FileAttribute.UNIX_DEVICE);
            if (directory in excluded_locations ||
                (ScanFlags.EXCLUDE_MOUNTS in scan_flags && current_unix_device != unix_device)) {
                return null;
            }

            var results = new Results ();
            results.display_name = info.get_display_name ();
            results.parse_name = directory.get_parse_name ();
            results.parent = parent;

            results.time_modified = info.get_attribute_uint64 (FileAttribute.TIME_MODIFIED);

            results.size = info.get_size ();
            results.alloc_size = info.get_attribute_uint64 (FileAttribute.STANDARD_ALLOCATED_SIZE);
            results.elements = 1;
            results.error = null;
            results.child_error = false;

            try {
                var children = directory.enumerate_children (ATTRIBUTES, FileQueryInfoFlags.NOFOLLOW_SYMLINKS, cancellable);
                FileInfo? child_info;
                while ((child_info = children.next_file (cancellable)) != null) {
                    switch (child_info.get_file_type ()) {
                        case FileType.DIRECTORY:
                            var child = directory.get_child (child_info.get_name ());
                            var child_results = add_directory (child, child_info, results);

                            if (child_results != null) {
                                results.size += child_results.size;
                                results.alloc_size += child_results.alloc_size;
                                results.elements += child_results.elements;
                                results.max_depth = int.max (results.max_depth, child_results.max_depth + 1);
                                if (child_results.error != null || child_results.child_error) {
                                    results.child_error = true;
                                }

                                if (results.time_modified < child_results.time_modified) {
                                    results.time_modified = child_results.time_modified;
                                }

                                results_array.results += (owned) child_results;
                            }
                            break;

                        case FileType.REGULAR:
                            if (child_info.has_attribute (FileAttribute.UNIX_NLINK)) {
                                if (child_info.get_attribute_uint32 (FileAttribute.UNIX_NLINK) > 1) {
                                    var hl = new HardLink (child_info);

                                    // check if we've already encountered this file
                                    if (hl in hardlinks) {
                                        continue;
                                    }

                                    hardlinks.add ((owned) hl);
                                }
                            }

                            results.size += child_info.get_size ();
                            results.alloc_size += child_info.get_attribute_uint64 (FileAttribute.STANDARD_ALLOCATED_SIZE);
                            results.elements++;

                            var child_time = child_info.get_attribute_uint64 (FileAttribute.TIME_MODIFIED);
                            if (results.time_modified < child_time) {
                                results.time_modified = child_time;
                            }
                            break;

                        default:
                            // ignore other types (symlinks, sockets, devices, etc)
                            break;
                    }
                }
            } catch (Error e) {
                results.error = e;
            }

            foreach (unowned Results child_results in results_array.results) {
                if (results.size > 0) {
                    child_results.percent = 100 * ((double) child_results.size) / ((double) results.size);
                } else {
                    child_results.percent = 0;
                }
            }

            // No early exit: in order to avoid a potential crash, we absolutely *must* push this onto the
            // queue after having passed it to any recursive invocation of add_directory() above.
            //  See the large comment at the top of this class for why.
            results_queue.push ((owned) results_array);

            return results;
        }

        void* scan_in_thread () {
            try {
                var array = new ResultsArray ();
                var info = directory.query_info (ATTRIBUTES, 0, cancellable);
                unix_device = info.get_attribute_uint32 (FileAttribute.UNIX_DEVICE);
                var results = add_directory (directory, info);
                results.percent = 100.0;
                array.results += (owned) results;
                results_queue.push ((owned) array);
            } catch (Error e) {
            }

            // drop the thread's reference on the Scanner object
            this.self = null;
            return null;
        }

        void ensure_iter_exists (Results results) {
            Gtk.TreeIter? parent_iter;

            if (results.iter_is_set) {
                return;
            }

            if (results.parent != null) {
                ensure_iter_exists (results.parent);
                parent_iter = results.parent.iter;
            } else {
                parent_iter = null;
            }

            prepend (out results.iter, parent_iter);
            set (results.iter,
                 Columns.STATE,        State.SCANNING,
                 Columns.DISPLAY_NAME, results.display_name,
                 Columns.PARSE_NAME,   results.parse_name,
                 Columns.TIME_MODIFIED,results.time_modified);
            results.iter_is_set = true;
        }

        bool process_results () {
            while (true) {
                var results_array = results_queue.try_pop ();

                if (results_array == null) {
                    break;
                }

                foreach (unowned Results results in results_array.results) {
                    ensure_iter_exists (results);

                    State state;
                    if (results.child_error) {
                        state = State.CHILD_ERROR;
                    } else if (results.error != null) {
                        state = State.ERROR;
                    } else {
                        state = State.DONE;
                    }

                    set (results.iter,
                         Columns.SIZE,       results.size,
                         Columns.ALLOC_SIZE, results.alloc_size,
                         Columns.PERCENT,    results.percent,
                         Columns.ELEMENTS,   results.elements,
                         Columns.STATE,      state,
                         Columns.ERROR,      results.error);

                    if (results.max_depth > max_depth) {
                        max_depth = results.max_depth;
                    }

                    // If the user cancelled abort the scan and
                    // report CANCELLED as the error, otherwise
                    // consider the error not fatal and report the
                    // first error we encountered
                    if (results.error != null) {
                        if (results.error is IOError.CANCELLED) {
                            scan_error = results.error;
                            completed ();
                            return false;
                        } else if (scan_error == null) {
                            scan_error = results.error;
                        }
                    }

                    if (results.parent == null) {
                        successful = true;
                        completed ();
                        return false;
                    }
                }
            }

            return this.self != null;
        }

        void cancel_and_reset () {
            cancellable.cancel ();

            if (thread != null) {
                thread.join ();
                thread = null;
            }

            if (process_result_idle != 0) {
                GLib.Source.remove (process_result_idle);
                process_result_idle = 0;
            }

            // Drain the async queue
            var tmp = results_queue.try_pop ();
            while (tmp != null) {
                tmp = results_queue.try_pop ();
            }

            hardlinks = new GenericSet<HardLink> (HardLink.hash, HardLink.equal);

            base.clear ();
            set_sort_column_id (Gtk.SortColumn.UNSORTED, Gtk.SortType.DESCENDING);

            cancellable.reset ();
            scan_error = null;
        }

        public void scan (bool force) {
            if (force) {
                successful = false;
            }

            if (!successful) {
                cancel_and_reset ();

                // the thread owns a reference on the Scanner object
                this.self = this;

                thread = new Thread<void*> ("scanner", scan_in_thread);

                process_result_idle = Timeout.add (100, () => {
                        bool res = process_results();
                        if (!res) {
                            process_result_idle = 0;
                        }
                        return res;
                    });
            } else {
                completed ();
            }
        }

        public void cancel () {
            if (!successful) {
                cancel_and_reset ();
            }
        }

        public void finish () throws Error {
            set_sort_column_id (Columns.SIZE, Gtk.SortType.DESCENDING);

            if (scan_error != null) {
                throw scan_error;
            }
        }

        public Scanner (File directory, ScanFlags flags) {
            this.directory = directory;
            this.scan_flags = flags;
            cancellable = new Cancellable();
            scan_error = null;
            set_column_types (new Type[] {
                typeof (string),  // DIR_NAME
                typeof (string),  // PARSE_NAME
                typeof (double),  // PERCENT
                typeof (uint64),  // SIZE
                typeof (uint64),  // ALLOC_SIZE
                typeof (uint64),  // TIME_MODIFIED
                typeof (int),     // ELEMENTS
                typeof (State),   // STATE
                typeof (Error)    // ERROR (if STATE is ERROR)
            });

            excluded_locations = Application.get_default ().get_excluded_locations ();

            results_queue = new AsyncQueue<ResultsArray> ();
        }
    }
}
