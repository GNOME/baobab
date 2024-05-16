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

    [GtkTemplate (ui = "/org/gnome/baobab/ui/baobab-contents-cell.ui")]
    public class ContentsCell : Adw.Bin {
        public Scanner.Results? item { set; get; }

        [GtkCallback]
        private string format_items_cb (int items) {
            if (item == null || items < 0 || item.state == Scanner.State.ERROR)
                return "";

            return format_items (items);
        }
    }
}
