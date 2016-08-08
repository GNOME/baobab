NULL =

baobabappdir = $(datadir)/applications
baobabapp_in_files = org.gnome.baobab.desktop.in
baobabapp_DATA = $(baobabapp_in_files:.desktop.in=.desktop)

$(baobabapp_in_files): $(baobabapp_in_files:.desktop.in=.desktop.in.in)
	sed -e "s|\@VERSION\@|@VERSION@|" $< > $@

$(baobabapp_DATA): $(baobabapp_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@


appdatadir = $(datadir)/appdata
appdata_in_files = org.gnome.baobab.appdata.xml.in
appdata_XML = $(appdata_in_files:.appdata.xml.in=.appdata.xml)

$(appdata_XML): $(appdata_in_files)
	$(AM_V_GEN) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

dbusservicedir = $(datadir)/dbus-1/services
dbusservice_DATA = org.gnome.baobab.service

org.gnome.baobab.service: Makefile
	$(AM_V_GEN) (echo '[D-BUS Service]'; \
		     echo 'Name=org.gnome.baobab'; \
		     echo 'Exec=${bindir}/baobab --gapplication-service') > $@.tmp && \
		     mv $@.tmp $@

gsettingsschema_in_files = org.gnome.baobab.gschema.xml.in
gsettings_SCHEMAS = $(gsettingsschema_in_files:.xml.in=.xml)
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
	$(appdata_XML)			\
	org.gnome.baobab.service	\
	$(NULL)

-include $(top_srcdir)/git.mk
