NULL =

AM_CPPFLAGS = \
	-DGETTEXT_PACKAGE=\""$(GETTEXT_PACKAGE)"\" 		\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	$(NULL)

bin_PROGRAMS = baobab

BUILT_SOURCES = baobab-resources.c

baobab_VALAFLAGS = \
	--vapidir=$(top_srcdir)/libgd	\
	--pkg gtk+-3.0		\
	--pkg clutter-gtk-1.0	\
	--pkg gio-2.0		\
	--pkg gio-unix-2.0	\
	--pkg gd-1.0

noinst_HEADERS = \
	baobab-chart.h			\
	baobab-treemap.h		\
	baobab-ringschart.h		\
	egg-list-box.h			\
	cc-notebook.h			\
	$(NULL)

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

baobab_SOURCES = \
	$(VALA_SOURCES)		\
	fixes.vapi			\
	baobab.vapi			\
	config.vapi			\
	egglistbox.vapi			\
	ccnotebook.vapi			\
	baobab-chart.c			\
	baobab-treemap.c		\
	baobab-ringschart.c		\
	egg-list-box.c			\
	cc-notebook.c			\
	$(BUILT_SOURCES)

baobab-resources.c: baobab.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies baobab.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source baobab.gresource.xml

AM_CFLAGS = \
	$(BAOBAB_CFLAGS)		\
	-I$(top_srcdir)/libgd		\
	-Wall				\
	-Wno-unused-but-set-variable	\
	-Wno-unused-variable		\
	$(NULL)

baobab_LDFLAGS = -export-dynamic -rpath $(libdir)
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
