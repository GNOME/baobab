[CCode (cprefix = "G", gir_namespace = "Gio", gir_version = "2.0", lower_case_cprefix = "g_")]
namespace GLib2 {
	public class Thread {
		public delegate void* ThreadFunc ();
		public Thread (string thread_name, ThreadFunc func);
		public void* join ();
	}
}

[CCode (cprefix = "Gtk", gir_namespace = "Gtk", gir_version = "3.0", lower_case_cprefix = "gtk_")]
namespace LocalGtk {
	[CCode (cheader_filename = "gtk/gtk.h", type_id = "gtk_list_box_get_type ()")]
	public class ListBox : Gtk.Container, Atk.Implementor, Gtk.Buildable {
		[CCode (has_construct_function = false, type = "GtkWidget*")]
		public ListBox ();
		public void drag_highlight_row (LocalGtk.ListBoxRow row);
		public void drag_unhighlight_row ();
		public unowned Gtk.Adjustment get_adjustment ();
		public unowned LocalGtk.ListBoxRow get_row_at_index (int index);
		public unowned LocalGtk.ListBoxRow get_row_at_y (int y);
		public unowned LocalGtk.ListBoxRow get_selected_row ();
		public Gtk.SelectionMode get_selection_mode ();
		public void invalidate_filter ();
		public void invalidate_headers ();
		public void invalidate_sort ();
		public void select_row (LocalGtk.ListBoxRow? row);
		public void set_activate_on_single_click (bool single);
		public void set_adjustment (Gtk.Adjustment? adjustment);
		public void set_filter_func (owned LocalGtk.ListBoxFilterFunc? filter_func);
		public void set_header_func (owned LocalGtk.ListBoxUpdateHeaderFunc? update_header);
		public void set_placeholder (Gtk.Widget? placeholder);
		public void set_selection_mode (Gtk.SelectionMode mode);
		public void set_sort_func (owned LocalGtk.ListBoxSortFunc? sort_func);
		public bool activate_on_single_click { get; set; }
		public Gtk.SelectionMode selection_mode { get; set; }
		public virtual signal void activate_cursor_row ();
		public virtual signal void move_cursor (Gtk.MovementStep step, int count);
		public virtual signal void row_activated (LocalGtk.ListBoxRow row);
		public virtual signal void row_selected (LocalGtk.ListBoxRow row);
		public virtual signal void toggle_cursor_row ();
	}
	[CCode (cheader_filename = "gtk/gtk.h", type_id = "gtk_list_box_row_get_type ()")]
	public class ListBoxRow : Gtk.Bin, Atk.Implementor, Gtk.Buildable {
		[CCode (has_construct_function = false, type = "GtkWidget*")]
		public ListBoxRow ();
		public void changed ();
		public unowned Gtk.Widget get_header ();
		public void set_header (Gtk.Widget? header);
	}
	[CCode (cheader_filename = "gtk/gtk.h", instance_pos = 1.9)]
	public delegate bool ListBoxFilterFunc (LocalGtk.ListBoxRow row);
	[CCode (cheader_filename = "gtk/gtk.h", instance_pos = 2.9)]
	public delegate int ListBoxSortFunc (LocalGtk.ListBoxRow row1, LocalGtk.ListBoxRow row2);
	[CCode (cheader_filename = "gtk/gtk.h", instance_pos = 2.9)]
	public delegate void ListBoxUpdateHeaderFunc (LocalGtk.ListBoxRow row, LocalGtk.ListBoxRow before);
}
