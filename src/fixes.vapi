[CCode (cprefix = "G", gir_namespace = "Gio", gir_version = "2.0", lower_case_cprefix = "g_")]
namespace GLib2 {
	public class Thread {
		public delegate void* ThreadFunc ();
		public Thread (string thread_name, ThreadFunc func);
		public void* join ();
	}
}
