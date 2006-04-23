# FIXME - this is an evil hack, but glade doesn't find the pixmaps otherwise
gladedir         = $(datadir)/baobab/pixmaps
glade_DATA       = baobab.glade

baobabappdir	   = $(datadir)/applications
baobabapp_in_files = baobab.desktop.in
baobabapp_DATA     = $(baobabapp_in_files:.desktop.in=.desktop)

$(baobabapp_in_files): $(baobabapp_in_files:.desktop.in=.desktop.in.in)
	sed -e "s|\@VERSION\@|@VERSION@|" $< > $@

@INTLTOOL_DESKTOP_RULE@

schemasdir	 = $(GCONF_SCHEMA_FILE_DIR)
schemas_in_files = baobab.schemas.in
schemas_DATA 	 = $(schemas_in_files:.schemas.in=.schemas)
@INTLTOOL_SCHEMAS_RULE@

man_MANS = baobab.1

if GCONF_SCHEMAS_INSTALL
install-data-local:
	if test -z "$(DESTDIR)" ; then \
		GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(schemas_DATA) ; \
	fi
else
install-data-local:
endif

EXTRA_DIST = 				\
	$(glade_DATA)			\
	baobab.desktop.in.in		\
	$(schemas_in_files)		\
	$(man_MANS)

CLEANFILES =	 			\
	$(baobabapp_in_files)		\
	$(baobabapp_DATA)		\
	$(schemas_DATA)
