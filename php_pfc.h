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
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_PFC_H
#define PHP_PFC_H

#define PFC_EXTVER "0.0.1-dev"

#include "php_pfc_storage.h"
#include <unistd.h>

extern zend_module_entry pfc_module_entry;
#define phpext_pfc_ptr &pfc_module_entry

#ifdef PHP_WIN32
#	define PHP_PFC_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_PFC_API __attribute__ ((visibility("default")))
#else
#	define PHP_PFC_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

#if !defined(uint64)
typedef unsigned long long uint64;
#endif
#if !defined(uint32)
typedef unsigned int uint32;
#endif
#if !defined(uint8)
typedef unsigned char uint8;
#endif

PHP_MINIT_FUNCTION(pfc);
PHP_MSHUTDOWN_FUNCTION(pfc);
PHP_RINIT_FUNCTION(pfc);
PHP_RSHUTDOWN_FUNCTION(pfc);
PHP_MINFO_FUNCTION(pfc);

PHP_FUNCTION(pfc);
PHP_FUNCTION(pfc_call);
PHP_FUNCTION(pfc_stats);

PHPAPI int pfc_register_storage_module(pfc_storage_module *ptr);

typedef struct {
	HashTable *function_table;
	zend_function function;
} pfc_internal_function;



/* pfc function profile maintains a stack of entries being profiled. The memory for the entry
 * is passed by the layer that invokes BEGIN_PROFILING(), e.g. the hp_execute()
 * function. Often, this is just C-stack memory.
 *
 * This structure is a convenient place to track start time of a particular
 * profile operation, recursion depth, and the name of the function being
 * profiled. */
typedef struct mhp_entry_t {
  char                   *name_hprof;                       /* function name */
  char                   *filename;             /* php script file name */
  zend_uint            line;                    /* line number of function in script file*/
  int                     rlvl_hprof;        /* recursion level for function */
  uint64                  tsc_start;         /* start value for TSC counter  */
  struct mhp_entry_t      *prev_hprof;    /* ptr to prev entry being profiled */
  uint8                   hash_code;     /* hash_code for the function name  */
} mhp_entry_t;


int pfc_fix_internal_functions(pfc_internal_function *fe TSRMLS_DC);
int pfc_remove_handler_functions(zend_function *fe TSRMLS_DC);
int pfc_load_functions(void);
int function_pfcd(char *class_name,char *fname);
int pfc_profiler_init(char *script_name TSRMLS_DC);
void pfc_profiler_deinit(TSRMLS_D);

int pfc_function_profile_init(TSRMLS_DC);
int pfc_function_profile_deinit(TSRMLS_DC);

#define PFC_KEY_PREFIX "_memoizd"
#define PFC_FUNC_SUFFIX "$memoizd"

ZEND_BEGIN_MODULE_GLOBALS(pfc)
	HashTable *internal_functions;
	char *cache_namespace;
	char *storage_module;
	char *data_dir;
	zend_bool profiler_enable;
	char *config_file;
	long default_ttl;
	zend_bool profiler_enabled;
	FILE *profiler_file;
	FILE *sclog_file;
	int function_profile_ever_enabled;
	uint64 start;//start timestamp of a request
       zval *stats_count;
	char buf_singlelog[1024];   
	uint64 buf_singelog_len;
	mhp_entry_t *function_profile_entries;
	mhp_entry_t *entry_free_list;
	 /* counter table indexed by hash value of function names. */
 	uint8  func_hash_counters[256];
	/* This array is used to store cpu frequencies for all available logical
	* cpus.  For now, we assume the cpu frequencies will not change for power
	* saving or other reasons. If we need to worry about that in the future, we
	* can use a periodical timer to re-calculate this arrary every once in a
	* while (for example, every 1 or 5 seconds). */
	double *cpu_frequencies; 
	uint32 cur_cpu_id;
	/* The number of logical CPUs this machine has. */
	uint32 cpu_num;
ZEND_END_MODULE_GLOBALS(pfc)

#ifdef ZTS
#define PFC_G(v) TSRMG(pfc_globals_id, zend_pfc_globals *, v)
#else
#define PFC_G(v) (pfc_globals.v)
#endif

#if ZEND_MODULE_API_NO >= 20100525
#define PFC_RETURNS_REFERENCE(fe) (fe->common.fn_flags & ZEND_ACC_RETURN_REFERENCE)
#else
#define PFC_RETURNS_REFERENCE(fe) (fe->common.return_reference)
#endif


#endif	/* PHP_PFC_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
