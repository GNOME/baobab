dnl -*- mode: m4 -*-
AC_PREREQ(2.63)

AC_INIT([baobab],[3.3.0],[http://bugzilla.gnome.org/enter_bug.cgi?product=baobab],[baobab])

AC_CONFIG_HEADERS(config.h)
AC_CONFIG_SRCDIR(src/baobab.c)
AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([1.11 tar-ustar dist-xz no-dist-gzip -Wno-portability])
AM_MAINTAINER_MODE([enable])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

IT_PROG_INTLTOOL(0.40.0)

GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",[The name of the gettext domain])

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
PKG_PROG_PKG_CONFIG([0.22])

# no stupid static libraries
AM_DISABLE_STATIC
# enable libtool
AC_PROG_LIBTOOL
# avoid libtool for LTCOMPILE, use it only to link

## don't rerun to this point if we abort
AC_CACHE_SAVE

GLIB_GSETTINGS

GNOME_DOC_INIT

PKG_CHECK_MODULES(BAOBAB, [
    glib-2.0 >= 2.28.0
    gtk+-3.0 >= 3.0.0
    libgtop-2.0
])

AC_CONFIG_FILES([
Makefile
data/Makefile
help/Makefile
pixmaps/Makefile
src/Makefile
po/Makefile.in
])

AC_OUTPUT

echo "
    Baobab $VERSION

    prefix:    ${prefix}
    compiler:  ${CC}

    Now type 'make' to build $PACKAGE
"
