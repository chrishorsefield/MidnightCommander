AM_CFLAGS = -I$(top_srcdir)/lib/vfs $(GLIB_CFLAGS) -I$(top_srcdir) @CHECK_CFLAGS@

EXTRA_DIST = test-data.xml.in

AM_LDFLAGS = @TESTS_LDFLAGS@

LIBS=@CHECK_LIBS@  \
	$(top_builddir)/src/libinternal.la \
	$(top_builddir)/lib/libmc.la

if ENABLE_VFS_SMB
# this is a hack for linking with own samba library in simple way
LIBS += $(top_builddir)/src/vfs/smbfs/helpers/libsamba.a
endif


TESTS = \
	xmltag

check_PROGRAMS = $(TESTS)

xmltag_SOURCES = \
	xmltag.c
