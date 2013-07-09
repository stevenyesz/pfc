dnl $Id$
dnl config.m4 for extension pfc

PHP_ARG_ENABLE(pfc, whether to enable pfc support,
[  --enable-pfc           Enable pfc support])

if test "$PHP_PFC" != "no"; then
  PHP_NEW_EXTENSION(pfc, pfc.c, $ext_shared)
  PHP_INSTALL_HEADERS([ext/pfc], [php_pfc.h php_pfc_storage.h])
fi
