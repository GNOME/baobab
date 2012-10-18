namespace Cc {
	[CCode (cheader_filename = "cc-notebook.h")]
	public class Notebook : Gtk.Box {
		[CCode (has_construct_function = false)]
		public Notebook ();
		public void add_page (Gtk.Widget widget);
		public void remove_page (Gtk.Widget widget);
		public void select_page (Gtk.Widget widget, bool animate);
		public Gtk.Widget get_selected_page ();
	}
}
