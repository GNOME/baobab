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

baobab_VALAFLAGS = --pkg gtk+-3.0
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
	main.vala

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
	main.c

-include $(top_srcdir)/git.mk
