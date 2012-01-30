namespace Baobab {
	[CCode (cheader_filename = "baobab-ringschart.h")]
	public class Ringschart : Chart {
		public Ringschart ();
		public void set_subfoldertips_enabled (bool enabled);
	}

	[CCode (cheader_filename = "baobab-treemap.h")]
	public class Treemap : Chart {
		public Treemap ();
	}

	[CCode (cheader_filename = "baobab-chart.h")]
	public class Chart : Gtk.Widget {
		public virtual signal void item_activated (Gtk.TreeIter iter);

		public void set_model_with_columns (Gtk.TreeModel model, uint name_column, uint size_column, uint info_column, uint percentage_column, uint valid_column, Gtk.TreePath? root);

		public Gtk.TreeModel model { get; set; }
		public Gtk.TreePath root { get; set; }
		public uint max_depth { get; set; }

		public void set_model (Gtk.TreeModel? model);
		public unowned Gtk.TreeModel? get_model ();

		public void set_max_depth (uint max_depth);
		public uint get_max_depth ();

		public void set_root (Gtk.TreePath? path);
		public unowned Gtk.TreePath? get_root ();

		public void freeze_updates ();
		public void thaw_updates ();
		public bool is_frozen ();
	}
}
