NULL =

INCLUDES = \
	-DPREFIX=\""$(prefix)"\" 				\
	-DGETTEXT_PACKAGE=\""$(GETTEXT_PACKAGE)"\" 		\
	-DSYSCONFDIR=\""$(sysconfdir)"\"			\
	-DLIBDIR=\""$(libdir)"\"				\
	-DDATADIR=\""$(datadir)"\"				\
	-DPKGDATADIR=\""$(datadir)/baobab"\"			\
	-DBAOBAB_PIX_DIR=\""$(datadir)/baobab/pixmaps/"\" 	\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	$(NULL)

bin_PROGRAMS = baobab

BUILT_SOURCES = baobab-resources.c

baobab_VALAFLAGS = \
	--pkg gtk+-3.0		\
	--pkg gio-2.0		\
	--pkg gio-unix-2.0

baobab_SOURCES = \
	fixes.vapi			\
	baobab.vapi			\
	config.vapi			\
	baobab-chart.c			\
	baobab-treemap.c		\
	baobab-ringschart.c		\
	baobab-threaded-scanner.vala	\
	baobab-sync-scanner.vala	\
	baobab-scanner.vala		\
	baobab-cellrenderers.vala	\
	baobab-application.vala		\
	baobab-window.vala		\
	baobab-connect-server.vala	\
	baobab-location.vala		\
	baobab-location-monitor.vala	\
	baobab-location-widget.vala	\
	main.vala			\
	$(BUILT_SOURCES)

baobab-resources.c: baobab.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies baobab.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source baobab.gresource.xml

AM_CFLAGS = \
	$(gtk_CFLAGS)			\
	-Wall				\
	-Wno-unused-but-set-variable	\
	-Wno-unused-variable		\
	$(NULL)

baobab_LDFLAGS = -export-dynamic
baobab_LDADD = \
	-lm				\
	$(gtk_LIBS)			\
	$(NULL)

MAINTAINERCLEANFILES = \
	baobab-application.c		\
	baobab-threaded-scanner.c	\
	baobab-sync-scanner.c		\
	baobab-scanner.c		\
	baobab-cellrenderers.c		\
	baobab-window.c			\
	baobab_vala.stamp		\
	main.c				\
	$(BUILT_SOURCES)

EXTRA_DIST = \
	baobab.gresource.xml \
	baobab-main-window.ui \
	baobab-menu.ui

-include $(top_srcdir)/git.mk
