AC_PREREQ([2.64])

m4_define([libudev_version], [243])

AC_INIT([libudev],[0.6.0.2],[https://github.com/kikadf/libudev-bsd],[libudev-bsd])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([ac-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_TARGET
AC_PROG_CC
AC_USE_SYSTEM_EXTENSIONS

AM_INIT_AUTOMAKE([1.11 foreign no-dist-gzip dist-xz])
AM_SILENT_RULES([yes])

LT_PREREQ([2.2])
LT_INIT([disable-static])

AC_SUBST([PACKAGE_VERSION], [libudev_version])

AC_ARG_ENABLE([gpl],
              AS_HELP_STRING([--enable-gpl], [enable GPL-licensed code]),
              AC_DEFINE([ENABLE_GPL],[1],[Enable GPL-licensed code])
              enable_gpl="yes")
AM_CONDITIONAL(ENABLE_GPL, [test "$enable_gpl" = "yes"])

AC_CHECK_HEADERS([libprocstat.h],
                 [AC_SEARCH_LIBS([procstat_open_sysctl], [procstat])],
                 [],
                 [[@%:@include <sys/param.h>]
                  [@%:@include <sys/queue.h>]
                  [@%:@include <sys/socket.h>]])
AC_CHECK_HEADERS([devinfo.h],
                 [AC_SEARCH_LIBS([devinfo_init], [devinfo])],
                 [],
                 [[@%:@include <devinfo.h>]])
AC_CHECK_HEADERS([dev/evdev/input.h
                  dev/hid/hidraw.h
                  linux/input.h
                  net/if_dl.h
                  sys/tree.h])
AC_CHECK_FUNCS([devname_r pipe2 strchrnul strlcat strlcpy sysctlbyname])

AC_CONFIG_FILES([Makefile
		 libudev.pc
		])
AC_OUTPUT
