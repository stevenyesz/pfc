/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2011 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Arpad Ray <arpad@php.net>                                    |
  +----------------------------------------------------------------------+
*/

#include "php_pfc_memory.h"
#include "ext/standard/info.h"

pfc_storage_module pfc_storage_module_memory = {
	PFC_STORAGE_MODULE(memory)
};

/* {{{ Globals */
ZEND_DECLARE_MODULE_GLOBALS(pfc_memory);

static PHP_GINIT_FUNCTION(pfc_memory) 
{
	pfc_memory_globals->store = NULL;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION(pfc_memory) */
PHP_RINIT_FUNCTION(pfc_memory) 
{
	PFC_STORAGE_REGISTER(memory);
	ALLOC_HASHTABLE(PFC_MEMORY_G(store));
	return zend_hash_init(PFC_MEMORY_G(store), 4, NULL, (dtor_func_t)ZVAL_PTR_DTOR, 0);
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION(pfc_memory) */
PHP_RSHUTDOWN_FUNCTION(pfc_memory) 
{
	zend_hash_destroy(PFC_MEMORY_G(store));
	FREE_HASHTABLE(PFC_MEMORY_G(store));
	return SUCCESS;
}
/* }}} */

/* {{{ PFC_GET_FUNC(memory)
*/
PFC_GET_FUNC(memory)
{
	zval **entry;
	if (zend_hash_find(PFC_MEMORY_G(store), key, strlen(key) + 1, (void **)&entry) == SUCCESS) {
		zval_ptr_dtor(value);
		*value = *entry;
		Z_ADDREF_PP(value);
		return SUCCESS;
	}

	return FAILURE;
}
/* }}} */

/* {{{ PFC_SET_FUNC(memory) */
PFC_SET_FUNC(memory)
{
	Z_ADDREF_P(value);
	return zend_hash_update(PFC_MEMORY_G(store), key, strlen(key) + 1, (void *)&value, sizeof(zval *), NULL);
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION(pfc_memory) */
PHP_MINFO_FUNCTION(pfc_memory)
{	
	php_info_print_table_start();
	php_info_print_table_row(2, "pfc_memory storage", "enabled");
	php_info_print_table_row(2, "pfc_memory version", PFC_MEMORY_EXTVER);
	php_info_print_table_end();
}
/* }}} */

static zend_function_entry pfc_memory_functions[] = {
	{NULL, NULL, NULL}
};

zend_module_entry pfc_memory_module_entry = {
	STANDARD_MODULE_HEADER,
	"pfc_memory",
	pfc_memory_functions,
	NULL,
	NULL,
	PHP_RINIT(pfc_memory),
	PHP_RSHUTDOWN(pfc_memory),
	PHP_MINFO(pfc_memory),
	PFC_MEMORY_EXTVER,
	PHP_MODULE_GLOBALS(pfc_memory),
	PHP_GINIT(pfc_memory),
	NULL,
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_PFC_MEMORY
ZEND_GET_MODULE(pfc_memory)
#endif
