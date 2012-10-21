/* -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
namespace Baobab {

    public class CrossFadeNotebook : Gtk.Grid, Gtk.Buildable {

        Gtk.Widget current_page = null;
        GtkClutter.Embed embed;

        construct {
            embed = new GtkClutter.Embed ();
            add (embed);
            embed.show ();

            vexpand = false;
            hexpand = false;

            GtkClutter.embed_set_use_layout_size (embed, true);
            var stage = embed.get_stage ();
            stage.set_layout_manager (new Clutter.BinLayout (Clutter.BinAlignment.FILL, Clutter.BinAlignment.FILL));
        }

        public void add_page (Gtk.Widget widget) {
            var actor = new GtkClutter.Actor.with_contents (widget);
            widget.set_data ("clutter_actor", actor);
            embed.get_stage ().add_child (actor);

            select_page (widget);
        }

        public void select_page (Gtk.Widget widget) {
            if (current_page == widget) {
                return;
            }

            if (current_page != null) {
                var cur_actor = current_page.get_data<Clutter.Actor> ("clutter_actor");
                cur_actor.reactive = false;
                cur_actor.save_easing_state ();
                cur_actor.set_easing_duration (500);
                cur_actor.opacity = 0;
                cur_actor.restore_easing_state ();
            }

            var actor = widget.get_data<Clutter.Actor> ("clutter_actor");
            actor.reactive = true;
            actor.save_easing_state ();
            actor.set_easing_duration (500);
            actor.opacity = 0xff;
            actor.restore_easing_state ();

            current_page = widget;
        }

        // GtkBuildable interface

        public void add_child (Gtk.Builder builder, Object child, string? type) {
            add_page (child as Gtk.Widget);
        }
    }
}
