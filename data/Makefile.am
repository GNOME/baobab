NULL =

baobabicondir = $(datadir)/icons/hicolor/scalable/actions/
baobabicon_DATA = \
	view-ringschart-symbolic.svg	\
	view-treemap-symbolic.svg	\
	$(NULL)

baobabappdir = $(datadir)/applications
baobabapp_in_files = org.gnome.baobab.desktop.in
baobabapp_DATA = $(baobabapp_in_files:.desktop.in=.desktop)

$(baobabapp_in_files): $(baobabapp_in_files:.desktop.in=.desktop.in.in)
	sed -e "s|\@VERSION\@|@VERSION@|" $< > $@

@INTLTOOL_DESKTOP_RULE@

appdatadir = $(datadir)/appdata
appdata_in_files = baobab.appdata.xml.in
appdata_DATA = $(appdata_in_files:.xml.in=.xml)

dbusservicedir = $(datadir)/dbus-1/services
dbusservice_DATA = org.gnome.baobab.service

org.gnome.baobab.service: Makefile
	$(AM_V_GEN) (echo '[D-BUS Service]'; \
		     echo 'Name=org.gnome.baobab'; \
		     echo 'Exec=${bindir}/baobab --gapplication-service') > $@.tmp && \
		     mv $@.tmp $@

gsettingsschema_in_files = org.gnome.baobab.gschema.xml.in
gsettings_SCHEMAS = $(gsettingsschema_in_files:.xml.in=.xml)
.PRECIOUS: $(gsettings_SCHEMAS)

@INTLTOOL_XML_NOMERGE_RULE@

@GSETTINGS_RULES@

man_MANS = baobab.1

EXTRA_DIST = 				\
	org.gnome.baobab.desktop.in.in	\
	$(gsettingsschema_in_files)	\
	$(man_MANS)			\
	$(baobabicon_DATA)		\
	$(appdata_in_files)		\
	$(NULL)

CLEANFILES =	 			\
	$(baobabapp_in_files)		\
	$(baobabapp_DATA)		\
	$(gsettings_SCHEMAS)		\
	$(appdata_DATA)			\
	$(NULL)

-include $(top_srcdir)/git.mk
