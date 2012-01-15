uidir = $(datadir)/baobab
ui_DATA = \
	baobab-main-window.ui \
	baobab-menu.ui \
	baobab-preferences-dialog.ui

baobabappdir	   = $(datadir)/applications
baobabapp_in_files = baobab.desktop.in
baobabapp_DATA     = $(baobabapp_in_files:.desktop.in=.desktop)

$(baobabapp_in_files): $(baobabapp_in_files:.desktop.in=.desktop.in.in)
	sed -e "s|\@VERSION\@|@VERSION@|" $< > $@

@INTLTOOL_DESKTOP_RULE@

gsettingsschema_in_files = org.gnome.baobab.gschema.xml.in
gsettings_SCHEMAS = $(gsettingsschema_in_files:.xml.in=.xml)
.PRECIOUS: $(gsettings_SCHEMAS)

@INTLTOOL_XML_NOMERGE_RULE@

@GSETTINGS_RULES@

man_MANS = baobab.1

EXTRA_DIST = 				\
	$(ui_DATA)			\
	baobab.desktop.in.in		\
	$(gsettingsschema_in_files)	\
	$(man_MANS)

CLEANFILES =	 			\
	$(baobabapp_in_files)		\
	$(baobabapp_DATA)		\
	$(gsettings_SCHEMAS)

-include $(top_srcdir)/git.mk
