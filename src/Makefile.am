## Process this file with automake to produce Makefile.in

NULL =

INCLUDES = \
	-DPREFIX=\""$(prefix)"\" 				\
	-DSYSCONFDIR=\""$(sysconfdir)"\"			\
	-DLIBDIR=\""$(libdir)"\"				\
	-DDATADIR=\""$(datadir)"\"				\
	-DPKGDATADIR=\""$(datadir)/baobab"\"			\
	-DUIDATADIR=\""$(datadir)/gnome-2.0/ui"\"		\
	-DBAOBAB_DATA_DIR=\""$(datadir)"\" 			\
	-DBAOBAB_PIX_DIR=\""$(datadir)/baobab/pixmaps/"\" 	\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	$(NULL)

bin_PROGRAMS = baobab

baobab_SOURCES = \
	baobab.c \
	baobab.h \
	baobab-graphwin.c \
	baobab-graphwin.h \
	baobab-ringschart.c \
	baobab-ringschart.h \
	baobab-scan.c \
	baobab-scan.h \
	baobab-treeview.c \
	baobab-treeview.h \
	baobab-utils.c \
	baobab-utils.h \
	baobab-spinner.c \
	baobab-spinner.h \
	callbacks.c \
	callbacks.h \
	baobab-prefs.c \
	baobab-prefs.h \
	baobab-tree-map.c \
	baobab-tree-map.h \
	baobab-remote-connect-dialog.c \
	baobab-remote-connect-dialog.h

baobab_CFLAGS = 			\
	$(GNOME_UTILS_CFLAGS)		\
	$(GNOMEVFS_CFLAGS)		\
	$(LIBGLADE_CFLAGS)		\
	$(GNOMECANVAS_CFLAGS)		\
	$(LIBGTOP_CFLAGS)		\
	$(NULL)

baobab_LDFLAGS = -export-dynamic
baobab_LDADD = 				\
	$(GNOME_UTILS_LIBS)		\
	$(GNOMEVFS_LIBS)		\
	$(LIBGLADE_LIBS)		\
	$(GNOMECANVAS_LIBS)		\
	$(LIBGTOP_LIBS)			\
	$(NULL)
