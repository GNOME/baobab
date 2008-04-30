/* Baobab - (C) 2005 Fabio Marzocca

	baobab-remote-connect-dialog.c

   Modified module from nautilus-connect-server-dialog.c
   Released under same licence
 */
/*
 * Nautilus
 *
 * Copyright (C) 2003 Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include "baobab-remote-connect-dialog.h"

#include <string.h>

#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-volume.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "baobab.h"

struct _BaobabRemoteConnectDialogDetails {
	
	char *uri;

	GtkWidget *required_table;
	GtkWidget *optional_table;
	
	GtkWidget *type_combo;
	GtkWidget *uri_entry;
	GtkWidget *server_entry;
	GtkWidget *share_entry;
	GtkWidget *port_entry;
	GtkWidget *folder_entry;
	GtkWidget *domain_entry;
	GtkWidget *user_entry;

	GtkWidget *name_entry;
};

static void  baobab_remote_connect_dialog_class_init       (BaobabRemoteConnectDialogClass *class);
static void  baobab_remote_connect_dialog_init             (BaobabRemoteConnectDialog      *dialog);

#define RESPONSE_CONNECT GTK_RESPONSE_OK
/*
enum {
	
	RESPONSE_CONNECT
};	
*/

/* Keep this order in sync with strings below */
enum {
	TYPE_SSH,
	TYPE_ANON_FTP,
	TYPE_FTP,
	TYPE_SMB,
	TYPE_DAV,
	TYPE_DAVS,
	TYPE_URI
};

G_DEFINE_TYPE(BaobabRemoteConnectDialog, baobab_remote_connect_dialog, GTK_TYPE_DIALOG)

static void
baobab_remote_connect_dialog_finalize (GObject *object)
{
	BaobabRemoteConnectDialog *dialog;

	dialog = BAOBAB_REMOTE_CONNECT_DIALOG(object);

	g_free (dialog->details->uri);

	g_object_unref (dialog->details->uri_entry);
	g_object_unref (dialog->details->server_entry);
	g_object_unref (dialog->details->share_entry);
	g_object_unref (dialog->details->port_entry);
	g_object_unref (dialog->details->folder_entry);
	g_object_unref (dialog->details->domain_entry);
	g_object_unref (dialog->details->user_entry);
	g_object_unref (dialog->details->name_entry);
	
	g_free (dialog->details);

	G_OBJECT_CLASS (baobab_remote_connect_dialog_parent_class)->finalize (object);
}

static gboolean
remote_connect (BaobabRemoteConnectDialog *dialog)
{
	char *uri;
	char *user_uri;
	GnomeVFSURI *vfs_uri;
	char *error_message;
	char *name;
	int type;

	g_free (dialog->details->uri);
	dialog->details->uri = NULL;	

	type = gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->details->type_combo));

	if (type == TYPE_URI) {
		user_uri = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->uri_entry), 0, -1);
		uri = gnome_vfs_make_uri_from_input (user_uri);
		g_free (user_uri);
	
		vfs_uri = gnome_vfs_uri_new (uri);
		
		if (vfs_uri == NULL) {
			GtkWidget *dlg;

			error_message = g_strdup_printf (_("\"%s\" is not a valid location"), uri);
			
			dlg = gtk_message_dialog_new (GTK_WINDOW (dialog),
						      GTK_DIALOG_DESTROY_WITH_PARENT,
						      GTK_MESSAGE_ERROR,
						      GTK_BUTTONS_OK,
						      error_message);

			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dlg),
					_("Please check the spelling and try again."));

			g_free(error_message);

			gtk_dialog_run (GTK_DIALOG (dlg));
    			gtk_widget_destroy (dlg);

			return FALSE;
		}
		else {
			gnome_vfs_uri_unref (vfs_uri);
		}
	}
	else {
		char *method, *user, *port, *initial_path, *server, *folder, *domain;
		char *t, *join;
		gboolean free_initial_path, free_user, free_domain, free_port;

		server = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->server_entry), 0, -1);
		if (strlen (server) == 0) {
			GtkWidget *dlg;

			dlg = gtk_message_dialog_new (GTK_WINDOW (dialog),
						      GTK_DIALOG_DESTROY_WITH_PARENT,
						      GTK_MESSAGE_ERROR,
						      GTK_BUTTONS_OK,
						      _("You must enter a name for the server"));

			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dlg),
					_("Please enter a name and try again."));

			g_free(server);

			gtk_dialog_run (GTK_DIALOG (dlg));
    			gtk_widget_destroy (dlg);

			return FALSE;
		}
		
		method = "";
		user = "";
		port = "";
		initial_path = "";
		domain = "";
		free_initial_path = FALSE;
		free_user = FALSE;
		free_domain = FALSE;
		free_port = FALSE;
		switch (type) {
		case TYPE_SSH:
			method = "sftp";
			break;
		case TYPE_ANON_FTP:
			method = "ftp";
			user = "anonymous";
			break;
		case TYPE_FTP:
			method = "ftp";
			break;
		case TYPE_SMB:
			method = "smb";
			t = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->share_entry), 0, -1);
			initial_path = g_strconcat ("/", t, NULL);
			free_initial_path = TRUE;
			g_free (t);
			break;
		case TYPE_DAV:
			method = "dav";
			break;
		case TYPE_DAVS:
			method = "davs";
			break;
		}

		if (dialog->details->port_entry->parent != NULL) {
			free_port = TRUE;
			port = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->port_entry), 0, -1);
		}
		folder = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->folder_entry), 0, -1);
		if (dialog->details->user_entry->parent != NULL) {
			free_user = TRUE;
			
			t = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->user_entry), 0, -1);

			user = gnome_vfs_escape_string (t);

			g_free (t);
		}
		if (dialog->details->domain_entry->parent != NULL) {
			free_domain = TRUE;

			domain = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->domain_entry), 0, -1);
			
			if (strlen (domain) != 0) {
				t = user;

				user = g_strconcat (domain , ";" , t, NULL);

				if (free_user) {
					g_free (t);
				}

				free_user = TRUE;
			}
		}

		if (folder[0] != 0 &&
		    folder[0] != '/') {
			join = "/";
		} else {
			join = "";
		}

		t = folder;
		folder = g_strconcat (initial_path, join, t, NULL);
		g_free (t);

		t = folder;
		folder = gnome_vfs_escape_path_string (t);
		g_free (t);

		uri = g_strdup_printf ("%s://%s%s%s%s%s%s",
				       method,
				       user, (user[0] != 0) ? "@" : "",
				       server,
				       (port[0] != 0) ? ":" : "", port,
				       folder);

		if (free_initial_path) {
			g_free (initial_path);
		}
		g_free (server);
		if (free_port) {
			g_free (port);
		}
		g_free (folder);
		if (free_user) {
			g_free (user);
		}
		if (free_domain) {
			g_free (domain);
		}
	}
	
	name = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->name_entry), 0, -1);
	if (strlen (name) == 0) {
		const char *host, *path;
		char *path_utf8, *basename;
		
		g_free (name);
		
		vfs_uri = gnome_vfs_uri_new (uri);
	
		if (vfs_uri == NULL) {
			g_warning ("Illegal uri in connect to server!\n");
			g_free (uri);
			g_free (name);
			return FALSE;
		} 

		host = gnome_vfs_uri_get_host_name (vfs_uri);
		path = gnome_vfs_uri_get_path (vfs_uri);
		if (path != NULL &&
		    strlen (path) > 0 &&
		    strcmp (path, "/") != 0) {
			path_utf8 = gnome_vfs_format_uri_for_display (uri);
			basename = g_path_get_basename (path_utf8);
			name = g_strdup_printf (_("%s on %s"), basename, host);
			g_free (path_utf8);
			g_free (basename);
		} else {
			name = g_strdup (host);
		}
		gnome_vfs_uri_unref (vfs_uri);
	}

	dialog->details->uri = uri;

	g_free (name);

	return TRUE;
}

static void
response_callback (BaobabRemoteConnectDialog *dialog,
		   int response_id,
		   gpointer data)
{
	switch (response_id) {
	case RESPONSE_CONNECT:
		if (!remote_connect (dialog))
			g_signal_stop_emission_by_name (dialog, "response");
		break;
	case GTK_RESPONSE_NONE:
	case GTK_RESPONSE_DELETE_EVENT:
	case GTK_RESPONSE_CANCEL:

		break;
	default :
		g_assert_not_reached ();
	}
}

static void
baobab_remote_connect_dialog_class_init (BaobabRemoteConnectDialogClass *class)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = baobab_remote_connect_dialog_finalize;
}

static void
setup_for_type (BaobabRemoteConnectDialog *dialog)
{
	int type, i;
	gboolean show_share, show_port, show_user, show_domain;
	GtkWidget *align, *label, *table;
	gchar *str;

	type = gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->details->type_combo));

	if (dialog->details->uri_entry->parent != NULL) {
		gtk_container_remove (GTK_CONTAINER (dialog->details->required_table),
				      dialog->details->uri_entry);
	}
	if (dialog->details->server_entry->parent != NULL) {
		gtk_container_remove (GTK_CONTAINER (dialog->details->required_table),
				      dialog->details->server_entry);
	}
	if (dialog->details->share_entry->parent != NULL) {
		gtk_container_remove (GTK_CONTAINER (dialog->details->optional_table),
				      dialog->details->share_entry);
	}
	if (dialog->details->port_entry->parent != NULL) {
		gtk_container_remove (GTK_CONTAINER (dialog->details->optional_table),
				      dialog->details->port_entry);
	}
	if (dialog->details->folder_entry->parent != NULL) {
		gtk_container_remove (GTK_CONTAINER (dialog->details->optional_table),
				      dialog->details->folder_entry);
	}
	if (dialog->details->user_entry->parent != NULL) {
		gtk_container_remove (GTK_CONTAINER (dialog->details->optional_table),
				      dialog->details->user_entry);
	}
	if (dialog->details->domain_entry->parent != NULL) {
		gtk_container_remove (GTK_CONTAINER (dialog->details->optional_table),
				      dialog->details->domain_entry);
	}
	if (dialog->details->name_entry->parent != NULL) {
		gtk_container_remove (GTK_CONTAINER (dialog->details->optional_table),
				      dialog->details->name_entry);
	}
	/* Destroy all labels */
	gtk_container_foreach (GTK_CONTAINER (dialog->details->required_table),
			       (GtkCallback) gtk_widget_destroy, NULL);

	
	i = 1;
	table = dialog->details->required_table;
	
	if (type == TYPE_URI) {
		label = gtk_label_new_with_mnemonic (_("_Location (URI):"));
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_widget_show (label);
		gtk_table_attach (GTK_TABLE (table), label,
				  0, 1,
				  i, i+1,
				  GTK_FILL, GTK_FILL,
				  0, 0);
		
		gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->details->uri_entry);
		gtk_widget_show (dialog->details->uri_entry);
		gtk_table_attach (GTK_TABLE (table), dialog->details->uri_entry,
				  1, 2,
				  i, i+1,
				  GTK_FILL | GTK_EXPAND, GTK_FILL,
				  0, 0);

		i++;
		
		return;
	}
	
	switch (type) {
	default:
	case TYPE_SSH:
	case TYPE_FTP:
	case TYPE_DAV:
	case TYPE_DAVS:
		show_share = FALSE;
		show_port = TRUE;
		show_user = TRUE;
		show_domain = FALSE;
		break;
	case TYPE_ANON_FTP:
		show_share = FALSE;
		show_port = TRUE;
		show_user = FALSE;
		show_domain = FALSE;
		break;
	case TYPE_SMB:
		show_share = TRUE;
		show_port = FALSE;
		show_user = TRUE;
		show_domain =TRUE;
		break;
	}

	label = gtk_label_new_with_mnemonic (_("S_erver:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label,
			  0, 1,
			  i, i+1,
			  GTK_FILL, GTK_FILL,
			  0, 0);
	
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->details->server_entry);
	gtk_widget_show (dialog->details->server_entry);
	gtk_table_attach (GTK_TABLE (table), dialog->details->server_entry,
			  1, 2,
			  i, i+1,
			  GTK_FILL | GTK_EXPAND, GTK_FILL,
			  0, 0);

	i++;

	align = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (align), 12, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), align,
			  0, 2,
			  i, i+1,
			  GTK_FILL, GTK_FILL,
			  0, 0);
	gtk_widget_show (align);
	
	i++;
	
	str = g_strdup_printf ("<b>%s</b>", _("Optional Information"));
	label = gtk_label_new (str);
	g_free (str);
	
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_container_add (GTK_CONTAINER (align), label);
	
	align = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 0, 12, 0);
	gtk_table_attach (GTK_TABLE (table), align,
			  0, 2,
			  i, i+1,
			  GTK_FILL, GTK_FILL,
			  0, 0);
	gtk_widget_show (align);
	
	
	dialog->details->optional_table = table = gtk_table_new (1, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 12);
	gtk_widget_show (table);
	gtk_container_add (GTK_CONTAINER (align), table);
		
	i = 0;
		
	if (show_share) {
		label = gtk_label_new_with_mnemonic (_("Sh_are:"));
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_widget_show (label);
		gtk_table_attach (GTK_TABLE (table), label,
				  0, 1,
				  i, i+1,
				  GTK_FILL, GTK_FILL,
				  0, 0);
		
		gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->details->share_entry);
		gtk_widget_show (dialog->details->share_entry);
		gtk_table_attach (GTK_TABLE (table), dialog->details->share_entry,
				  1, 2,
				  i, i+1,
				  GTK_FILL | GTK_EXPAND, GTK_FILL,
				  0, 0);

		i++;
	}

	if (show_port) {
		label = gtk_label_new_with_mnemonic (_("_Port:"));
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_widget_show (label);
		gtk_table_attach (GTK_TABLE (table), label,
				  0, 1,
				  i, i+1,
				  GTK_FILL, GTK_FILL,
				  0, 0);
		
		gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->details->port_entry);
		gtk_widget_show (dialog->details->port_entry);
		gtk_table_attach (GTK_TABLE (table), dialog->details->port_entry,
				  1, 2,
				  i, i+1,
				  GTK_FILL | GTK_EXPAND, GTK_FILL,
				  0, 0);

		i++;
	}

	label = gtk_label_new_with_mnemonic (_("_Folder:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label,
			  0, 1,
			  i, i+1,
			  GTK_FILL, GTK_FILL,
			  0, 0);
	
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->details->folder_entry);
	gtk_widget_show (dialog->details->folder_entry);
	gtk_table_attach (GTK_TABLE (table), dialog->details->folder_entry,
			  1, 2,
			  i, i+1,
			  GTK_FILL | GTK_EXPAND, GTK_FILL,
			  0, 0);

	i++;

	if (show_user) {
		label = gtk_label_new_with_mnemonic (_("_User name:"));
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_widget_show (label);
		gtk_table_attach (GTK_TABLE (table), label,
				  0, 1,
				  i, i+1,
				  GTK_FILL, GTK_FILL,
				  0, 0);
		
		gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->details->user_entry);
		gtk_widget_show (dialog->details->user_entry);
		gtk_table_attach (GTK_TABLE (table), dialog->details->user_entry,
				  1, 2,
				  i, i+1,
				  GTK_FILL | GTK_EXPAND, GTK_FILL,
				  0, 0);

		i++;
	}

	if (show_domain) {
		label = gtk_label_new_with_mnemonic (_("_Domain name:"));
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_widget_show (label);
		gtk_table_attach (GTK_TABLE (table), label,
				  0, 1,
				  i, i+1,
				  GTK_FILL, GTK_FILL,
				  0, 0);

                gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->details->domain_entry);
                gtk_widget_show (dialog->details->domain_entry);
                gtk_table_attach (GTK_TABLE (table), dialog->details->domain_entry,
                                  1, 2,
                                  i, i+1,
                                  GTK_FILL | GTK_EXPAND, GTK_FILL,
                                  0, 0);

                i++;
        }

	
}

static void
combo_changed_callback (GtkComboBox *combo_box,
			BaobabRemoteConnectDialog *dialog)
{
	setup_for_type (dialog);
}


static void
port_insert_text (GtkEditable *editable,
		  const gchar *new_text,
		  gint         new_text_length,
		  gint        *position)
{
	if (new_text_length < 0) {
		new_text_length = strlen (new_text);
	}

	if (new_text_length != 1 ||
	    !g_ascii_isdigit (new_text[0])) {
		gdk_display_beep (gtk_widget_get_display (GTK_WIDGET (editable)));
		g_signal_stop_emission_by_name (editable, "insert_text");
	}
}

static void
baobab_remote_connect_dialog_init (BaobabRemoteConnectDialog *dialog)
{
	GtkWidget *align;
	GtkWidget *label;
	GtkWidget *table;
	GtkWidget *combo;
	GtkWidget *hbox;
	GtkWidget *vbox;
	gchar *str;
	
	dialog->details = g_new0 (BaobabRemoteConnectDialogDetails, 1);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Scan Remote Folder"));
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    vbox, FALSE, TRUE, 0);
	gtk_widget_show (vbox);
			    
	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox),
			    hbox, FALSE, TRUE, 0);
	gtk_widget_show (hbox);
	
	str = g_strdup_printf ("<b>%s</b>", _("Service _type:"));
	label = gtk_label_new_with_mnemonic (str);
	g_free (str);
	
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox),
			    label, FALSE, FALSE, 0);

	dialog->details->type_combo = combo = gtk_combo_box_new_text ();
	/* Keep this in sync with enum */
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo),
				   _("SSH"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo),
				   _("Public FTP"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo),
				   _("FTP (with login)"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo),
				   _("Windows share"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo),
				   _("WebDAV (HTTP)"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo),
				   _("Secure WebDAV (HTTPS)"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo),
				   _("Custom Location"));
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), TYPE_ANON_FTP);
	gtk_widget_show (combo);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
	gtk_box_pack_start (GTK_BOX (hbox),
			    combo, TRUE, TRUE, 0);
	g_signal_connect (combo, "changed",
			  G_CALLBACK (combo_changed_callback),
			  dialog);
	
	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox),
			    hbox, FALSE, TRUE, 0);
	gtk_widget_show (hbox);

	align = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 0, 12, 0);
	gtk_box_pack_start (GTK_BOX (hbox),
			    align, TRUE, TRUE, 0);
	gtk_widget_show (align);
	
	
	dialog->details->required_table = table = gtk_table_new (1, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 12);
	gtk_widget_show (table);
	gtk_container_add (GTK_CONTAINER (align), table);

	//dialog->details->uri_entry = nautilus_location_entry_new ();
	dialog->details->uri_entry = gtk_entry_new();
	dialog->details->server_entry = gtk_entry_new ();
	dialog->details->share_entry = gtk_entry_new ();
	dialog->details->port_entry = gtk_entry_new ();
	g_signal_connect (dialog->details->port_entry, "insert_text", G_CALLBACK (port_insert_text),
			  NULL);
	dialog->details->folder_entry = gtk_entry_new ();
	dialog->details->domain_entry = gtk_entry_new ();
	dialog->details->user_entry = gtk_entry_new ();
	dialog->details->name_entry = gtk_entry_new ();
	/* We need an extra ref so we can remove them from the table */
	g_object_ref (dialog->details->uri_entry);
	g_object_ref (dialog->details->server_entry);
	g_object_ref (dialog->details->share_entry);
	g_object_ref (dialog->details->port_entry);
	g_object_ref (dialog->details->folder_entry);
	g_object_ref (dialog->details->domain_entry);
	g_object_ref (dialog->details->user_entry);
	g_object_ref (dialog->details->name_entry);
	
	setup_for_type (dialog);
	
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       GTK_STOCK_CANCEL,
			       GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       _("_Scan"),
			       RESPONSE_CONNECT);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog),
					 RESPONSE_CONNECT);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (response_callback),
			  dialog);
}

GtkWidget *
baobab_remote_connect_dialog_new (GtkWindow *window, const gchar *location)
{
	BaobabRemoteConnectDialog *conndlg;
	GtkWidget *dialog;
	GnomeVFSURI *uri;

	dialog = gtk_widget_new (BAOBAB_TYPE_REMOTE_CONNECT_DIALOG, NULL);

	if (window) {
		conndlg = BAOBAB_REMOTE_CONNECT_DIALOG(dialog);

		gtk_window_set_screen (GTK_WINDOW (dialog),
				       gtk_window_get_screen (GTK_WINDOW (window)));

		if (location) {
			uri = gnome_vfs_uri_new (location);
			g_return_val_if_fail (uri != NULL, dialog);

			/* ... and if it's a remote URI, then load as the default */
			if (!g_str_equal (gnome_vfs_uri_get_scheme (uri), "file") && 
			    !gnome_vfs_uri_is_local (uri)) {

				gtk_combo_box_set_active (GTK_COMBO_BOX (conndlg->details->type_combo), TYPE_URI);
				gtk_entry_set_text (GTK_ENTRY (conndlg->details->uri_entry), location);
			}
;
			gnome_vfs_uri_unref (uri);
		}
	}

	return dialog;
}

char *
baobab_remote_connect_dialog_get_uri (BaobabRemoteConnectDialog *dlg)
{
	return g_strdup (dlg->details->uri);
}

