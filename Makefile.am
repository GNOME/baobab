ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
NULL =

SUBDIRS =  src data pixmaps help po

EXTRA_DIST = \
	COPYING.docs

MAINTAINERCLEANFILES = \
        $(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL) \
        $(GITIGNORE_MAINTAINERCLEANFILES_MAKEFILE_IN) \
	ABOUT-NLS		\
	config.rpath		\
	m4			\
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
