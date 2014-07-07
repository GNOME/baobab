NULL =

AM_CPPFLAGS = \
	-DGETTEXT_PACKAGE=\""$(GETTEXT_PACKAGE)"\"  \
	-DGNOMELOCALEDIR=\""$(localedir)"\" 	    \
	$(NULL)

bin_PROGRAMS = baobab

BUILT_SOURCES = baobab-resources.c

baobab_VALAFLAGS = \
	--target-glib=2.38			\
	--pkg gtk+-3.0				\
	--pkg gio-2.0				\
	--pkg gio-unix-2.0			\
	--gresources=$(srcdir)/resources/baobab.gresource.xml	\
	$(NULL)

VALA_SOURCES = \
	baobab-application.vala		\
	baobab-cellrenderers.vala	\
	baobab-chart.vala		\
	baobab-treemap.vala		\
	baobab-ringschart.vala		\
	baobab-connect-server.vala	\
	baobab-location.vala		\
	baobab-location-list.vala	\
	baobab-scanner.vala		\
	baobab-window.vala		\
	main.vala			\
	$(NULL)

baobab_SOURCES = \
	$(VALA_SOURCES)			\
	config.vapi			\
	$(BUILT_SOURCES)		\
	$(NULL)

resource_files = $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir=$(srcdir)/resources $(srcdir)/resources/baobab.gresource.xml)
baobab-resources.c: $(srcdir)/resources/baobab.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir)/resources --generate-source $<

AM_CFLAGS = \
	$(BAOBAB_CFLAGS)		\
	-Wall				\
	-Wno-unused-but-set-variable	\
	-Wno-unused-variable		\
	$(NULL)

baobab_LDFLAGS = -rdynamic
baobab_LDADD = \
	-lm				\
	$(BAOBAB_LIBS)			\
	$(NULL)

MAINTAINERCLEANFILES =	\
	*.stamp				\
	$(VALA_SOURCES:.vala=.c)	\
	$(BUILT_SOURCES)		\
	$(NULL)

EXTRA_DIST = \
	resources/baobab.gresource.xml	\
	$(resource_files)		\
	$(NULL)

-include $(top_srcdir)/git.mk
