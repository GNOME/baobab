/* -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* Baobab - disk usage analyzer
 *
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

    class ConnectServer : Object {
        const string[] argv = {
            "nautilus-connect-server",
            "--print-uri"
        };

        public signal void selected(string? uri);

        void on_child_watch (Pid pid, int status) {
            Process.close_pid (pid);
        }

        bool on_out_watch (IOChannel channel, IOCondition cond) {
            string? uri = null;

            if (IOCondition.IN in cond) {
                try {
                    size_t len;
                    size_t lineend;
                    channel.read_line(out uri, out len, out lineend);
                    if (len > 0) {
                        uri = uri[0:(int)lineend];
                    }
                } catch {
                }
            }

            selected (uri);

            return false;
        }

        public void show () {

            Pid pid;
            int out_fd;

            try {
                Process.spawn_async_with_pipes (
                    null,
                    argv,
                    null, // envp
                    SpawnFlags.SEARCH_PATH | SpawnFlags.STDERR_TO_DEV_NULL,
                    null, // child_setup
                    out pid,
                    null, // stdin
                    out out_fd,
                    null // stderr
                );
            } catch (SpawnError e) {
                warning ("Failed to run nautilus-connect-server: %s", e.message);
            }

            var out_channel = new IOChannel.unix_new (out_fd);
            out_channel.add_watch (IOCondition.IN | IOCondition.HUP, on_out_watch);

            ChildWatch.add (pid, on_child_watch);
        }

        public static bool available () {
            return (GLib.Environment.find_program_in_path (argv[0]) != null);
        }
    }
}
