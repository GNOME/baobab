ACLOCAL_AMFLAGS = -I m4 -I libgd ${ACLOCAL_FLAGS}
NULL =

SUBDIRS =  libgd src data pixmaps help po

EXTRA_DIST = COPYING.docs

MAINTAINERCLEANFILES = 		\
	ABOUT-NLS		\
	aclocal.m4		\
	config.guess		\
	config.h.in		\
	config.rpath		\
	config.sub		\
	depcomp			\
	INSTALL			\
	install-sh		\
	ltmain.sh		\
	missing			\
	m4			\
	omf.make		\
	xmldocs.make		\
	po/Makefile.in.in	\
	po/Makevars.template	\
	po/Rules-quot		\
	po/boldquot.sed		\
	po/en@boldquot.header	\
	po/en@quot.header	\
	po/insert-header.sin	\
	po/quot.sed		\
	po/remove-potcdate.sin	\
	$(NULL)

dist-hook:
	@if test -d "$(srcdir)/.git"; \
	then \
		echo Creating ChangeLog && \
		(GIT_DIR=$(top_srcdir)/.git \
		  ./missing --run git log --stat -M -C --name-status  --date=short --no-color) | \
		  fmt --split-only > ChangeLog.tmp \
		&& mv -f ChangeLog.tmp $(top_distdir)/ChangeLog \
		|| ( rm -f ChangeLog.tmp ; \
		  echo Failed to generate ChangeLog >&2 ); \
	else \
		echo A git clone is required to generate a ChangeLog >&2; \
	fi

-include $(top_srcdir)/git.mk
