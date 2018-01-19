AM_CFLAGS = \
	$(WARNING_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS)

AM_LDFLAGS = \
	$(CODE_COVERAGE_LIBS)

AM_CPPFLAGS = \
	$(CODE_COVERAGE_CPPFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src

libexec_PROGRAMS = \
	flux-imp

flux_imp_LDADD = \
	$(top_builddir)/src/libutil/libutil.la \
	$(top_builddir)/src/libtomlc99/libtomlc99.la

flux_imp_SOURCES = \
	imp.c \
	imp_log.h \
	imp_log.c \
	privsep.c \
	privsep.h \
	impcmd.c \
	impcmd.h \
	version.c \
	testconfig.c

testconfig.c: $(top_builddir)/config/config.h
	@(confdir=`cd $(srcdir) && pwd`/imp.conf.d; \
	  echo "const char *imp_config_pattern = \"$$confdir/*.toml\";" \
	 )> testconfig.c

MOSTLYCLEANFILES = \
	testconfig.c

TESTS = \
	test_imp_log.t \
	test_privsep.t \
	test_impcmd.t

check_PROGRAMS = \
	$(TESTS)

test_ldadd = \
	$(top_builddir)/src/libutil/libutil.la \
	$(top_builddir)/src/libtap/libtap.la

TEST_EXTENSIONS = .t
T_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
	$(top_srcdir)/config/tap-driver.sh

test_imp_log_t_SOURCES =  \
	test/imp_log.c \
	imp_log.c \
	imp_log.h

test_imp_log_t_LDADD = $(test_ldadd)

test_privsep_t_SOURCES = \
	test/privsep.c \
	privsep.c \
	privsep.h \
	sudosim.h \
	sudosim.c \
	imp_log.h \
	imp_log.c

test_privsep_t_LDADD = $(test_ldadd)

test_impcmd_t_SOURCES =  \
	test/impcmd.c \
	version.c \
	impcmd.c \
	impcmd.h

test_impcmd_t_LDADD = $(test_ldadd)
