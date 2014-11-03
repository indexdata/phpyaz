/* This file is part of PHP YAZ.
 * Copyright (C) Index Data 2004-2014
 * See the file LICENSE for details.
 */

#ifndef PHP_YAZ_H
#define PHP_YAZ_H

#define PHP_YAZ_VERSION "1.1.8"

#if HAVE_YAZ

#ifdef ZTS
#include "TSRM.h"
#endif

extern zend_module_entry yaz_module_entry;
#define yaz_module_ptr &yaz_module_entry

PHP_FUNCTION(yaz_connect);
PHP_FUNCTION(yaz_close);
PHP_FUNCTION(yaz_search);
PHP_FUNCTION(yaz_wait);
PHP_FUNCTION(yaz_errno);
PHP_FUNCTION(yaz_error);
PHP_FUNCTION(yaz_addinfo);
PHP_FUNCTION(yaz_hits);
PHP_FUNCTION(yaz_record);
PHP_FUNCTION(yaz_syntax);
PHP_FUNCTION(yaz_element);
PHP_FUNCTION(yaz_range);
PHP_FUNCTION(yaz_itemorder);
PHP_FUNCTION(yaz_scan);
PHP_FUNCTION(yaz_scan_result);
PHP_FUNCTION(yaz_es_result);
PHP_FUNCTION(yaz_present);
PHP_FUNCTION(yaz_ccl_conf);
PHP_FUNCTION(yaz_ccl_parse);
#if YAZ_VERSIONL >= 0x050100
PHP_FUNCTION(yaz_cql_parse);
PHP_FUNCTION(yaz_cql_conf);
#endif
PHP_FUNCTION(yaz_database);
PHP_FUNCTION(yaz_sort);
PHP_FUNCTION(yaz_schema);
PHP_FUNCTION(yaz_set_option);
PHP_FUNCTION(yaz_get_option);
PHP_FUNCTION(yaz_es);

ZEND_BEGIN_MODULE_GLOBALS(yaz)
	int assoc_seq;
	long max_links;
	long keepalive;
	char *log_file;
	char *log_mask;
ZEND_END_MODULE_GLOBALS(yaz)

#ifdef ZTS
#define YAZSG(v) TSRMG(yaz_globals_id, zend_yaz_globals *, v)
#else
#define YAZSG(v) (yaz_globals.v)
#endif

#else

#define yaz_module_ptr NULL
#endif

#define phpext_yaz_ptr yaz_module_ptr
#endif
