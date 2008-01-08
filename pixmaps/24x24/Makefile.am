NULL =

size = 24x24

iconsdir = $(datadir)/icons/hicolor/$(size)/apps

icons_DATA =					\
	baobab.png				\
	$(NULL)

icons_SOURCE =					\
	$(NULL)

EXTRA_DIST =		\
	$(icons_DATA)	\
	$(icons_SOURCE)

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache
update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi
