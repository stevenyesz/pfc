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

#ifndef PHP_PFC_APC_H
#define PHP_PFC_APC_H

#define PFC_APC_EXTVER "0.0.1-dev"

#include "php.h"
#include "php_ini.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef ZTS
# include "TSRM.h"
#endif

#include "ext/pfc/php_pfc.h"
#include "ext/pfc/php_pfc_storage.h"

/* Hook into pfc module */
extern pfc_storage_module pfc_storage_module_apc;
#define pfc_storage_module_apc_ptr &pfc_storage_module_apc
ZEND_EXTERN_MODULE_GLOBALS(pfc);

/* Normal PHP entry */
extern zend_module_entry pfc_apc_module_entry;
#define phpext_pfc_storage_apc_ptr &pfc_apc_module_entry

PFC_STORAGE_FUNCS(apc);

#endif

