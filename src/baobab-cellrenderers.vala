/* -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

    public string format_items (int items) {
        return ngettext ("%d item", "%d items", items).printf (items);
    }

    public string format_time_approximate (uint64 time) {
        if (time == 0) {
            // Translators: when the last modified time is unknown
            return _("Unknown");
        }

        var dt = new DateTime.from_unix_local ((int64) time);
        var now = new DateTime.now_local ();
        var ts = now.difference (dt);
        if (ts < TimeSpan.DAY) {
            // Translators: when the last modified time is today
            return _("Today");
        }
        if (ts < 31 * TimeSpan.DAY) {
            var days = (ulong) (ts / TimeSpan.DAY);
            // Translators: when the last modified time is "days" days ago
            return ngettext ("%lu day", "%lu days", days).printf (days);
        }
        if (ts < 365 * TimeSpan.DAY) {
            var months = (ulong) (ts / (31 * TimeSpan.DAY));
            // Translators: when the last modified time is "months" months ago
            return ngettext ("%lu month", "%lu months", months).printf (months);
        }
        var years = (ulong) (ts / (365 * TimeSpan.DAY));
        // Translators: when the last modified time is "years" years ago
        return ngettext ("%lu year", "%lu years", years).printf (years);
    }
}
