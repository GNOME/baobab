icondir = ${datadir}/icons/hicolor/48x48/apps
icon_DATA = baobab.png

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

imagesdir = $(datadir)/baobab/pixmaps
images_DATA =			\
	alfa_sort.png		\
	green.png		\
	red.png			\
	search.png		\
	done.png		\
	scan_directory.png	\
	stop.png		\
	baobab.xpm		\
	format_numeric.png	\
	nautilus.png		\
	scan_harddisk.png	\
	white.png		\
	busy.gif		\
	format_percent.png	\
	num_sort.png		\
	searchall.png		\
	shot.png		\
	yellow.png

EXTRA_DIST =		\
	$(images_DATA)	\
	$(icon_DATA)
