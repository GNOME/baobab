/* -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* Baobab - disk usage analyzer
 *
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

    [DBus (name = "org.freedesktop.hostname1")]
    interface HostnameIface : Object {
        public abstract string pretty_hostname { owned get; set; }
        public abstract string hostname { owned get; set; }
    }

    public class Location {
        public string name { get; private set; }
        public File? file { get; private set; }

        public uint64? size { get; private set; }
        public uint64? used { get; private set; }
        public uint64? reserved { get; private set; }
        public Icon? icon { get; private set; }
        public Icon? symbolic_icon { get; private set; }

        public Volume? volume { get; private set; }
        public Mount? mount { get; private set; }

        public bool is_main_volume { get; private set; default = false; }
        public bool is_remote { get; private set; default = false; }
        public bool is_recent { get; private set; default = false; }

        public Scanner? scanner { get; private set; }

        public signal void changed ();
        public signal void progress ();

        private bool querying_fs = false;

        private const string FS_ATTRIBUTES =
            FileAttribute.FILESYSTEM_SIZE + "," +
            FileAttribute.FILESYSTEM_USED + "," +
            FileAttribute.FILESYSTEM_FREE;

        string get_hostname () throws Error {
            HostnameIface hostname_iface;
            hostname_iface = Bus.get_proxy_sync (BusType.SYSTEM,
                                                 "org.freedesktop.hostname1",
                                                 "/org/freedesktop/hostname1");
            var pretty_name = hostname_iface.pretty_hostname;
            if (pretty_name != "") {
                return pretty_name;
            } else {
                return hostname_iface.hostname;
            }
        }

        void make_this_home_location () {
            name = _("Home Folder");
            icon = new ThemedIcon ("user-home");
            symbolic_icon = new ThemedIcon ("user-home-symbolic");
        }

        public Location.for_home_folder () {
            file = File.new_for_path (GLib.Environment.get_home_dir ());

            make_this_home_location ();

            scanner = new Scanner (file, ScanFlags.EXCLUDE_MOUNTS);
        }

        public Location.from_volume (Volume volume_) {
            volume = volume_;
            update_volume_info ();
        }

        public Location.from_mount (Mount mount_) {
            mount = mount_;
            fill_from_mount ();

            var uri_scheme = Uri.parse_scheme (file.get_uri ());
            string[] remote_schemes = { "ftp", "sftp", "ssh", "smb", "google-drive", "afp" };
            is_remote = (uri_scheme in remote_schemes);
        }

        public Location.for_main_volume () {
            try {
                name = get_hostname ();
            } catch (Error e) {
                name = null;
            }

            if (name == null) {
                name = _("Computer");
            }

            file = File.new_for_path ("/");
            icon = new ThemedIcon.with_default_fallbacks ("drive-harddisk-system");
            symbolic_icon = new ThemedIcon.with_default_fallbacks ("drive-harddisk-system-symbolic");
            is_main_volume = true;

            scanner = new Scanner (file, ScanFlags.EXCLUDE_MOUNTS);
        }

        public Location.for_recent_info (Gtk.RecentInfo recent_info) {
            is_recent = true;
            file = File.new_for_uri (recent_info.get_uri ());
            name = recent_info.get_display_name ();
            icon = recent_info.get_gicon ();
            symbolic_icon = new ThemedIcon ("folder-symbolic");

            scanner = new Scanner (file, ScanFlags.EXCLUDE_MOUNTS);
        }

        public Location.for_file (File file_, ScanFlags flags) {
            file = file_;

            try {
                var info = file.query_info (FileAttribute.STANDARD_DISPLAY_NAME, FileQueryInfoFlags.NONE, null);
                name = info.get_display_name ();
            } catch {
                name = Filename.display_name (file.get_parse_name ());
            }

            scanner = new Scanner (file, flags);
        }

        public void update_volume_info () {
            is_remote = volume.get_identifier (VolumeIdentifier.CLASS) == "network";

            var new_mount = volume.get_mount ();
            if (new_mount != null) {
                if (new_mount != mount) {
                    mount = new_mount;
                    fill_from_mount ();
                }
            } else {
                name = volume.get_name ();
                icon = volume.get_icon ();
                symbolic_icon = volume.get_symbolic_icon ();
                mount = null;
                file = null;
                size = null;
                used = null;
                scanner = null;
            }

            changed ();
        }

        void fill_from_mount () {
            name = mount.get_name ();
            icon = mount.get_icon ();
            symbolic_icon = mount.get_symbolic_icon ();
            file = mount.get_root ();

            if (file != null && file.equal (File.new_for_path (Environment.get_home_dir ()))) {
                make_this_home_location ();
            }

            scanner = new Scanner (file, ScanFlags.EXCLUDE_MOUNTS);
        }

        public void queue_query_fs_usage () {
            if (querying_fs || file == null) {
                return;
            }

            querying_fs = true;

            file.query_filesystem_info_async.begin (FS_ATTRIBUTES, Priority.DEFAULT, null, (obj, res) => {
                querying_fs = false;

                size = null;
                used = null;
                reserved = null;

                try {
                    var info = file.query_filesystem_info_async.end (res);
                    if (info.has_attribute (FileAttribute.FILESYSTEM_SIZE)) {
                        size = info.get_attribute_uint64 (FileAttribute.FILESYSTEM_SIZE);
                    }
                    if (info.has_attribute (FileAttribute.FILESYSTEM_USED)) {
                        used = info.get_attribute_uint64 (FileAttribute.FILESYSTEM_USED);
                    }
                    if (size != null && used != null && info.has_attribute (FileAttribute.FILESYSTEM_FREE)) {
                        var free = info.get_attribute_uint64 (FileAttribute.FILESYSTEM_FREE);
                        reserved = size - free - used;
                    }

                    // this can happen sometimes with remote mounts. The result, if
                    // unchecked, is a uint64 underflow and the drive being presented
                    // with 18.4 EB free
                    if (size != null && used != null && used > size) {
                        size = null;
                    }

                    changed ();
                } catch (Error e) {
                }

            });
        }

        public async void mount_volume () throws Error {
            if (mount != null || volume == null)
                return;

            var mount_op = new Gtk.MountOperation (null);
            yield volume.mount (MountMountFlags.NONE, mount_op, null);

            update_volume_info ();
        }
    }
}
