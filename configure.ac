#
# Prologue
#

# XXX: replace hard-coded version with git-describe 
AC_INIT([flux-security],
         m4_esyscmd([git describe --always | awk '/.*/ {sub(/^v/, ""); printf "%s",$1; exit}']))

AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config])
AC_CONFIG_SRCDIR([NEWS])
AC_CANONICAL_SYSTEM

#
#  Automake support:
#
AM_INIT_AUTOMAKE([subdir-objects tar-pax])
AM_SILENT_RULES([yes])
AM_CONFIG_HEADER([config/config.h])
AM_MAINTAINER_MODE([enable])

AC_DEFINE([_GNU_SOURCE], 1,
          [Define _GNU_SOURCE so that we get all necessary prototypes])

#
# Generate project versions from PACKAGE_VERSION (set from git describe above)
#
AX_SPLIT_VERSION
AX_POINT_VERSION=$(echo $AX_POINT_VERSION | $SED 's/-.*$//')
AC_SUBST([AX_MAJOR_VERSION])
AC_SUBST([AX_MINOR_VERSION])
AC_SUBST([AX_POINT_VERSION])

#
# Library version
# https://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
LIBFLUX_SECURITY_CURRENT=1
LIBFLUX_SECURITY_REVISION=0
LIBFLUX_SECURITY_AGE=0
AC_SUBST(
  [LIBFLUX_SECURITY_VERSION_INFO],
  [$LIBFLUX_SECURITY_CURRENT:$LIBFLUX_SECURITY_REVISION:$LIBFLUX_SECURITY_AGE]
)

#
#  Initialize pkg-config for PKG_CHECK_MODULES to avoid conditional issues
#
PKG_PROG_PKG_CONFIG

#
#  Checks for programs
#
AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_PROG_AWK
AC_ARG_ENABLE([docs],
              AS_HELP_STRING([--disable-docs], [disable building docs]))
AS_IF([test "x$enable_docs" != "xno"], [
  AC_CHECK_PROG(A2X,[a2x],[a2x])
])
AM_CONDITIONAL([ENABLE_DOCS], [test "$A2X" = "a2x" ])
AC_CHECK_PROG(ASPELL,[aspell],[aspell])

#
#  If Clang or GNU CC, add warning/hardening flags:
#
AX_COMPILER_VENDOR
AS_CASE($ax_cv_c_compiler_vendor,
  [clang | gnu], [
    WARNING_CFLAGS="-Wall -Werror -Wno-strict-aliasing -Wextra"
    AC_SUBST([WARNING_CFLAGS])
  ]
)

#
#  Libotool support
#
LT_INIT

#
#  Checks for header files
#
AC_HEADER_STDC

#
#  Checks for packages
#
PKG_CHECK_MODULES([SODIUM], [libsodium >= 1.0.14], [], [])
PKG_CHECK_MODULES([JANSSON], [jansson], [], [])
PKG_CHECK_MODULES([LIBUUID], [uuid], [], [])
PKG_CHECK_MODULES([MUNGE], [munge], [], [])

#
#  Other checks
#
AX_CODE_COVERAGE

AC_PKGCONFIG

#
# Project directories
#
AS_VAR_SET(fluxlibdir, $libdir/flux)
AC_SUBST(fluxlibdir)

AS_VAR_SET(fluxsecuritycfdir, $sysconfdir/flux/security/conf.d)
AC_SUBST(fluxsecuritycfdir)

AS_VAR_SET(fluxsecurityincludedir, $includedir/flux/security)
AC_SUBST(fluxsecurityincludedir)

#
#  Epilogue
#
AC_CONFIG_FILES( \
  Makefile \
  t/Makefile \
  src/Makefile \
  src/lib/Makefile \
  src/lib/flux-security.pc \
  src/lib/version.h \
  src/libtap/Makefile \
  src/libtomlc99/Makefile \
  src/libutil/Makefile \
  src/libca/Makefile \
  src/imp/Makefile \
  etc/Makefile \
)

AC_OUTPUT
