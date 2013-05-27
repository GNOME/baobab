NULL =

AM_CPPFLAGS = \
	-DGETTEXT_PACKAGE=\""$(GETTEXT_PACKAGE)"\" 		\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	$(NULL)

bin_PROGRAMS = baobab

BUILT_SOURCES = baobab-resources.c

baobab_VALAFLAGS = \
	--vapidir=$(top_builddir)/egg-list-box	\
	--vapidir=$(top_builddir)/libgd		\
	--pkg gtk+-3.0				\
	--pkg gio-2.0				\
	--pkg gio-unix-2.0			\
	--pkg gd-1.0

noinst_HEADERS = \
	baobab-chart.h			\
	baobab-treemap.h		\
	baobab-ringschart.h

VALA_SOURCES = \
	baobab-application.vala		\
	baobab-cellrenderers.vala	\
	baobab-connect-server.vala	\
	baobab-location.vala		\
	baobab-location-list.vala	\
	baobab-location-widget.vala	\
	baobab-scanner.vala		\
	baobab-window.vala		\
	main.vala			\
	$(NULL)

egg_list_box_sources = \
	$(top_builddir)/egg-list-box/egglistbox.vapi	\
	$(top_builddir)/egg-list-box/egg-list-box.c	\
	$(top_builddir)/egg-list-box/egg-list-box-accessible.c

baobab_SOURCES = \
	$(VALA_SOURCES)			\
	fixes.vapi			\
	baobab.vapi			\
	config.vapi			\
	baobab-chart.c			\
	baobab-treemap.c		\
	baobab-ringschart.c		\
	$(egg_list_box_sources)		\
	$(BUILT_SOURCES)

baobab-resources.c: baobab.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir=$(srcdir) $(srcdir)/baobab.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<

AM_CFLAGS = \
	$(BAOBAB_CFLAGS)		\
	-I$(top_srcdir)/egg-list-box	\
	-I$(top_srcdir)/libgd		\
	-Wall				\
	-Wno-unused-but-set-variable	\
	-Wno-unused-variable		\
	$(NULL)

baobab_LDFLAGS = -export-dynamic
baobab_LDADD = \
	-lm				\
	$(BAOBAB_LIBS)			\
	$(top_builddir)/libgd/libgd.la \
	$(NULL)

MAINTAINERCLEANFILES =	\
	*.stamp				\
	$(VALA_SOURCES:.vala=.c)	\
	$(BUILT_SOURCES)

EXTRA_DIST = \
	baobab.gresource.xml \
	baobab-main-window.ui \
	baobab-menu.ui

-include $(top_srcdir)/git.mk
