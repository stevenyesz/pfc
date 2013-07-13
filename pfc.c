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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "php.h"
#include <sys/time.h>
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/php_var.h"
#include "ext/standard/php_smart_str.h"
#include "ext/standard/md5.h"
#include "php_pfc.h"

/* {{{ pfc_functions[]
 */
const zend_function_entry pfc_functions[] = {
	PHP_FE(pfc,	NULL)
	PHP_FE(pfc_call, NULL)
	PHP_FE(pfc_stats, NULL)
	{NULL, NULL, NULL}
};
/* }}} */

/* {{{ PHP_GINIT_FUNCTION(pfc) */
ZEND_DECLARE_MODULE_GLOBALS(pfc);
static PHP_GINIT_FUNCTION(pfc)
{
	pfc_globals->internal_functions = NULL;
	pfc_globals->cache_namespace = NULL;
	pfc_globals->storage_module = NULL;
	pfc_globals->default_ttl = 0;
}
/* }}} */

/* {{{ pfc_module_entry
 */
zend_module_entry pfc_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"pfc",
	pfc_functions,
	PHP_MINIT(pfc),
	PHP_MSHUTDOWN(pfc),
	PHP_RINIT(pfc),
	PHP_RSHUTDOWN(pfc),
	PHP_MINFO(pfc),
	PFC_EXTVER,
	PHP_MODULE_GLOBALS(pfc),
	PHP_GINIT(pfc),
	NULL,
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */

#ifdef COMPILE_DL_PFC
ZEND_GET_MODULE(pfc)
#endif

/* {{{ Storage modules
 */

#define PFC_MAX_MODULES 10
/* Size of a temp scratch buffer            */
#define SCRATCH_BUF_LEN            512

#define ROOT_SYMBOL                "main()"
/**
 * ****************************
 * STATIC FUNCTION DECLARATIONS
 * ****************************
 */
static pfc_storage_module *pfc_storage_modules[PFC_MAX_MODULES + 1] = {};

static zend_op_array* (*old_compile_file)(zend_file_handle* file_handle, int type TSRMLS_DC);
void (*pfc_old_execute) (zend_op_array *ops TSRMLS_DC);


static zend_op_array* pfc_compile_file(zend_file_handle*, int TSRMLS_DC);
void mhp_execute (zend_op_array *ops TSRMLS_DC); 
static mhp_entry_t *pfc_fast_alloc_mhp_entry();
static void pfc_free_the_free_list();
static mhp_entry_t *pfc_fast_alloc_hprof_entry();
static void pfc_fast_free_hprof_entry(mhp_entry_t *p);


void pfc_mode_common_beginfn(mhp_entry_t **entries,
                            mhp_entry_t  *current  TSRMLS_DC);
void pfc_mode_common_endfn(mhp_entry_t **entries, mhp_entry_t *current TSRMLS_DC);
void pfc_mode_hier_beginfn_cb(mhp_entry_t **entries,
                             mhp_entry_t  *current  TSRMLS_DC);
void pfc_mode_hier_endfn_cb(mhp_entry_t **entries  TSRMLS_DC);
size_t pfc_get_function_stack(mhp_entry_t *entry,
                             int            level,
                             char          *result_buf,
                             size_t         result_len);
size_t pfc_get_entry_name(mhp_entry_t  *entry,
                         char           *result_buf,
                         size_t          result_len);
/**
 * Takes an input of the form /a/b/c/d/foo.php and returns
 * a pointer to one-level directory and basefile name
 * (d/foo.php) in the same string.
 */
static char *pfc_get_base_filename(char *filename) {
  char *ptr;
  int   found = 0;

  if (!filename)
    return "";

  /* reverse search for "/" and return a ptr to the next char */
  for (ptr = filename + strlen(filename) - 1; ptr >= filename; ptr--) {
    if (*ptr == '/') {
      found++;
    }
    if (found == 2) {
      return ptr + 1;
    }
  }

  /* no "/" char found, so return the whole string */
  return filename;
}

/**
 * Reclaim the memory allocated for cpu_frequencies.
 *
 * @author cjiang
 */
static void clear_frequencies() {
  if (PFC_G(cpu_frequencies)) {
    free(PFC_G(cpu_frequencies));
    PFC_G(cpu_frequencies) = NULL;
  }
}


static int _pfc_find_storage_module(pfc_storage_module **ret TSRMLS_DC)
{
	int i;

	if (!PFC_G(storage_module)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "No storage module selected");
		return FAILURE;
	}

	for (i = 0; i < PFC_MAX_MODULES; i++) {
		if (pfc_storage_modules[i] && !strcasecmp(PFC_G(storage_module), pfc_storage_modules[i]->name)) {
			*ret = pfc_storage_modules[i];
			break;
		}
	}

	if (!*ret) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Couldn't load pfc storage module '%s'", PFC_G(storage_module));
		return FAILURE;
	}

	return SUCCESS;
}

/**
 * A hash function to calculate a 8-bit hash code for a function name.
 * This is based on a small modification to 'zend_inline_hash_func' by summing
 * up all bytes of the ulong returned by 'zend_inline_hash_func'.
 *
 * @param str, char *, string to be calculated hash code for.
 *
 * @author cjiang
 */
static inline uint8 pfc_inline_hash(char * str) {
  ulong h = 5381;
  uint i = 0;
  uint8 res = 0;

  while (*str) {
    h += (h << 5);
    h ^= (ulong) *str++;
  }

  for (i = 0; i < sizeof(ulong); i++) {
    res += ((uint8 *)&h)[i];
  }
  return res;
}


#define BEGIN_PROFILING(entries, symbol, file, fline,  profile_curr)                  \
  do {    \
  	    uint8 hash_code  = pfc_inline_hash(symbol);                          \
  	    mhp_entry_t *cur_entry = pfc_fast_alloc_hprof_entry();              \
     	    (cur_entry)->hash_code = hash_code;                               \
           (cur_entry)->name_hprof = symbol;                                 \
           (cur_entry)->filename = file;                                 \
           (cur_entry)->line = fline;                                 \
           (cur_entry)->prev_hprof = (*(entries));                           \
            /* Call the universal callback */                                 \
      	    pfc_mode_common_beginfn((entries), (cur_entry) TSRMLS_CC);         \
      	    pfc_mode_hier_beginfn_cb((entries), (cur_entry) TSRMLS_DC);   \
           (*(entries)) = (cur_entry);                                       \
  } while (0)
  
PHPAPI int pfc_register_storage_module(pfc_storage_module *ptr)
{
	int i, ret = FAILURE;

	for (i = 0; i < PFC_MAX_MODULES; i++) {
		if (!pfc_storage_modules[i]) {
			pfc_storage_modules[i] = ptr;
			ret = SUCCESS;
			break;
		}
	}

	return ret;
}
/* }}} */

/* {{{ PHP_INI
 */
PHP_INI_BEGIN()
	STD_PHP_INI_ENTRY("pfc.cache_namespace", "", PHP_INI_ALL, OnUpdateString, cache_namespace, zend_pfc_globals, pfc_globals)
	STD_PHP_INI_ENTRY("pfc.storage_module", "memory", PHP_INI_ALL, OnUpdateString, storage_module, zend_pfc_globals, pfc_globals)
	STD_PHP_INI_ENTRY("pfc.data_dir", "", PHP_INI_ALL, OnUpdateString, data_dir, zend_pfc_globals, pfc_globals)
	STD_PHP_INI_BOOLEAN("pfc.profiler_enable", "0", PHP_INI_SYSTEM|PHP_INI_PERDIR , OnUpdateBool, profiler_enable, zend_pfc_globals, pfc_globals)
	STD_PHP_INI_ENTRY("pfc.config_file", "", PHP_INI_ALL, OnUpdateString, config_file, zend_pfc_globals, pfc_globals)
	STD_PHP_INI_ENTRY("pfc.default_ttl", "3600", PHP_INI_ALL, OnUpdateLong, default_ttl, zend_pfc_globals, pfc_globals)
PHP_INI_END()
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(pfc)
{
	REGISTER_INI_ENTRIES();
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(pfc)
{
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}
/* }}} */

PHP_RINIT_FUNCTION(pfc)
{
	int i;
	
	old_compile_file = zend_compile_file;
	zend_compile_file = pfc_compile_file;
	pfc_old_execute = zend_execute;
	zend_execute = mhp_execute;

	/* {{{ Initialize auto globals in Zend Engine 2 */
	zend_is_auto_global("_ENV",     sizeof("_ENV")-1     TSRMLS_CC);
	zend_is_auto_global("_GET",     sizeof("_GET")-1     TSRMLS_CC);
	zend_is_auto_global("_POST",    sizeof("_POST")-1    TSRMLS_CC);
	zend_is_auto_global("_COOKIE",  sizeof("_COOKIE")-1  TSRMLS_CC);
	zend_is_auto_global("_REQUEST", sizeof("_REQUEST")-1 TSRMLS_CC);
	zend_is_auto_global("_FILES",   sizeof("_FILES")-1   TSRMLS_CC);
	zend_is_auto_global("_SERVER",  sizeof("_SERVER")-1  TSRMLS_CC);
	zend_is_auto_global("_SESSION", sizeof("_SESSION")-1 TSRMLS_CC);
	/* }}} */

	PFC_G(profiler_enabled) = 0;
	PFC_G(entry_free_list) = NULL;
	for (i = 0; i < 256; i++) {
   	PFC_G(func_hash_counters)[i] = 0;
  	}
	/* Initialize cpu_frequencies and cur_cpu_id. */
	PFC_G(cpu_frequencies) = NULL;
	PFC_G(cur_cpu_id) = 0;
	//code for function profile
	  /* Get the number of available logical CPUs. */
	PFC_G(cpu_num) = sysconf(_SC_NPROCESSORS_CONF);
	pfc_function_profile_init(TSRMLS_DC);
	BEGIN_PROFILING(&PFC_G(function_profile_entries), ROOT_SYMBOL, ROOT_SYMBOL,1,1);
}


/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(pfc)
{

	if (PFC_G(internal_functions)) {
		zend_hash_apply(PFC_G(internal_functions), (apply_func_t) pfc_fix_internal_functions TSRMLS_CC);
		zend_hash_destroy(PFC_G(internal_functions));
		FREE_HASHTABLE(PFC_G(internal_functions));
		PFC_G(internal_functions) = NULL;
	}

	zend_hash_apply(EG(function_table), (apply_func_t) pfc_remove_handler_functions TSRMLS_CC);
	if(PFC_G(profiler_enabled)) pfc_profiler_deinit(TSRMLS_C);

	//code for function profile
	pfc_function_profile_deinit(TSRMLS_DC);
	zend_compile_file = old_compile_file;
	zend_execute = pfc_old_execute;
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(pfc)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "pfc.support", "enabled");
	php_info_print_table_row(2, "pfc.version", PFC_EXTVER);
	php_info_print_table_end();
	DISPLAY_INI_ENTRIES();
}
/* }}} */

int function_pfcd(char *class_name, char *fname)
{
	zval *function_name, *retval, *argv[1], *param;
	char *pfc = "pfc";
	
	MAKE_STD_ZVAL(retval);
	MAKE_STD_ZVAL(param);
	MAKE_STD_ZVAL(function_name);

	if(fname > class_name)
	{
		array_init_size(param,2);
		add_next_index_string(param, class_name, 1);
	       add_next_index_string(param, fname, 1);
		argv[0] = param;
	}else{
		ZVAL_STRING(param, fname, 1);
		argv[0] = param;
	}
	
	ZVAL_STRING(function_name, pfc, 0);
	call_user_function(EG(function_table), NULL, function_name, retval, 1, argv TSRMLS_CC);

	efree(retval);
	zval_ptr_dtor(&param);
	efree(function_name);
	return 1;
}

int pfc_cache_log(char *fname, zval **args)
{

	return 1;
}

/**
* write profile data to log file
*/
int pfc_profiler_log( TSRMLS_DC) {
	char *filename = NULL, *fname = NULL;
	char *char_ptr, *strval=NULL;
	int retval = FAILURE;
	FILE *logfile;
	zval **data;
	time_t t = time(NULL);
	php_serialize_data_t var_hash;
	smart_str buf = {0};

	//to generate log file name based on request url
	if (PG(http_globals)[TRACK_VARS_SERVER]) {
		retval = zend_hash_find(Z_ARRVAL_P(PG(http_globals)[TRACK_VARS_SERVER]), "REQUEST_URI", sizeof("REQUEST_URI"), (void **) &data);

		if (retval == SUCCESS) {
				strval = estrdup(Z_STRVAL_PP(data));
		}else {
			strval = emalloc(strlen("cli") + 1);
			sprintf(strval,"cli");
		}
		
		/* replace slashes, dots, question marks, plus
		* signs, ampersands, spaces and other evil chars
		* with underscores */
		while ((char_ptr = strpbrk(strval, "/\\.?&+:*\"<>| ")) != NULL) {
						char_ptr[0] = '_';
		}			
	}else{
		strval = emalloc(strlen("norequesturi") + 1);
			sprintf(strval,"norequesturi");
	}

	 fname = emalloc(strlen("pfc.out..") + strlen(strval) +1 + 10);//10 for time stamp 1 for string end
        sprintf(fname,"pfc.out.%ld.%s",t,strval);
        filename = emalloc(strlen(PFC_G(data_dir)) + strlen(fname) + strlen("/profile/") + 1);
	 sprintf(filename,"%s/profile/%s",PFC_G(data_dir), fname);
	 efree(fname);

	logfile = fopen(filename,"w");
	efree(filename);
	if(strlen(strval) > 0) efree(strval);

	if(!logfile) return FAILURE;


	PHP_VAR_SERIALIZE_INIT(var_hash);
	php_var_serialize(&buf, &PFC_G(stats_count), &var_hash TSRMLS_CC);
	PHP_VAR_SERIALIZE_DESTROY(var_hash);

	if (buf.c) {
		fprintf(logfile, "%s\n",buf.c);	
	}
	efree(buf.c);
	fclose(logfile);
	
	return SUCCESS;
}

void pfc_profiler_deinit(TSRMLS_D) {

	if(PFC_G(profiler_file)) fclose(PFC_G(profiler_file));
}



int pfc_profiler_init(char *script_name TSRMLS_DC)
{
	char *filename = NULL, *fname = NULL;
	zval **data;
	char *char_ptr, *strval;
	int retval = FAILURE;
	if (PG(http_globals)[TRACK_VARS_SERVER]) {
		retval = zend_hash_find(Z_ARRVAL_P(PG(http_globals)[TRACK_VARS_SERVER]), "REQUEST_URI", sizeof("REQUEST_URI"), (void **) &data);

		if (retval == SUCCESS) {
				strval = estrdup(Z_STRVAL_PP(data));
		}else {
			strval = estrdup(script_name);
		}
		
		/* replace slashes, dots, question marks, plus
		* signs, ampersands, spaces and other evil chars
		* with underscores */
		while ((char_ptr = strpbrk(strval, "/\\.?&+:*\"<>| ")) != NULL) {
						char_ptr[0] = '_';
		}			
	}else{

		strval = emalloc(strlen("cache") + 1);
		sprintf(strval, "cache");
	}
	
        fname = emalloc(strlen("cachegrind.out.") + strlen(strval) +1);
        sprintf(fname,"cachegrind.out.%s",strval);
        filename = emalloc(strlen(PFC_G(data_dir)) + strlen(fname) + 8);
	 sprintf(filename,"%s/cache/%s",PFC_G(data_dir), fname);
	 efree(fname);
	
       PFC_G(profiler_file)= fopen(filename,"w");
	if(!PFC_G(profiler_file)) return FAILURE;

	
	efree(filename);
	if(strlen(strval) > 0) efree(strval);
	
	return SUCCESS;
}


int pfc_load_functions(void) 
{
	FILE* file;
	char *config_file_name;
	char  c;
	char buf[100];
	unsigned int line_len = 0;
	char *fname;
	char *class_name;

       class_name = fname = buf;
	config_file_name = malloc(strlen(PFC_G(data_dir)) + strlen(PFC_G(config_file)) +2);
	sprintf(config_file_name,"%s/%s",PFC_G(data_dir),PFC_G(config_file));
	file = fopen(config_file_name,"r");
	free(config_file_name);
	if(file) {
	    while( (c = getc(file)) != EOF && line_len <= 100) {
               if(c == '\n'){
			 buf[line_len] = 0;
			 if(line_len > 0) { function_pfcd(class_name,fname);}
			 line_len = 0;	 
	         }else{
                     if(c == ':')
                   	{
                   	    buf[line_len] = 0;
			    fname = buf + line_len +1;					
                   	}else {
		     	    buf[line_len] = c;
                   	}
		       line_len++;
	     	}
	    }
     	fclose(file);
	}
	
	return 1;
}

/* {{{ pfc_fix_internal_functions
	Restores renamed internal functions */
int pfc_fix_internal_functions(pfc_internal_function *mem_func TSRMLS_DC)
{
	zend_internal_function *fe = (zend_internal_function*)&mem_func->function;
	char *new_fname = NULL, *fname = zend_str_tolower_dup(fe->function_name, strlen(fe->function_name));

	/* remove renamed function */
	spprintf(&new_fname, 0, "%s%s", fname, PFC_FUNC_SUFFIX);
	zend_hash_del(mem_func->function_table, new_fname, strlen(new_fname) + 1);
	efree(new_fname);

	/* restore original function */
	zend_hash_update(mem_func->function_table, fname, strlen(fname) + 1, (void*)fe, sizeof(zend_function), NULL);

	efree(fname);
	return ZEND_HASH_APPLY_REMOVE;
}
/* }}} */
	
#define PFC_IS_HANDLER(fe)	(fe->type == ZEND_INTERNAL_FUNCTION && \
		fe->internal_function.handler == &ZEND_FN(pfc_call) && \
		!strstr(fe->common.function_name, "pfc_call"))

/* {{{ pfc_remove_handler_functions
	Try to clean up before zend mm  */
int pfc_remove_handler_functions(zend_function *fe TSRMLS_DC)
{
	if (PFC_IS_HANDLER(fe)) {
		return ZEND_HASH_APPLY_REMOVE;
	}

	return ZEND_HASH_APPLY_KEEP;
}
/* }}} */

/* {{{ pfc_arguments_hash
	Returns an MD5 hash of the given arguments */
void pfc_arguments_hash(int argc, char *fname, zval ***args, zval **object, char *hash TSRMLS_DC)
{
	php_serialize_data_t args_data;
	unsigned char raw_hash[16];
	smart_str args_str = {0};
	PHP_MD5_CTX md5ctx;
	int i;
	zval *args_array;

	/* construct php array from args */
	MAKE_STD_ZVAL(args_array);
	array_init_size(args_array, argc + 2 + (object != NULL));
	add_next_index_string(args_array, PFC_G(cache_namespace), 1);
	add_next_index_string(args_array, fname, 1);
	if (object) {
		Z_ADDREF_PP(object);
		add_next_index_zval(args_array, *object);
	}
	for (i = 0; i < argc; i++) {
		zval **arg_pp = args[i];
		Z_ADDREF_PP(arg_pp);
		add_next_index_zval(args_array, *arg_pp);
	}

	/* serialize php array */
	PHP_VAR_SERIALIZE_INIT(args_data);
	php_var_serialize(&args_str, &args_array, &args_data TSRMLS_CC);
	PHP_VAR_SERIALIZE_DESTROY(args_data);
	zval_ptr_dtor(&args_array);

	/* hash serialized string */
	PHP_MD5Init(&md5ctx);
	PHP_MD5Update(&md5ctx, args_str.c, args_str.len);
	PHP_MD5Final(raw_hash, &md5ctx);
	make_digest(hash, raw_hash);
	smart_str_free(&args_str);
}
/* }}} */




 #define END_PROFILING(entries, profile_curr)                            \
  do {                                                                                           \
		if (profile_curr) {                                                 \
		mhp_entry_t *cur_entry;                                            \
		/* Call the mode's endfn callback. */                             \
		/* NOTE(cjiang): we want to call this 'end_fn_cb' before */       \
		/* 'hp_mode_common_endfn' to avoid including the time in */       \
		/* 'hp_mode_common_endfn' in the profiling results.      */       \
		pfc_mode_hier_endfn_cb((entries) TSRMLS_CC);                \
		cur_entry = (*(entries));                                         \
		/* Call the universal callback */                                 \
	       pfc_mode_common_endfn((entries), (cur_entry) TSRMLS_CC);           \
		/* Free top entry and update entries linked list */               \
		(*(entries)) = (*(entries))->prev_hprof;                          \
		pfc_fast_free_hprof_entry(cur_entry);                              \
		}                                                                   \
  } while (0)

/**
 * Get the name of the current function. The name is qualified with
 * the class name if the function is in a class.
 *
 * @author kannan, hzhao
 */
static char *mhp_get_function_name(zend_op_array *ops TSRMLS_DC) {
  zend_execute_data *data;
  char              *func = NULL;
  char 		*filename = NULL;
  char              *cls = NULL;
  char              *ret = NULL;
  int                len;
  zend_function      *curr_func;

  data = EG(current_execute_data);

  if (data) {
    /* shared meta data for function on the call stack */
    curr_func = data->function_state.function;
    filename = pfc_get_base_filename((curr_func->op_array).filename);

    /* extract function name from the meta info */
    func = curr_func->common.function_name;

    if (func) {
      /* previously, the order of the tests in the "if" below was
       * flipped, leading to incorrect function names in profiler
       * reports. When a method in a super-type is invoked the
       * profiler should qualify the function name with the super-type
       * class name (not the class name based on the run-time type
       * of the object.
       */
      if (curr_func->common.scope) {
        cls = curr_func->common.scope->name;
      } else if (data->object) {
        cls = Z_OBJCE(*data->object)->name;
      }

      if (cls) {
        len = strlen(cls) + strlen(func) + 10;
        ret = (char*)emalloc(len);
        snprintf(ret, len, "%s::%s", cls, func);
      } else {
        ret = estrdup(func);
      }
    } else {
      long     curr_op;
      int      desc_len;
      char    *desc;
      int      add_filename = 0;

      /* we are dealing with a special directive/function like
       * include, eval, etc.
       */
      curr_op = data->opline->op2.u.constant.value.lval;
      switch (curr_op) {
        case ZEND_EVAL:
          func = "eval";
          break;
        case ZEND_INCLUDE:
          func = "include";
          add_filename = 1;
          break;
        case ZEND_REQUIRE:
          func = "require";
          add_filename = 1;
          break;
        case ZEND_INCLUDE_ONCE:
          func = "include_once";
          add_filename = 1;
          break;
        case ZEND_REQUIRE_ONCE:
          func = "require_once";
          add_filename = 1;
          break;
        default:
          func = "???_op";
          break;
      }

      /* For some operations, we'll add the filename as part of the function
       * name to make the reports more useful. So rather than just "include"
       * you'll see something like "run_init::foo.php" in your reports.
       */
      if (add_filename){
        
        int   len;
       
        len      = strlen("run_init") + strlen(filename) + 3;
        ret      = (char *)emalloc(len);
        snprintf(ret, len, "run_init::%s", filename);
      } else {
        ret = estrdup(func);
      }
    }
  }
  return ret;
}
		
/**
 * pfc function profile  enable replaced the zend_execute function with this
 * new execute function. We can do whatever profiling we need to
 * before and after calling the actual zend_execute().
 *
 * @author hzhao, kannan
 */
void mhp_execute (zend_op_array *ops TSRMLS_DC) {
  char          *func = NULL;
  char          *filename = NULL;
  int hp_profile_flag = 1;

  func = mhp_get_function_name(ops TSRMLS_CC);
  if (!func) {
   pfc_old_execute(ops TSRMLS_CC);
    return;
  }

  filename = ops->filename;
  BEGIN_PROFILING(&PFC_G(function_profile_entries), func,filename, ops->line_start, hp_profile_flag);
  pfc_old_execute(ops TSRMLS_CC);
  if (PFC_G(function_profile_entries)) {
    END_PROFILING(&PFC_G(function_profile_entries), hp_profile_flag);
  }
  efree(func);
}
		
/* {{{ zend_op_array pfc_compile_file (file_handle, type)
 *    This function provides a hook for compilation */
static zend_op_array *pfc_compile_file(zend_file_handle *file_handle, int type TSRMLS_DC)
{
	char           *filename;
	char           *func;
	int             len;
	zend_op_array  *ret;
	int             hp_profile_flag = 1;

	filename = pfc_get_base_filename(file_handle->filename);
	len      = strlen("load") + strlen(filename) + 3;
	func      = (char *)emalloc(len);
	snprintf(func, len, "load::%s", filename);

	BEGIN_PROFILING(&PFC_G(function_profile_entries), func,filename, 1,  hp_profile_flag);
	ret = old_compile_file(file_handle, type TSRMLS_CC);
	if (PFC_G(function_profile_entries)) {
	END_PROFILING(&PFC_G(function_profile_entries), hp_profile_flag);
	}

	efree(func);
  
	if(!PFC_G(profiler_enabled)){
	if(pfc_profiler_init((char *) ret->filename TSRMLS_DC) == SUCCESS){
		PFC_G(profiler_enabled) = 1;
	}
	}

	pfc_load_functions();
	
	return ret;
}
/* }}} */


/* {{{ proto mixed pfc_call()
   Function to perform the actual pfcd call */
PHP_FUNCTION(pfc_call)
{
	char *fname, *key = NULL;
	char hash[33] = "";
	int argc = 0;
	zval ***args = NULL, *temp_value = NULL;
	size_t key_len;
	zend_function *fe;
	pfc_storage_module *mod = NULL;
	char *cname = NULL;

	if (_pfc_find_storage_module(&mod TSRMLS_CC) == FAILURE) {
		RETURN_FALSE;
	}

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "*", &args, &argc) == FAILURE) {
		RETURN_FALSE;
	}

	fe = EG(current_execute_data)->function_state.function;

	if (strlen(fe->common.function_name) == strlen("pfc_call") && !memcmp(fe->common.function_name, "pfc_call", strlen("pfc_call"))) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Cannot call pfc_call() directly");
		if (argc) {
			efree(args);
		}
		RETURN_FALSE;
	}

	fname = estrdup(fe->common.function_name);
	if(fe->common.scope) cname = estrdup(fe->common.scope->name);
	
	/* construct hash key */
	pfc_arguments_hash(argc, fname, args, EG(current_execute_data)->object ? &EG(current_execute_data)->object : NULL, hash TSRMLS_CC);
	key_len = spprintf(&key, 0, "%s%s", PFC_KEY_PREFIX, hash);

	//write cache key to log file
	if(cname) {
            fprintf(PFC_G(profiler_file), "%s:%s|%s\n",cname,fname,key);
	     efree(cname);
	}else{
	     fprintf(PFC_G(profiler_file), "%s|%s\n",fname,key);
	}
       fflush(PFC_G(profiler_file));

	   BEGIN_PROFILING(&PFC_G(function_profile_entries), fname,fe->op_array.filename, fe->op_array.line_start, 1);
	/* look up key in storage mod */
	if (mod->get(key, return_value_ptr TSRMLS_CC) == FAILURE) {
		/* create callable for original function */
		char *new_fname = NULL;
		zval **obj_pp = NULL;
		size_t new_fname_len = spprintf(&new_fname, 0, "%s%s", fname, PFC_FUNC_SUFFIX);
		zval *callable;
		MAKE_STD_ZVAL(callable);
		if (fe->common.scope) {
			/* static method */
			array_init_size(callable, 2);
			if (EG(current_execute_data)->object) {
				obj_pp = &EG(current_execute_data)->object;
				Z_ADDREF_PP(obj_pp);
				add_next_index_zval(callable, *obj_pp);
			} else {
				add_next_index_stringl(callable, fe->common.scope->name, strlen(fe->common.scope->name), 1);
			}
			add_next_index_stringl(callable, new_fname, new_fname_len, 0);
		} else {
			/* function */
			ZVAL_STRINGL(callable, new_fname, new_fname_len, 0);
		}

		/* ensure we have a zval for the return value even if it isn't used */ 
		//if (!return_value_used) {
		if(!return_value_ptr) {
			MAKE_STD_ZVAL(temp_value);
			return_value_ptr = &temp_value;
		}

		/* call original function */
		if (call_user_function(&fe->common.scope->function_table, obj_pp, callable, *return_value_ptr, argc, (argc ? *args : NULL) TSRMLS_CC) == FAILURE) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to call pfcd function %s.", fname);
		} else {
			/* store result in storage mod */
			mod->set(key, *return_value_ptr TSRMLS_CC);
		}
		zval_ptr_dtor(&callable);

		/* free the return value if it's not being returned */
		if (temp_value) {
			zval_ptr_dtor(&temp_value);
		}
	}
if (PFC_G(function_profile_entries)) {
    END_PROFILING(&PFC_G(function_profile_entries), 1);
  }
	efree(fname);
	efree(key);
	if (argc) {
		efree(args);
	}
}
/* }}} */

PHP_FUNCTION (pfc_stats)
{
	php_serialize_data_t var_hash;
	smart_str buf = {0};

	Z_TYPE_P(return_value) = IS_STRING;
	Z_STRVAL_P(return_value) = NULL;
	Z_STRLEN_P(return_value) = 0;

	PHP_VAR_SERIALIZE_INIT(var_hash);
	php_var_serialize(&buf, &PFC_G(stats_count), &var_hash TSRMLS_CC);
	PHP_VAR_SERIALIZE_DESTROY(var_hash);

	if (buf.c) {
		RETURN_STRINGL(buf.c, buf.len, 0);
	} else {
		RETURN_NULL();
	}
	
	//if(PFC_G(function_profile_ever_enabled)){
	//	RETURN_ZVAL(PFC_G(stats_count), 1, 0);
	//}

}

/* {{{ proto bool pfc(string fname)
   Memoizes the given function */
PHP_FUNCTION(pfc)
{
	zval *callable;
	char *fname = NULL, *new_fname = NULL;
	int fname_len;
	size_t new_fname_len;
	zend_function *fe, *dfe, func, *new_dfe;
	HashTable *function_table = EG(function_table);
	pfc_storage_module *mod = NULL;

	if (_pfc_find_storage_module(&mod TSRMLS_CC) == FAILURE) {
		RETURN_FALSE;
	}

	/* parse argument */

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &callable) == FAILURE) {
		return;
	}
	
	if (Z_TYPE_P(callable) == IS_ARRAY) {
		zval **fname_zv, **obj_zv;
		if (zend_hash_num_elements(Z_ARRVAL_P(callable)) == 2) {
			zend_hash_index_find(Z_ARRVAL_P(callable), 0, (void **)&obj_zv);
			zend_hash_index_find(Z_ARRVAL_P(callable), 1, (void **)&fname_zv);
		}

		if (obj_zv && fname_zv && (Z_TYPE_PP(obj_zv) == IS_OBJECT || Z_TYPE_PP(obj_zv) == IS_STRING) && Z_TYPE_PP(fname_zv) == IS_STRING) {
			/* looks like a valid callback */
			zend_class_entry *ce, **pce;

			if (Z_TYPE_PP(obj_zv) == IS_OBJECT) {
				ce = Z_OBJCE_PP(obj_zv);
			} else if (Z_TYPE_PP(obj_zv) == IS_STRING) {
				if (zend_lookup_class(Z_STRVAL_PP(obj_zv), Z_STRLEN_PP(obj_zv), &pce TSRMLS_CC) == FAILURE) {
					php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Class %s() not found", Z_STRVAL_PP(obj_zv));
					RETURN_FALSE;
				}
				ce = *pce;
			}

			function_table = &ce->function_table;

			fname = zend_str_tolower_dup(Z_STRVAL_PP(fname_zv), Z_STRLEN_PP(fname_zv));
			fname_len = Z_STRLEN_PP(fname_zv);

			if (zend_hash_exists(function_table, fname, fname_len + 1) == FAILURE) {
				php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Method %s() not found", fname);
				efree(fname);
				RETURN_FALSE;
			}
		} else {
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Argument is not a valid callback");
			return;
		}
	} else if (Z_TYPE_P(callable) == IS_STRING) {
		fname = zend_str_tolower_dup(Z_STRVAL_P(callable), Z_STRLEN_P(callable));
		fname_len = Z_STRLEN_P(callable);

		if (fname_len == strlen("pfc") && !memcmp(fname, "pfc", fname_len)) {
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Cannot pfc pfc()!");
			efree(fname);
			RETURN_FALSE;
		}
	} else {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Argument must be a string (function name) or an array (class or object and method name)");
		RETURN_FALSE;
	}


	/* find source function */
	if (zend_hash_find(function_table, fname, fname_len + 1, (void**)&fe) == FAILURE) {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Function %s() not found", fname);
		efree(fname);
		RETURN_FALSE;
	}

	if (PFC_IS_HANDLER(fe)) {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "%s() is already pfcd", fe->common.function_name);
		efree(fname);
		RETURN_FALSE;
	}


	if (PFC_RETURNS_REFERENCE(fe)) {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Cannot cache functions which return references");
		efree(fname);
		RETURN_FALSE;
	}

	func = *fe;
	function_add_ref(&func);

	/* find dest function */
	if (zend_hash_find(EG(function_table), "pfc_call", strlen("pfc_call") + 1, (void**)&dfe) == FAILURE) {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "pfc_call() not found");
		efree(fname);
		RETURN_FALSE;
	}
	
	/* copy dest entry with source name */
	new_dfe = emalloc(sizeof(zend_function));
	memcpy(new_dfe, dfe, sizeof(zend_function));
	new_dfe->common.scope = fe->common.scope;
	new_dfe->common.fn_flags = fe->common.fn_flags;
	new_dfe->common.function_name = fe->common.function_name;
	new_dfe->op_array.filename = fe->op_array.filename;
	new_dfe->op_array.line_start = fe->op_array.line_start;
        new_dfe->common.return_reference = 1;
	/* replace source with dest */
	if (zend_hash_update(function_table, fname, fname_len + 1, new_dfe, sizeof(zend_function), NULL) == FAILURE) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Error replacing %s()", fname);
		zend_function_dtor(&func);
		efree(fname);
		efree(new_dfe);
		RETURN_FALSE;
	}
	efree(new_dfe);

	/* rename source */
	if (func.type == ZEND_INTERNAL_FUNCTION) {
		if (!PFC_G(internal_functions)) {
			ALLOC_HASHTABLE(PFC_G(internal_functions));
			zend_hash_init(PFC_G(internal_functions), 4, NULL, NULL, 0);
		}

		pfc_internal_function mem_func;
		mem_func.function = func;
		mem_func.function_table = function_table;
		zend_hash_add(PFC_G(internal_functions), fname, fname_len + 1, (void*)&mem_func, sizeof(pfc_internal_function), NULL);
	}

	new_fname_len = spprintf(&new_fname, 0, "%s%s", fname, PFC_FUNC_SUFFIX);
	if (zend_hash_add(function_table, new_fname, new_fname_len + 1, &func, sizeof(zend_function), NULL) == FAILURE) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Error renaming %s()", fname);
		efree(new_fname);
		zend_function_dtor(&func);
		RETURN_FALSE;
	}
	efree(new_fname);
	efree(fname);

	RETURN_TRUE;
}


//code for time cost profile


/**
 * Fast allocate a hp_entry_t structure. Picks one from the
 * free list if available, else does an actual allocate.
 *
 * Doesn't bother initializing allocated memory.
 *
 * @author kannan
 */
static mhp_entry_t *pfc_fast_alloc_mhp_entry() {
  mhp_entry_t *p;

  p =  PFC_G(entry_free_list);

  if (p) {
    PFC_G(entry_free_list) = p->prev_hprof;
    return p;
  } else {
    return (mhp_entry_t *)malloc(sizeof(mhp_entry_t));
  }
}

/**
 * Free any items in the free list.
 */
static void pfc_free_the_free_list() {
  mhp_entry_t *p = PFC_G(entry_free_list);
  mhp_entry_t *cur;

  while (p) {
    cur = p;
    p = p->prev_hprof;
    free(cur);
  }
}

/**
 * Fast allocate a hp_entry_t structure. Picks one from the
 * free list if available, else does an actual allocate.
 *
 * Doesn't bother initializing allocated memory.
 *
 * @author kannan
 */
static mhp_entry_t *pfc_fast_alloc_hprof_entry() {
  mhp_entry_t *p;

  p = PFC_G(entry_free_list);

  if (p) {
    PFC_G(entry_free_list) = p->prev_hprof;
    return p;
  } else {
    return (mhp_entry_t *)malloc(sizeof(mhp_entry_t));
  }
}

/**
 * Fast free a hp_entry_t structure. Simply returns back
 * the hp_entry_t to a free list and doesn't actually
 * perform the free.
 *
 * @author kannan
 */
static void pfc_fast_free_hprof_entry(mhp_entry_t *p) {

  /* we use/overload the prev_hprof field in the structure to link entries in
   * the free list. */
  p->prev_hprof =PFC_G(entry_free_list);
  PFC_G(entry_free_list) = p;
}


/**
 * XHPROF universal begin function.
 * This function is called for all modes before the
 * mode's specific begin_function callback is called.
 *
 * @param  hp_entry_t **entries  linked list (stack)
 *                                  of hprof entries
 * @param  hp_entry_t  *current  hprof entry for the current fn
 * @return void
 * @author kannan, veeve
 */
void pfc_mode_common_beginfn(mhp_entry_t **entries,
                            mhp_entry_t  *current  TSRMLS_DC) {
  mhp_entry_t   *p;

  /* This symbol's recursive level */
  int    recurse_level = 0;

  if (PFC_G(func_hash_counters)[current->hash_code] > 0) {
    /* Find this symbols recurse level */
    for(p = (*entries); p; p = p->prev_hprof) {
      if (!strcmp(current->name_hprof, p->name_hprof)) {
        recurse_level = (p->rlvl_hprof) + 1;
        break;
      }
    }
  }
  PFC_G(func_hash_counters)[current->hash_code]++;

  /* Init current function's recurse level */
  current->rlvl_hprof = recurse_level;
}

/**
 * XHPROF universal end function.  This function is called for all modes after
 * the mode's specific end_function callback is called.
 *
 * @param  hp_entry_t **entries  linked list (stack) of hprof entries
 * @return void
 * @author kannan, veeve
 */
void pfc_mode_common_endfn(mhp_entry_t **entries, mhp_entry_t *current TSRMLS_DC) {
  PFC_G(func_hash_counters)[current->hash_code]--;
}

/**
 * Get time stamp counter (TSC) value via 'rdtsc' instruction.
 *
 * @return 64 bit unsigned integer
 * @author cjiang
 */
inline uint64 cycle_timer() {
  uint32 __a,__d;
  uint64 val;
  asm volatile("rdtsc" : "=a" (__a), "=d" (__d));
  (val) = ((uint64)__a) | (((uint64)__d)<<32);
  return val;
}

/**
 *get current time in miroseconds
 */
static uint64 pfc_get_utime(void)
{
#ifdef HAVE_GETTIMEOFDAY
	struct timeval tp;
	if (gettimeofday((struct timeval *) &tp, NULL) == 0) {

		return tp.tv_sec*1000000 + tp.tv_usec;
	}
#endif
	return 0;
}
/**
 * Convert from TSC counter values to equivalent microseconds.
 *
 * @param uint64 count, TSC count value
 * @param double cpu_frequency, the CPU clock rate (MHz)
 * @return 64 bit unsigned integer
 *
 * @author cjiang
 */
inline double get_us_from_tsc(uint64 count, double cpu_frequency) {
  return count / cpu_frequency;
}

/**
 * Increment the count of the given stat with the given count
 * If the stat was not set before, inits the stat to the given count
 *
 * @param  zval *counts   Zend hash table pointer
 * @param  char *name     Name of the stat
 * @param  long  count    Value of the stat to incr by
 * @return void
 * @author kannan
 */
void pfc_inc_count(zval *counts, char *name, long count TSRMLS_DC) {
  HashTable *ht;
  void *data;

  if (!counts) return;
  ht = HASH_OF(counts);
  if (!ht) return;

  if (zend_hash_find(ht, name, strlen(name) + 1, &data) == SUCCESS) {
    ZVAL_LONG(*(zval**)data, Z_LVAL_PP((zval**)data) + count);
  } else {
    add_assoc_long(counts, name, count);
  }
}

/**
 * Looksup the hash table for the given symbol
 * Initializes a new array() if symbol is not present
 *
 * @author kannan, veeve
 */
zval * pfc_hash_lookup(char *symbol, char *filename, zend_uint  line TSRMLS_DC) {
  HashTable   *ht;
  void        *data;
  zval        *counts = (zval *) 0;

  /* Bail if something is goofy */
  if (!PFC_G(stats_count) || !(ht = HASH_OF(PFC_G(stats_count)))) {
    return (zval *) 0;
  }

  /* Lookup our hash table */
  if (zend_hash_find(ht, symbol, strlen(symbol) + 1, &data) == SUCCESS) {
    /* Symbol already exists */
    counts = *(zval **) data;
  }
  else {
    /* Add symbol to hash table */
    MAKE_STD_ZVAL(counts);
    array_init(counts);
    add_assoc_string(counts, "file", filename , 1);
    add_assoc_long(counts, "line", line);
    add_assoc_zval(PFC_G(stats_count), symbol, counts);
  }

  return counts;
}

/**
 * XHPROF_MODE_HIERARCHICAL's begin function callback
 *
 * @author kannan
 */
void pfc_mode_hier_beginfn_cb(mhp_entry_t **entries,
                             mhp_entry_t  *current  TSRMLS_DC) {
  /* Get start tsc counter */
  //current->tsc_start = cycle_timer();
  current->tsc_start = pfc_get_utime();
}


/**
 * XHPROF shared end function callback
 *
 * @author kannan
 */
zval * pfc_mode_shared_endfn_cb(mhp_entry_t *top,
                               char          *symbol  TSRMLS_DC) {
  zval    *counts;
  uint64   tsc_end;

  double gtod_value, rdtsc_value;

  /* Get end tsc counter */
 // tsc_end = cycle_timer();
  tsc_end = pfc_get_utime();

  /* Get the stat array */
  if (!(counts = pfc_hash_lookup(symbol, top->filename, top->line TSRMLS_CC))) {
    return (zval *) 0;
  }

  /* Bump stats in the counts hashtable */
  pfc_inc_count(counts, "ct", 1  TSRMLS_CC);

  //pfc_inc_count(counts, "wt", get_us_from_tsc(tsc_end - top->tsc_start,
   //    PFC_G(cpu_frequencies)[PFC_G(cur_cpu_id)]) TSRMLS_CC);
  pfc_inc_count(counts, "wt", (tsc_end - top->tsc_start) TSRMLS_CC);
  return counts;
}

/**
 * XHPROF_MODE_HIERARCHICAL's end function callback
 *
 * @author kannan
 */
void pfc_mode_hier_endfn_cb(mhp_entry_t **entries  TSRMLS_DC) {
  mhp_entry_t   *top = (*entries);
  zval            *counts;
  char             symbol[SCRATCH_BUF_LEN];

  /* Get the stat array */
  pfc_get_function_stack(top, 2, symbol, sizeof(symbol));
  if (!(counts = pfc_mode_shared_endfn_cb(top,
                                         symbol  TSRMLS_CC))) {
    return;
  }

}

/**
 * Returns formatted function name
 *
 * @param  entry        hp_entry
 * @param  result_buf   ptr to result buf
 * @param  result_len   max size of result buf
 * @return total size of the function name returned in result_buf
 * @author veeve
 */
size_t pfc_get_entry_name(mhp_entry_t  *entry,
                         char           *result_buf,
                         size_t          result_len) {
  size_t    len = 0;

  /* Validate result_len */
  if (result_len <= 1) {
    /* Insufficient result_bug. Bail! */
    return 0;
  }

  /* Add '@recurse_level' if required */
  /* NOTE:  Dont use snprintf's return val as it is compiler dependent */
  if (entry->rlvl_hprof) {
    snprintf(result_buf, result_len,
             "%s@%d",
             entry->name_hprof, entry->rlvl_hprof);
  }
  else {
    snprintf(result_buf, result_len,
             "%s",
             entry->name_hprof);
  }

  /* Force null-termination at MAX */
  result_buf[result_len - 1] = 0;

  return strlen(result_buf);
}


/**
 * Build a caller qualified name for a callee.
 *
 * For example, if A() is caller for B(), then it returns "A==>B".
 * Recursive invokations are denoted with @<n> where n is the recursion
 * depth.
 *
 * For example, "foo==>foo@1", and "foo@2==>foo@3" are examples of direct
 * recursion. And  "bar==>foo@1" is an example of an indirect recursive
 * call to foo (implying the foo() is on the call stack some levels
 * above).
 *
 * @author kannan, veeve
 */
size_t pfc_get_function_stack(mhp_entry_t *entry,
                             int            level,
                             char          *result_buf,
                             size_t         result_len) {
  size_t         len = 0;

  /* End recursion if we dont need deeper levels or we dont have any deeper
   * levels */
  if (!entry->prev_hprof || (level <= 1)) {
    return pfc_get_entry_name(entry, result_buf, result_len);
  }

  /* Take care of all ancestors first */
  len = pfc_get_function_stack(entry->prev_hprof,
                              level - 1,
                              result_buf,
                              result_len);

  /* Append the delimiter */
# define    HP_STACK_DELIM        "==>"
# define    HP_STACK_DELIM_LEN    (sizeof(HP_STACK_DELIM) - 1)

  if (result_len < (len + HP_STACK_DELIM_LEN)) {
    /* Insufficient result_buf. Bail out! */
    return len;
  }

  /* Add delimiter only if entry had ancestors */
  if (len) {
    strncat(result_buf + len,
            HP_STACK_DELIM,
            result_len - len);
    len += HP_STACK_DELIM_LEN;
  }

# undef     HP_STACK_DELIM_LEN
# undef     HP_STACK_DELIM

  /* Append the current function name */
  return len + pfc_get_entry_name(entry,
                                 result_buf + len,
                                 result_len - len);
}

/**
 * Get time delta in microseconds.
 */
static long get_us_interval(struct timeval *start, struct timeval *end) {
  return (((end->tv_sec - start->tv_sec) * 1000000)
          + (end->tv_usec - start->tv_usec));
}

/**
 * This is a microbenchmark to get cpu frequency the process is running on. The
 * returned value is used to convert TSC counter values to microseconds.
 *
 * @return double.
 * @author cjiang
 */
static double get_cpu_frequency() {
  struct timeval start;
  struct timeval end;

  if (gettimeofday(&start, 0)) {
    perror("gettimeofday");
    return 0.0;
  }
  uint64 tsc_start = cycle_timer();
  /* Sleep for 5 miliseconds. Comparaing with gettimeofday's  few microseconds
   * execution time, this should be enough. */
  usleep(5000);
  if (gettimeofday(&end, 0)) {
    perror("gettimeofday");
    return 0.0;
  }
  uint64 tsc_end = cycle_timer();
  return (tsc_end - tsc_start) * 1.0 / (get_us_interval(&start, &end));
}

/**
 * Calculate frequencies for all available cpus.
 *
 * @author cjiang
 */
static void get_all_cpu_frequencies() {
  int id;
  double frequency;

  PFC_G(cpu_frequencies) = malloc(sizeof(double) * PFC_G(cpu_num));
  if (PFC_G(cpu_frequencies) == NULL) {
    return;
  }

  /* Iterate over all cpus found on the machine. */
  for (id = 0; id < PFC_G(cpu_num); ++id) {

    /* Make sure the current process gets scheduled to the target cpu. This
     * might not be necessary though. */
    usleep(0);

    frequency = get_cpu_frequency();
    if (frequency == 0.0) {
      clear_frequencies();
      return;
    }
    PFC_G(cpu_frequencies)[id] = frequency;
  }
}


int pfc_function_profile_init(TSRMLS_DC) {
	if(!PFC_G(function_profile_ever_enabled)){
		PFC_G(function_profile_ever_enabled) = 1;
		PFC_G(function_profile_entries) = NULL;
		PFC_G(stats_count) = NULL;

		MAKE_STD_ZVAL(PFC_G(stats_count));
		array_init(PFC_G(stats_count));

		/* NOTE(cjiang): some fields such as cpu_frequencies take relatively longer
		* to initialize, (5 milisecond per logical cpu right now), therefore we
		* calculate them lazily. */
		if (PFC_G(cpu_frequencies) == NULL) {
		get_all_cpu_frequencies();
		}
	}

	
	
}


static int pfc_profile_trigger_enabled(char *var_name TSRMLS_DC)
{
	zval **dummy;

	if (
		(
			PG(http_globals)[TRACK_VARS_GET] &&
			zend_hash_find(PG(http_globals)[TRACK_VARS_GET]->value.ht, var_name, strlen(var_name) + 1, (void **) &dummy) == SUCCESS
		) || (
			PG(http_globals)[TRACK_VARS_POST] &&
			zend_hash_find(PG(http_globals)[TRACK_VARS_POST]->value.ht, var_name, strlen(var_name) + 1, (void **) &dummy) == SUCCESS
		) || (
			PG(http_globals)[TRACK_VARS_COOKIE] &&
			zend_hash_find(PG(http_globals)[TRACK_VARS_COOKIE]->value.ht, var_name, strlen(var_name) + 1, (void **) &dummy) == SUCCESS
		)
	) {
		return 1;
	}

	return 0;
}

int pfc_function_profile_deinit(TSRMLS_DC) {

	/* End any unfinished calls */
	if(PFC_G(function_profile_ever_enabled)) {
		while (PFC_G(function_profile_entries)) {
		END_PROFILING(&PFC_G(function_profile_entries), 1);
		}
	}

	
	 
	 if(PFC_G(stats_count)) {
		if(PFC_G(profiler_enable) ||
			pfc_profile_trigger_enabled("PFC_PROFILE_TRACE" TSRMLS_DC)) {
			pfc_profiler_log(TSRMLS_DC);
		}
		
		zval_dtor(PFC_G(stats_count));
		FREE_ZVAL(PFC_G(stats_count));
	 }

	 PFC_G(function_profile_ever_enabled) = 0;
	 PFC_G(function_profile_entries) = NULL;

}




/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
