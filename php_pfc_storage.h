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

#ifndef _PHP_PFC_STORAGE_H_
#define _PHP_PFC_STORAGE_H_

#include "php_pfc.h"

/* Storage function arguments */
#define PFC_GET_ARGS	char *key, zval **value TSRMLS_DC
#define PFC_SET_ARGS	char *key, zval *value TSRMLS_DC

/* {{{ typedef struct _php_pfc_storage_module */
typedef struct _pfc_storage_module {
	char *name;
	int (*get)(PFC_GET_ARGS);
	int (*set)(PFC_SET_ARGS);
} pfc_storage_module;
/* }}} */

/* Storage function signatures */
#define PFC_GET_FUNC(mod_name)	int pfc_storage_get_##mod_name(PFC_GET_ARGS)
#define PFC_SET_FUNC(mod_name)	int pfc_storage_set_##mod_name(PFC_SET_ARGS)

/* Define storage functions */
#define PFC_STORAGE_FUNCS(mod_name) \
	PFC_GET_FUNC(mod_name); \
	PFC_SET_FUNC(mod_name); 

/* Define the func ptrs for the specific module */
#define PFC_STORAGE_MODULE(mod_name) \
	#mod_name, pfc_storage_get_##mod_name, pfc_storage_set_##mod_name
	
/* Register the storage module. called from MINIT in module */
#define PFC_STORAGE_REGISTER(mod_name) \
	pfc_register_storage_module(pfc_storage_module_##mod_name##_ptr)

#endif
