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
	class ThreadedScanner : Scanner {
		AsyncQueue<ResultsArray> results_queue;
		ThreadedScanner? self;

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
			internal uint64 elements;
			internal double percent;
			internal int max_depth;

			// accessed only by the main thread
			internal Gtk.TreeIter iter;
			internal bool iter_is_set;
		}

		Results? add_directory (File directory, FileInfo info, Results? parent = null) {
			var results_array = new ResultsArray ();

			if (directory in excluded_locations) {
				return null;
			}

			var results = new Results ();
			results.display_name = info.get_display_name ();
			results.parse_name = directory.get_parse_name ();
			results.parent = parent;

			results.size = info.get_size ();
			if (info.has_attribute (FileAttribute.STANDARD_ALLOCATED_SIZE)) {
				results.alloc_size = info.get_attribute_uint64 (FileAttribute.STANDARD_ALLOCATED_SIZE);
			}
			results.elements = 1;

			try {
				var children = directory.enumerate_children (ATTRIBUTES, FileQueryInfoFlags.NOFOLLOW_SYMLINKS, cancellable);
				FileInfo? child_info;
				while ((child_info = children.next_file (cancellable)) != null) {
					if (cancellable.is_cancelled ()) {
						break;
					}

					switch (child_info.get_file_type ()) {
						case FileType.DIRECTORY:
							var child = directory.get_child (child_info.get_name ());
							var child_results = add_directory (child, child_info, results);

							if (child_results != null) {
								results.size += child_results.size;
								results.alloc_size += child_results.size;
								results.elements += child_results.elements;
								results.max_depth = int.max (results.max_depth, child_results.max_depth + 1);
								results_array.results += (owned) child_results;
							}
							break;

						case FileType.REGULAR:
							if (child_info.has_attribute (FileAttribute.UNIX_NLINK)) {
								if (child_info.get_attribute_uint32 (FileAttribute.UNIX_NLINK) > 1) {
									var hl = HardLink (child_info);

									// check if we've already encountered this file
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
							// ignore other types (symlinks, sockets, devices, etc)
							break;
					}
				}
			} catch (IOError.PERMISSION_DENIED e) {
			} catch (Error e) {
				warning ("couldn't iterate %s: %s", results.parse_name, e.message);
			}

			foreach (unowned Results child_results in results_array.results) {
				child_results.percent = 100 * ((double) child_results.size) / ((double) results.size);
			}

			/* No early exit.  In order to avoid a potential crash, we absolutely *must* push this onto the
			 * queue after having passed it to any recursive invocation of add_directory() above.  See the large
			 * comment at the top of this class for why.
			 */
			results_queue.push ((owned) results_array);

			return results;
		}

		void* scan_in_thread () {
			try {
				var array = new ResultsArray ();
				var info = directory.query_info (ATTRIBUTES, 0, cancellable);
				var results = add_directory (directory, info);
				results.percent = 100.0;
				array.results += (owned) results;
				results_queue.push ((owned) array);
			} catch {
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

			append (out results.iter, parent_iter);
			set (results.iter,
			     Columns.STATE,        State.SCANNING,
			     Columns.DISPLAY_NAME, results.display_name,
			     Columns.PARSE_NAME,   results.parse_name);
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

					set (results.iter,
					     Columns.SIZE,       results.size,
					     Columns.ALLOC_SIZE, results.alloc_size,
					     Columns.PERCENT,    results.percent,
					     Columns.ELEMENTS,   results.elements,
					     Columns.STATE,      State.DONE);

					if (results.max_depth > max_depth) {
						max_depth = results.max_depth;
					}

					if (results.parent == null) {
						completed ();
					}
				}
			}

			return this.self != null;
		}

		public override void scan () {
			new GLib2.Thread ("scanner", scan_in_thread);
			Timeout.add (100, process_results);
		}

		public ThreadedScanner (File directory) {
			base (directory);

			results_queue = new AsyncQueue<ResultsArray> ();

			// the thread owns a reference on the Scanner object
			this.self = this;
		}
	}
}
