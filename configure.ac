dnl -*- mode: m4 -*-
AC_PREREQ(2.63)

AC_INIT([baobab],
        [3.22.0],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=baobab],
        [baobab])

AC_CONFIG_HEADERS(config.h)
AC_CONFIG_SRCDIR(src/main.vala)
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 tar-ustar dist-xz no-dist-gzip -Wno-portability foreign])
AM_MAINTAINER_MODE([enable])
AM_SILENT_RULES([yes])

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AM_PROG_VALAC([0.25.1])
PKG_PROG_PKG_CONFIG([0.22])

## don't rerun to this point if we abort
AC_CACHE_SAVE

GLIB_GSETTINGS

# Resources
AC_PATH_PROG(GLIB_COMPILE_RESOURCES, glib-compile-resources)

# Mallard help
YELP_HELP_INIT

APPSTREAM_XML

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.8])
AC_SUBST([GETTEXT_PACKAGE], [baobab])

PKG_CHECK_MODULES(BAOBAB, [
    gtk+-3.0 >= 3.19.1
    glib-2.0 >= 2.39.90
    gio-2.0 >= 2.30.0
])

AC_CONFIG_FILES([
Makefile
data/Makefile
data/org.gnome.baobab.gschema.xml
help/Makefile
pixmaps/Makefile
src/Makefile
po/Makefile.in
])

AC_OUTPUT

echo "
    Baobab $VERSION

    prefix: ${prefix}
    Vala compiler: ${VALAC}
    C compiler: ${CC}

    Now type 'make' to build $PACKAGE
"
