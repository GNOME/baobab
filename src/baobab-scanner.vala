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

    public class Scanner : Object {
        public enum State {
            SCANNING,
            ERROR,
            CHILD_ERROR,
            DONE
        }

        public Results root { get; set; }

        public File directory { get; private set; }

        public ScanFlags scan_flags { get; private set; }

        public bool show_allocated_size { get; private set; }

        // Used for progress reporting, should be updated whenever a new Results object is created
        public uint64 total_size { get; private set; }
        public int total_elements { get; private set; }

        public int max_depth { get; protected set; }

        public signal void completed();

        public File get_file (Results results) {
            List<string> names = null;

            for (Results? child = results; child != null; child = child.parent) {
                names.prepend (child.name);
            }

            var file = directory;
            foreach (var name in names.next) {
                file = file.get_child (name);
            }

            return file;
        }

        const string ATTRIBUTES =
            FileAttribute.STANDARD_NAME + "," +
            FileAttribute.STANDARD_DISPLAY_NAME + "," +
            FileAttribute.STANDARD_TYPE + "," +
            FileAttribute.STANDARD_SIZE +  "," +
            FileAttribute.STANDARD_ALLOCATED_SIZE + "," +
            FileAttribute.TIME_MODIFIED + "," +
            FileAttribute.UNIX_NLINK + "," +
            FileAttribute.UNIX_INODE + "," +
            FileAttribute.UNIX_DEVICE;

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
        GenericSet<string> excluded_locations;
        uint32 unix_device = 0;

        bool successful = false;

        /* General overview:
         *
         * We cannot directly modify the model from the worker thread, so we have to have a way to dispatch
         * the results back to the main thread.
         *
         * Each scanned directory gets a 'Results' object created for it.  If the directory has a parent
         * directory, then the 'parent' pointer is set.  The 'display_name' and 'name' fields are filled
         * in as soon as the object is created.  This part is done as soon as the directory is encountered.
         *
         * In order to determine all of the information for a particular directory (and finish filling in the
         * results object), we must scan it and all of its children.  We must also scan all of the siblings
         * of the directory so that we know what percentage of the total size of the parent directory the
         * directory in question is responsible for.
         *
         * After a directory, all of its children and all of its siblings have been scanned, we can do the
         * percentage calculation.  We do this from the iteration that takes care of the parent directory: we
         * collect an array of all of the child directory result objects and when we have them all, we assign
         * the proper percentage to each.  At this point we can report this array of result objects back to the
         * main thread to be added to the model.
         *
         * Back in the main thread, we receive a Results object.  We add the object to its parent's children
         * list, we fill in the data that existed from the start (ie: display name and name).
         *
         * We can be sure that the 'parent' field always points to valid memory because of the nature of the
         * recursion and the queue.  At the time we queue a Results object for dispatch back to the main thread,
         * its 'parent' is held on the stack by a higher invocation of add_directory().  This invocation will
         * never finish without first pushing its own Results object onto the queue -- after ours.  It is
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

        public class Results : Object {
            // written in the worker thread on creation
            // read from the main thread at any time
            public unowned Results? parent { get; internal set; }
            public string name { get; internal set; }
            public string display_name { get; internal set; }
            internal FileType file_type;

            // written in the worker thread before dispatch
            // read from the main thread only after dispatch
            public uint64 size { get; internal set; }
            public uint64 time_modified { get; internal set; }
            public int elements { get; internal set; }
            internal int max_depth;
            internal Error? error;
            internal bool child_error;

            // accessed only by the main thread
            public GLib.ListStore children_list_store { get; construct set; }
            public State state { get; internal set; }

            double _percent;
            public double percent {
                get { return _percent; }
                internal set {
                    _percent = value;

                    notify_property ("fraction");
                }
            }

            public double fraction {
                get {
                    return _percent / 100.0;
                }
            }

            // No need to notify that property when the number of children
            // changes as the whole model won't change once constructed.
            public bool is_empty {
                get { return children_list_store.n_items == 0; }
            }

            construct {
                children_list_store = new ListStore (typeof (Results));
            }

            public Results (FileInfo info, Results? parent_results) {
                parent = parent_results;
                name = info.get_name ();
                display_name = info.get_display_name ();
                if (display_name == null && name != null) {
                    display_name = Filename.display_name (name);
                }
                if (display_name == null) {
                    display_name = "";
                }
                file_type = info.get_file_type ();
                size = info.get_attribute_uint64 (FileAttribute.STANDARD_ALLOCATED_SIZE);
                if (size == 0) {
                    size = info.get_size ();
                }
                time_modified = info.get_attribute_uint64 (FileAttribute.TIME_MODIFIED);
                elements = 1;
                error = null;
                child_error = false;
            }

            public Results.empty () {
            }

            public void update_with_child (Results child) {
                size         += child.size;
                elements     += child.elements;
                max_depth     = int.max (max_depth, child.max_depth + 1);
                child_error  |= child.child_error || (child.error != null);
                time_modified = uint64.max (time_modified, child.time_modified);
            }

            public int get_depth () {
                int depth = 1;
                for (var ancestor = parent; ancestor != null; ancestor = ancestor.parent) {
                    depth++;
                }
                return depth;
            }

            public bool is_ancestor (Results? descendant) {
                for (; descendant != null; descendant = descendant.parent) {
                    if (descendant == this)
                        return true;
                }
                return descendant == this;
            }

            public Gtk.TreeListModel create_tree_model () {
                return new Gtk.TreeListModel (children_list_store, false, false, (item) => {
                    var results = item as Scanner.Results;
                    return results == null ? null : results.children_list_store;
                });
            }
        }

        void add_children (File directory, Results results, ResultsArray results_array) throws Error {
            var children = directory.enumerate_children (ATTRIBUTES, FileQueryInfoFlags.NOFOLLOW_SYMLINKS, cancellable);
            FileInfo? child_info;
            while ((child_info = children.next_file (cancellable)) != null) {
                switch (child_info.get_file_type ()) {
                    case FileType.DIRECTORY:
                        var child = directory.get_child (child_info.get_name ());
                        var child_results = add_directory (child, child_info, results);

                        if (child_results != null) {
                            results.update_with_child (child_results);
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

                        var child_results = new Results (child_info, results);
                        total_size += child_results.size;
                        total_elements++;
                        results.update_with_child (child_results);
                        results_array.results += (owned) child_results;
                        break;

                    default:
                        // ignore other types (symlinks, sockets, devices, etc)
                        break;
                }
            }
        }

        Results? add_directory (File directory, FileInfo info, Results? parent = null) {
            var results_array = new ResultsArray ();

            var current_unix_device = info.get_attribute_uint32 (FileAttribute.UNIX_DEVICE);
            if (ScanFlags.EXCLUDE_MOUNTS in scan_flags && current_unix_device != unix_device) {
                return null;
            }

            var results = new Results (info, parent);
            if (directory.get_uri () in excluded_locations) {
                results.error = new IOError.FAILED ("Location is excluded");
                return results;
            }

            total_size += results.size;
            total_elements++;

            try {
                add_children (directory, results, results_array);
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

                // Use allocated size if available, where availability is defined as allocated size > 0
                // for the root element. This is consistent with how we attribute sizes in the Results structure
                show_allocated_size = info.get_attribute_uint64 (FileAttribute.STANDARD_ALLOCATED_SIZE) > 0;

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

        bool process_results () {
            while (true) {
                var results_array = results_queue.try_pop ();

                if (results_array == null) {
                    break;
                }

                foreach (unowned Results results in results_array.results) {
                    if (results.parent != null) {
                        results.parent.children_list_store.insert (0, results);
                    }

                    State state;
                    if (results.child_error) {
                        results.state = State.CHILD_ERROR;
                    } else if (results.error != null) {
                        results.state = State.ERROR;
                    } else {
                        results.state = State.DONE;
                    }

                    if (results.max_depth > max_depth) {
                        max_depth = results.max_depth;
                    }

                    // We reached the root, we're done
                    if (results.parent == null) {
                        this.root = results;
                        scan_error = results.error;
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

            cancellable.reset ();
            scan_error = null;
            total_size = 0;
            total_elements = 0;

            excluded_locations = Application.get_default ().get_excluded_locations ();
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
                scan_error = new IOError.CANCELLED ("Scan was cancelled");
            }
            completed ();
        }

        public void finish () throws Error {
            if (scan_error != null) {
                throw scan_error;
            }
        }

        public Scanner (File directory, ScanFlags flags) {
            this.directory = directory;
            this.scan_flags = flags;
            cancellable = new Cancellable();
            scan_error = null;

            results_queue = new AsyncQueue<ResultsArray> ();
        }
    }
}
