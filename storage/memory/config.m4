dnl config.m4 for extension pfc_memory

PHP_ARG_ENABLE(pfc-memory, whether to enable the memory storage module for pfc,
[  --enable-pfc-memory     Enable pfc memory storage module])

if test "$PHP_PFC_MEMORY" != "no"; then
  PHP_NEW_EXTENSION(pfc_memory, pfc_memory.c, $ext_shared)
  PHP_ADD_EXTENSION_DEP(pfc_memory, pfc)
fi
