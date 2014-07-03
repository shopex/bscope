AC_INIT(bscope, 1.0, ecae@shopex.cn)

config_args=$*
AC_SUBST(config_args)

AC_CONFIG_FILES([Makefile include.mk src/Makefile])

AC_CANONICAL_SYSTEM
AC_DEFINE_UNQUOTED(CPU_VENDOR_OS, "$target", CPU_VENDOR_OS)
AC_PREFIX_DEFAULT(/usr/local/)

dnl Programs

AC_PROG_CC
AC_PROG_INSTALL

PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.2.0,[
PKG_CHECK_MODULES(GTHREAD, gthread-2.0 >= 2.2.0,,)
AC_CHECK_LIB(gthread-2.0,g_thread_init,,,$GTHREAD_LIBS)
],)

AC_CHECK_HEADER([pcap.h], [], [
    AC_MSG_ERROR([Could not find the 'pcap.h' header.])])

CFLAGS="${CFLAGS# } -I${with_zeromq}/include"
LDFLAGS="${LDFLAGS} -L${with_zeromq}/lib"

echo ""
echo "CFLAGS='$CFLAGS'"
echo "LDFLAGS='$LDFLAGS'"
echo ""

AC_OUTPUT()

