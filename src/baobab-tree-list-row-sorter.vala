/* -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* Baobab - disk usage analyzer
 *
 * Copyright © 2024 Adrien Plazas <aplazas@gnome.org>
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

    // This is a reimplementation of Gtk.TreeListRowSorter adding the sort-order
    // property to work around this issue:
    // https://gitlab.gnome.org/GNOME/gtk/-/issues/6695
    class TreeListRowSorter : Gtk.Sorter {
        private Gtk.Sorter _sorter;
        [CCode(notify = false)]
        public Gtk.Sorter sorter {
            get { return _sorter; }
            set {
                if (value == null)
                    return;

                if (_sorter == value)
                    return;

                if (_sorter != null)
                    _sorter.changed.disconnect (propagate_changed);

                _sorter = value;
                _sorter.changed.connect (propagate_changed);

                changed (Gtk.SorterChange.DIFFERENT);
                notify_property ("sorter");
            }
        }

        private Gtk.SortType _sort_order;
        [CCode(notify = false)]
        public Gtk.SortType sort_order {
            get { return _sort_order; }
            set {
                if (_sort_order == value)
                    return;

                _sort_order = value;

                changed (Gtk.SorterChange.DIFFERENT);
                notify_property ("sort-order");
            }
        }

        TreeListRowSorter (Gtk.Sorter sorter) {
            Object (sorter: sorter);
        }

        ~TreeListRowSorter () {
            if (_sorter != null)
                _sorter.changed.disconnect (propagate_changed);
        }

        protected override Gtk.Ordering compare (Object? item1, Object? item2) {
            /* break ties here so we really are a total order */
            if (!(item1 is Gtk.TreeListRow)) {
                if (item2 is Gtk.TreeListRow) {
                    return Gtk.Ordering.LARGER;
                } else {
                    return &item1 < &item2 ? Gtk.Ordering.SMALLER : Gtk.Ordering.LARGER;
                }
            } else if (!(item2 is Gtk.TreeListRow)) {
                return Gtk.Ordering.SMALLER;
            }

            var row1 = item1 as Gtk.TreeListRow;
            var row2 = item2 as Gtk.TreeListRow;

            var depth1 = row1.get_depth ();
            var depth2 = row2.get_depth ();

            Gtk.Ordering result = Gtk.Ordering.EQUAL;

            /* First, get to the same depth */
            while (depth1 > depth2) {
                row1 = row1.get_parent ();
                depth1--;
                if (sort_order == Gtk.SortType.ASCENDING) {
                    result = Gtk.Ordering.LARGER;
                } else {
                    result = Gtk.Ordering.SMALLER;
                }
            }
            while (depth2 > depth1) {
                row2 = row2.get_parent ();
                depth2--;
                if (sort_order == Gtk.SortType.ASCENDING) {
                    result = Gtk.Ordering.SMALLER;
                } else {
                    result = Gtk.Ordering.LARGER;
                }
            }

            /* Now walk up until we find a common parent */
            if (row1 == row2) {
                return result;
            }

            while (true) {
                var parent1 = row1.get_parent ();
                var parent2 = row2.get_parent ();

                if (parent1 != parent2) {
                    row1 = parent1;
                    row2 = parent2;

                    continue;
                }

                if (sorter == null)
                    result = Gtk.Ordering.EQUAL;
                else
                    result = sorter.compare (row1.get_item (), row2.get_item ());

                /* We must break ties here because if row1 ever gets a child,
                * it would need to go right in between row1 and row2. */
                if (result == Gtk.Ordering.EQUAL) {
                    if (row1.get_position () < row2.get_position ()) {
                        if (sort_order == Gtk.SortType.ASCENDING) {
                            result = Gtk.Ordering.SMALLER;
                        } else {
                            result = Gtk.Ordering.LARGER;
                        }
                    } else {
                        if (sort_order == Gtk.SortType.ASCENDING) {
                            result = Gtk.Ordering.LARGER;
                        } else {
                            result = Gtk.Ordering.SMALLER;
                        }
                    }
                }

                break;
            }

            return result;
        }

        protected override Gtk.SorterOrder get_order () {
            return Gtk.SorterOrder.TOTAL;
        }

        private void propagate_changed (Gtk.SorterChange change) {
            changed (change);
        }
    }
}
