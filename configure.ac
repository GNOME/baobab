dnl -*- mode: m4 -*-
AC_PREREQ(2.63)

AC_INIT([baobab],
        [3.5.90],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=baobab],
        [baobab])

AC_CONFIG_HEADERS(config.h)
AC_CONFIG_SRCDIR(src/main.vala)
AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([1.11 tar-ustar dist-xz no-dist-gzip -Wno-portability])
AM_MAINTAINER_MODE([enable])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# i18n stuff
IT_PROG_INTLTOOL(0.40.0)

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.17])

GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",[The name of the gettext domain])

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AM_PROG_VALAC([0.17.4])
PKG_PROG_PKG_CONFIG([0.22])

## don't rerun to this point if we abort
AC_CACHE_SAVE

GLIB_GSETTINGS

# Resources
AC_PATH_PROG(GLIB_COMPILE_RESOURCES, glib-compile-resources)

# Mallard help
YELP_HELP_INIT

PKG_CHECK_MODULES(gtk, gtk+-3.0 >= 3.5.9)
PKG_CHECK_MODULES(gio, gio-2.0 >= 2.30.0)

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

    prefix: ${prefix}
    Vala compiler: ${VALAC}
    C compiler: ${CC}

    Now type 'make' to build $PACKAGE
"
