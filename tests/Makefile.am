SUBDIRS = src functional overhead

EXTRA_DIST = cross-compile-sizeof event

# Do not stop testing if tests fail within test subdirectories
MAKEFLAGS = -k

uninstall-hook:
	rmdir --ignore-fail-on-non-empty $(DESTDIR)$(datadir)/tests

