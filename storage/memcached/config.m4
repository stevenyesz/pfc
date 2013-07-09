dnl config.m4 for extension pfc_memcached

PHP_ARG_ENABLE(pfc-memcached, whether to enable the memcached storage module for pfc,
[  --enable-pfc-memcached     Enable pfc memcached storage module])

PHP_ARG_WITH(libmemcached-dir,  for libmemcached,
[  --with-libmemcached-dir[=DIR]   Set the path to libmemcached install prefix.], yes)

if test "$PHP_PFC_MEMCACHED" != "no"; then
  PHP_NEW_EXTENSION(pfc_memcached, pfc_memcached.c, $ext_shared)

  PHP_ADD_EXTENSION_DEP(pfc_memcached, pfc)
  PHP_ADD_EXTENSION_DEP(pfc_memcached, memcached, true)

  if test "$PHP_LIBMEMCACHED_DIR" != "no" && test "$PHP_LIBMEMCACHED_DIR" != "yes"; then
    if test -r "$PHP_LIBMEMCACHED_DIR/include/libmemcached/memcached.h"; then
      PHP_LIBMEMCACHED_DIR="$PHP_LIBMEMCACHED_DIR"
    else
      AC_MSG_ERROR([Can't find libmemcached headers under "$PHP_LIBMEMCACHED_DIR"])
    fi
  else
    PHP_LIBMEMCACHED_DIR="no"
    for i in /usr /usr/local; do
      if test -r "$i/include/libmemcached/memcached.h"; then
        PHP_LIBMEMCACHED_DIR=$i
        break
      fi
    done
  fi

  AC_MSG_CHECKING([for libmemcached location])
  if test "$PHP_LIBMEMCACHED_DIR" = "no"; then
    AC_MSG_RESULT([libmemcached not found. The memcached storage module for pfc will only be usable with the memcached extension via pfc_memcached_set_connection. Use --with-libmemcached-dir=<DIR> to specify the prefix where libmemcached headers and library are located])
  else
    AC_MSG_RESULT([$PHP_LIBMEMCACHED_DIR])
    PHP_ADD_INCLUDE($PHP_LIBMEMCACHED_DIR/include)
    PHP_ADD_LIBRARY_WITH_PATH(memcached, $PHP_LIBMEMCACHED_DIR/$PHP_LIBDIR, MEMCACHED_SHARED_LIBADD)
    PHP_SUBST(MEMCACHED_SHARED_LIBADD)
    PHP_DEF_HAVE(libmemcached)
  fi
fi
