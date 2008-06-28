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
	baobab-cell-renderer-progress.c \
	baobab-cell-renderer-progress.h \
	baobab-ringschart.c \
	baobab-ringschart.h \
	baobab-scan.c \
	baobab-scan.h \
	baobab-treeview.c \
	baobab-treeview.h \
	baobab-utils.c \
	baobab-utils.h \
	callbacks.c \
	callbacks.h \
	baobab-prefs.c \
	baobab-prefs.h \
	baobab-remote-connect-dialog.c \
	baobab-remote-connect-dialog.h \
	baobab-mount-operation.c \
	baobab-mount-operation.h \
	gedit-spinner.c \
	gedit-spinner.h \
	baobab-chart.c \
	baobab-chart.h \
	baobab-treemap.c \
	baobab-treemap.h

baobab_CFLAGS = 			\
	$(GNOME_UTILS_CFLAGS)		\
	$(LIBGLADE_CFLAGS)		\
	$(LIBGTOP_CFLAGS)		\
	$(NULL)

baobab_LDFLAGS = -export-dynamic
baobab_LDADD = 				\
	$(GNOME_UTILS_LIBS)		\
	$(LIBGLADE_LIBS)		\
	$(LIBGTOP_LIBS)			\
	$(NULL)
