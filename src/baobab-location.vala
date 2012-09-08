/* -*- indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

    public class Location {
        public string name { get; private set; }
        public File? file { get; private set; }
        public FileInfo? info { get; private set; }
        public bool is_volume { get; private set; default = true; }

        public uint64? size { get; private set; }
        public uint64? used { get; private set; }
        public uint64? reserved { get; private set; }
        public Icon? icon { get; private set; }

        public Volume? volume { get; private set; }
        public Mount? mount { get; private set; }

        public Scanner? scanner { get; private set; }

        private static const string FS_ATTRIBUTES =
            FileAttribute.FILESYSTEM_SIZE + "," +
            FileAttribute.FILESYSTEM_USED;

        private static const string FILE_ATTRIBUTES =
            FileAttribute.STANDARD_DISPLAY_NAME + "," +
            FileAttribute.STANDARD_ICON + "," +
            FileAttribute.STANDARD_TYPE;

        private static Location? home_location = null;

        void make_this_home_location () {
            name = _("Home folder");
            icon = new ThemedIcon ("user-home");

            home_location = this;
        }

        Location.for_home_folder () {
            is_volume = false;
            file = File.new_for_path (GLib.Environment.get_home_dir ());
            get_file_info ();
            get_fs_usage ();

            make_this_home_location ();

            scanner = new Scanner (file, ScanFlags.NONE);
        }

        public static Location get_home_location () {
            if (home_location == null) {
                home_location = new Location.for_home_folder ();
            }

            return home_location;
        }

        public Location.from_volume (Volume volume_) {
            volume = volume_;
            volume.changed.connect((vol) => {
                update_volume_info ();
            });
            update_volume_info ();
        }

        public Location.from_mount (Mount mount_) {
            mount = mount_;
            fill_from_mount ();
        }

        public Location.for_main_volume () {
            name = _("Main volume");
            file = File.new_for_path ("/");
            get_file_info ();
            icon = new ThemedIcon ("drive-harddisk-system");

            get_fs_usage ();

            scanner = new Scanner (file, ScanFlags.EXCLUDE_MOUNTS);
        }

        public Location.for_recent_info (Gtk.RecentInfo recent_info) {
            is_volume = false; // we assume recent locations are just folders
            file = File.new_for_uri (recent_info.get_uri ());
            name = recent_info.get_display_name ();
            icon = recent_info.get_gicon ();

            if (recent_info.is_local ()) {
                get_fs_usage ();
            }

            scanner = new Scanner (file, ScanFlags.NONE);
        }

        public Location.for_file (File file_) {
            is_volume = false;
            file = file_;
            get_file_info ();

            if (info != null) {
                name = info.get_display_name ();
                icon = info.get_icon ();
            } else {
                name = file_.get_parse_name ();
                icon = null;
            }

            get_fs_usage ();

            scanner = new Scanner (file, ScanFlags.NONE);
        }


        public Location.load_from_file (File report_file) throws Error {
            var parser = new Json.Parser ();
            parser.load_from_file (report_file.get_path ());

            var root_object = parser.get_root ().get_object ();

            name = root_object.get_string_member ("name");

            var scanner_object = root_object.get_object_member ("scanner");
            scanner = new Scanner.from_json_object (scanner_object);
        }

        public void update () {
            update_volume_info ();
        }

        public void save_to_file (string filename) throws Error {
            var root_object = new Json.Object ();
            root_object.set_string_member ("name", name);

            var scanner_object = scanner.to_json_object ();
            root_object.set_object_member ("scanner", scanner_object);

            var root_node = new Json.Node (Json.NodeType.OBJECT);
            root_node.set_object (root_object);

            var generator = new Json.Generator () { pretty = true, root = root_node };
            generator.to_file (filename);
        }

        void update_volume_info () {
            mount = volume.get_mount ();

            if (mount != null) {
                fill_from_mount ();
            } else {
                name = volume.get_name ();
                icon = volume.get_icon ();
                file = null;
                info = null;
                size = 0;
                used = 0;
                scanner = null;
            }
        }

        void fill_from_mount () {
            name = mount.get_name ();
            icon = mount.get_icon ();
            file = mount.get_root ();
            get_file_info ();

            if (file != null && file.equal (File.new_for_path (Environment.get_home_dir ()))) {
                make_this_home_location ();
            }

            get_fs_usage ();

            scanner = new Scanner (file, ScanFlags.EXCLUDE_MOUNTS);
        }

        void get_file_info () {
            try {
                info = file.query_info (FILE_ATTRIBUTES, FileQueryInfoFlags.NONE, null);
            } catch (Error e) {
                info = null;
            }
        }

        void get_fs_usage () {
            size = null;
            used = null;
            reserved = null;
            try {
                var info = file.query_filesystem_info (FS_ATTRIBUTES, null);
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
            } catch (Error e) {
            }
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
