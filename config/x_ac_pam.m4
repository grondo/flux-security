##*****************************************************************************
#  AUTHOR:
#    Mark A. Grondona <mgrondona@llnl.gov>
#
#  SYNOPSIS:
#    X_AC_PAM
#
#  DESCRIPTION:
#    Test if PAM is requested and available.
#    Sets PAM_LIBS and PAM_CPPFLAGS if requested and available
##*****************************************************************************
AC_DEFUN([X_AC_PAM], [
  AC_ARG_ENABLE([pam],
    AS_HELP_STRING([--enable-pam], [Enable PAM support for IMP exec]))
    AS_IF([test "x$enable_pam" = "xyes"], [
        AC_CHECK_LIB([pam], [pam_start], [ac_have_pam=yes; PAM_LIBS="-lpam"])
	AC_CHECK_LIB([pam_misc],
                     [misc_conv],
                     [ac_have_pam_misc=yes; PAM_LIBS="$PAM_LIBS -lpam_misc"])
	AC_SUBST(PAM_LIBS)
	if test "x$ac_have_pam" = "xyes" -a "x$ac_have_pam_misc" = "xyes"; then
	    AC_DEFINE([HAVE_PAM], [1], [Enable PAM support for flux-imp exec])
        else
            AC_MSG_ERROR([unable to locate PAM libraries]")
        fi
    ])
  AM_CONDITIONAL(HAVE_PAM, [test "x$enable_pam" = "xyes"])
])

