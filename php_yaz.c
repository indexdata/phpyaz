/* This file is part of PHP YAZ.
 * Copyright (C) Index Data 2004-2018
 * See the file LICENSE for details.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"

#if HAVE_YAZ

#include "ext/standard/info.h"

#include <yaz/yaz-version.h>

#include "php_yaz.h"

/* for PHP 8+ */
#ifndef TSRMLS_CC
#define TSRMLS_CC
#define TSRMLS_DC
#endif

#ifndef YAZ_VERSIONL
#error YAZ version 3.0.2 or later must be used.
#elif YAZ_VERSIONL < 0x030020
#error YAZ version 3.0.2 or later must be used.
#endif

#ifdef PHP_WIN32
#include <process.h>
#endif

#include <yaz/log.h>
#include <yaz/proto.h>
#include <yaz/marcdisp.h>
#include <yaz/yaz-util.h>
#include <yaz/yaz-ccl.h>
#include <yaz/cql.h>
#include <yaz/oid_db.h>
#include <yaz/zoom.h>
#include <yaz/pquery.h>

#if YAZ_VERSIONL >= 0x050100
#include <yaz/rpn2cql.h>
#endif

#ifndef ODR_INT_PRINTF
#define ODR_INT_PRINTF "%d"
#endif

#define MAX_ASSOC 200

typedef struct Yaz_AssociationInfo *Yaz_Association;

#if PHP_API_VERSION >= 20150101
typedef size_t zend_size_t;
#define ADD_ASSOC_STRING(x, y, z) add_assoc_string(x, y, z)
#define ADD_NEXT_INDEX_STRING(x, y) add_next_index_string(x, y)
#define ADD_NEXT_INDEX_STRINGl(x, y, z) add_next_index_stringl(x, y, z)
#else
#define ADD_ASSOC_STRING(x, y, z) add_assoc_string(x, y, z, 1)
#define ADD_NEXT_INDEX_STRING(x, y) add_next_index_string(x, y, 1)
#define ADD_NEXT_INDEX_STRINGl(x, y, z) add_next_index_stringl(x, y, z, 1)
typedef int zend_size_t;
typedef long zend_long;
#endif

struct Yaz_AssociationInfo {
	CCL_bibset bibset;
#if YAZ_VERSIONL >= 0x050100
	cql_transform_t ct;
#endif
	ZOOM_connection zoom_conn;
	ZOOM_resultset zoom_set;
	ZOOM_scanset zoom_scan;
	ZOOM_package zoom_package;
	char *sort_criteria;
	int persistent;
	int in_use;
	int order;
#if PHP_API_VERSION >= 20150101
	zend_resource *zval_resource;
#else
	int zval_resource;
#endif
	time_t time_stamp;
};

static Yaz_Association yaz_association_mk()
{
	Yaz_Association p = xmalloc(sizeof(*p));

	p->zoom_conn = ZOOM_connection_create(0);
	p->zoom_set = 0;
	p->zoom_scan = 0;
	p->zoom_package = 0;
	ZOOM_connection_option_set(p->zoom_conn, "implementationName", "PHP");
	ZOOM_connection_option_set(p->zoom_conn, "async", "1");
	p->sort_criteria = 0;
	p->in_use = 0;
	p->order = 0;
	p->persistent = 0;
	p->bibset = ccl_qual_mk();
#if YAZ_VERSIONL >= 0x050100
	p->ct = cql_transform_create();
#endif
	p->time_stamp = 0;
	return p;
}

static void yaz_association_destroy(Yaz_Association p)
{
	if (!p) {
		return;
	}

#if YAZ_VERSIONL >= 0x050100
	cql_transform_close(p->ct);
#endif
	ZOOM_resultset_destroy(p->zoom_set);
	ZOOM_scanset_destroy(p->zoom_scan);
	ZOOM_package_destroy(p->zoom_package);
	ZOOM_connection_destroy(p->zoom_conn);
	xfree(p->sort_criteria);
	ccl_qual_rm(&p->bibset);
}

#ifdef ZTS
static MUTEX_T yaz_mutex;
#endif

ZEND_DECLARE_MODULE_GLOBALS(yaz);

static Yaz_Association *shared_associations;
static int order_associations;
static int le_link;


#ifdef COMPILE_DL_YAZ
ZEND_GET_MODULE(yaz)
#endif

ZEND_BEGIN_ARG_INFO_EX(arginfo_yaz_connect, 0, 0, 1)
    ZEND_ARG_INFO(0, url)
    ZEND_ARG_INFO(0, options)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(arginfo_yaz_close, 0, 0, 1)
    ZEND_ARG_INFO(0, id)
ZEND_END_ARG_INFO();

#define arginfo_yaz_present       arginfo_yaz_close

#define arginfo_yaz_errno         arginfo_yaz_close

#define arginfo_yaz_error         arginfo_yaz_close

#define arginfo_yaz_addinfo       arginfo_yaz_close

#define arginfo_yaz_es_result     arginfo_yaz_close

ZEND_BEGIN_ARG_INFO_EX(arginfo_yaz_search, 0, 0, 3)
    ZEND_ARG_INFO(0, id)
    ZEND_ARG_INFO(0, type)
    ZEND_ARG_INFO(0, query)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(arginfo_yaz_wait, 0, 0, 0)
    ZEND_ARG_INFO(1, options)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(arginfo_yaz_hits, 0, 0, 1)
    ZEND_ARG_INFO(0, id)
    ZEND_ARG_INFO(1, searchresult)
    ZEND_ARG_INFO(0, query)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(arginfo_yaz_record, 0, 0, 3)
    ZEND_ARG_INFO(0, id)
    ZEND_ARG_INFO(0, pos)
    ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(arginfo_yaz_syntax, 0, 0, 2)
    ZEND_ARG_INFO(0, id)
    ZEND_ARG_INFO(0, syntax)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(arginfo_yaz_element, 0, 0, 2)
    ZEND_ARG_INFO(0, id)
    ZEND_ARG_INFO(0, elementsetname)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(arginfo_yaz_schema, 0, 0, 2)
    ZEND_ARG_INFO(0, id)
    ZEND_ARG_INFO(0, schema)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(arginfo_yaz_set_option, 0, 0, 2)
    ZEND_ARG_INFO(0, id)
    ZEND_ARG_INFO(0, options_or_name)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(arginfo_yaz_get_option, 0, 0, 2)
    ZEND_ARG_INFO(0, id)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(arginfo_yaz_range, 0, 0, 3)
    ZEND_ARG_INFO(0, id)
    ZEND_ARG_INFO(0, start)
    ZEND_ARG_INFO(0, number)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(arginfo_yaz_sort, 0, 0, 2)
    ZEND_ARG_INFO(0, id)
    ZEND_ARG_INFO(0, sortspec)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(arginfo_yaz_itemorder, 0, 0, 2)
    ZEND_ARG_INFO(0, id)
    ZEND_ARG_INFO(0, package)
ZEND_END_ARG_INFO();

#define arginfo_yaz_ccl_conf      arginfo_yaz_itemorder

#define arginfo_yaz_cql_conf      arginfo_yaz_itemorder

ZEND_BEGIN_ARG_INFO_EX(arginfo_yaz_es, 0, 0, 3)
    ZEND_ARG_INFO(0, id)
    ZEND_ARG_INFO(0, type)
    ZEND_ARG_INFO(0, package)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(arginfo_yaz_scan, 0, 0, 3)
    ZEND_ARG_INFO(0, id)
    ZEND_ARG_INFO(0, type)
    ZEND_ARG_INFO(0, query)
    ZEND_ARG_INFO(0, flags)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(arginfo_yaz_scan_result, 0, 0, 2)
    ZEND_ARG_INFO(0, id)
    ZEND_ARG_INFO(1, options)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(arginfo_yaz_ccl_parse, 0, 0, 3)
    ZEND_ARG_INFO(0, id)
    ZEND_ARG_INFO(0, query)
    ZEND_ARG_INFO(1, result)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(arginfo_yaz_cql_parse, 0, 0, 4)
    ZEND_ARG_INFO(0, id)
    ZEND_ARG_INFO(0, cql)
    ZEND_ARG_INFO(1, result)
    ZEND_ARG_INFO(0, rev)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(arginfo_databases, 0, 0, 2)
    ZEND_ARG_INFO(0, id)
    ZEND_ARG_INFO(0, package)
ZEND_END_ARG_INFO();


zend_function_entry yaz_functions [] = {
	PHP_FE(yaz_connect, arginfo_yaz_connect)
	PHP_FE(yaz_close, arginfo_yaz_close)
	PHP_FE(yaz_search, arginfo_yaz_search)
	PHP_FE(yaz_wait, arginfo_yaz_wait)
	PHP_FE(yaz_errno, arginfo_yaz_errno)
	PHP_FE(yaz_error, arginfo_yaz_error)
	PHP_FE(yaz_addinfo, arginfo_yaz_addinfo)
	PHP_FE(yaz_hits, arginfo_yaz_hits)
	PHP_FE(yaz_record, arginfo_yaz_record)
	PHP_FE(yaz_syntax, arginfo_yaz_syntax)
	PHP_FE(yaz_element, arginfo_yaz_element)
	PHP_FE(yaz_range, arginfo_yaz_range)
	PHP_FE(yaz_itemorder, arginfo_yaz_itemorder)
	PHP_FE(yaz_es_result, arginfo_yaz_es_result)
	PHP_FE(yaz_scan, arginfo_yaz_scan)
	PHP_FE(yaz_scan_result, arginfo_yaz_scan_result)
	PHP_FE(yaz_present, arginfo_yaz_present)
	PHP_FE(yaz_ccl_conf, arginfo_yaz_ccl_conf)
	PHP_FE(yaz_ccl_parse, arginfo_yaz_ccl_parse)
#if YAZ_VERSIONL >= 0x050100
	PHP_FE(yaz_cql_parse, arginfo_yaz_cql_parse)
	PHP_FE(yaz_cql_conf, arginfo_yaz_cql_conf)
#endif
	PHP_FE(yaz_database, arginfo_databases)
	PHP_FE(yaz_sort, arginfo_yaz_sort)
	PHP_FE(yaz_schema, arginfo_yaz_schema)
	PHP_FE(yaz_set_option, arginfo_yaz_set_option)
	PHP_FE(yaz_get_option, arginfo_yaz_get_option)
	PHP_FE(yaz_es, arginfo_yaz_es)
#ifdef PHP_FE_END
	PHP_FE_END
#else
	{NULL, NULL, NULL}
#endif
};

static void get_assoc(INTERNAL_FUNCTION_PARAMETERS, zval *id, Yaz_Association *assocp)
{
	Yaz_Association *as = 0;

	*assocp = 0;
#ifdef ZTS
	tsrm_mutex_lock(yaz_mutex);
#endif

#if PHP_API_VERSION >= 20150101
	as = (Yaz_Association *) zend_fetch_resource(Z_RES_P(id),
												 "YAZ link", le_link);
#else
	ZEND_FETCH_RESOURCE(as, Yaz_Association *, &id, -1, "YAZ link", le_link);
#endif
	if (as && *as && (*as)->order == YAZSG(assoc_seq) && (*as)->in_use) {
		*assocp = *as;
	} else {
#ifdef ZTS
		tsrm_mutex_unlock(yaz_mutex);
#endif
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid YAZ handle");
	}
}

static void release_assoc(Yaz_Association assoc)
{
#ifdef ZTS
	if (assoc) {
		tsrm_mutex_unlock(yaz_mutex);
	}
#endif
}

static const char *array_lookup_string(HashTable *ht, const char *idx)
{
#if PHP_API_VERSION >= 20150101
	zval *zv;

	zend_string *str = zend_string_init(idx, strlen(idx), 0);
	zv = zend_hash_find(ht, str);
	zend_string_release(str);
	if (zv) {
		convert_to_string(zv);
		if (Z_TYPE_P(zv) == IS_STRING)
			return ZSTR_VAL(zv->value.str);
	}
#else
	zval **pvalue;

	if (ht && zend_hash_find(ht, (char *) idx, strlen(idx) + 1, (void **) &pvalue) == SUCCESS) {
		SEPARATE_ZVAL(pvalue);
		convert_to_string(*pvalue);
		return (*pvalue)->value.str.val;
	}
#endif
	return 0;
}

static zend_long *array_lookup_long(HashTable *ht, const char *idx)
{
#if PHP_API_VERSION >= 20150101
	zval *zv;

	zend_string *str = zend_string_init(idx, strlen(idx), 0);
	zv = zend_hash_find(ht, str);
	zend_string_release(str);
	if (zv) {
		convert_to_long(zv);
		if (Z_TYPE_P(zv) == IS_LONG)
			return &zv->value.lval;
	}
#else
	zval **pvalue;

	if (ht && zend_hash_find(ht, (char *) idx, strlen(idx) + 1, (void **) &pvalue) == SUCCESS) {
		SEPARATE_ZVAL(pvalue);
		convert_to_long(*pvalue);
		return &(*pvalue)->value.lval;
	}
#endif
	return 0;
}

static long *array_lookup_bool(HashTable *ht, const char *idx)
{
#if PHP_API_VERSION >= 20150101
	zval *zv;
	static long l_true = 1;
	static long l_false = 0;

	zend_string *str = zend_string_init(idx, strlen(idx), 0);
	zv = zend_hash_find(ht, str);
	zend_string_release(str);
	if (zv) {
		convert_to_boolean(zv);
		if (Z_TYPE_P(zv) == IS_TRUE)
			return &l_true;
		if (Z_TYPE_P(zv) == IS_FALSE)
			return &l_false;
	}
#else
	zval **pvalue;

	if (ht && zend_hash_find(ht, (char *) idx, strlen(idx) + 1, (void **) &pvalue) == SUCCESS) {
		SEPARATE_ZVAL(pvalue);
		convert_to_boolean(*pvalue);
		return &(*pvalue)->value.lval;
	}
#endif
	return 0;
}

static const char *option_get(Yaz_Association as, const char *name)
{
	if (!as) {
		return 0;
	}
	return ZOOM_connection_option_get(as->zoom_conn, name);
}

static int option_get_int(Yaz_Association as, const char *name, int def)
{
	const char *v;

	v = ZOOM_connection_option_get(as->zoom_conn, name);

	if (!v) {
		return def;
	}

	return atoi(v);
}

static void option_set(Yaz_Association as, const char *name, const char *value)
{
	if (as && value) {
		ZOOM_connection_option_set(as->zoom_conn, name, value);
	}
}

static void option_set_int(Yaz_Association as, const char *name, int v)
{
	if (as) {
		char s[30];

		sprintf(s, "%d", v);
		ZOOM_connection_option_set(as->zoom_conn, name, s);
	}
}

static int strcmp_null(const char *s1, const char *s2)
{
	if (s1 == 0 && s2 == 0) {
		return 0;
	}
	if (s1 == 0 || s2 == 0) {
		return -1;
	}
	return strcmp(s1, s2);
}

/* {{{ proto resource yaz_connect(string zurl [, array options])
   Create target with given zurl. Returns positive id if successful. */
PHP_FUNCTION(yaz_connect)
{
	int i;
	char *cp;
	char *zurl_str;
	zend_size_t zurl_len;
	const char *sru_str = 0, *sru_version_str = 0;
	const char *user_str = 0, *group_str = 0, *pass_str = 0;
	const char *cookie_str = 0, *proxy_str = 0;
	const char *charset_str = 0;
	const char *client_IP = 0;
	const char *otherInfo[3];
	const char *maximumRecordSize = 0;
	const char *preferredMessageSize = 0;
	int persistent = 1;
	int piggyback = 1;
	Yaz_Association as = 0;
	int max_links = YAZSG(max_links);

	otherInfo[0] = otherInfo[1] = otherInfo[2] = 0;

	if (ZEND_NUM_ARGS() == 1) {
		if (zend_parse_parameters(1 TSRMLS_CC, "s", &zurl_str, &zurl_len)
			== FAILURE) {
			WRONG_PARAM_COUNT;
		}
	} else if (ZEND_NUM_ARGS() == 2) {
		zval *user = 0;
		if (zend_parse_parameters(2 TSRMLS_CC, "sz", &zurl_str, &zurl_len,
								  &user) == FAILURE) {
			WRONG_PARAM_COUNT;
		}

		if (Z_TYPE_P(user) == IS_ARRAY) {
			long *persistent_val;
			long *piggyback_val;
#if PHP_API_VERSION >= 20150101
			HashTable *ht = Z_ARRVAL_P(user);
#else
			HashTable *ht = Z_ARRVAL_PP(&user);
#endif

			sru_str = array_lookup_string(ht, "sru");
			sru_version_str = array_lookup_string(ht, "sru_version");
			user_str = array_lookup_string(ht, "user");
			group_str = array_lookup_string(ht, "group");
			pass_str = array_lookup_string(ht, "password");
			cookie_str = array_lookup_string(ht, "cookie");
			proxy_str = array_lookup_string(ht, "proxy");
			charset_str = array_lookup_string(ht, "charset");
			persistent_val = array_lookup_bool(ht, "persistent");
			if (persistent_val) {
				persistent = *persistent_val;
			}
			piggyback_val = array_lookup_bool(ht, "piggyback");
			if (piggyback_val) {
				piggyback = *piggyback_val;
			}
			maximumRecordSize =
				array_lookup_string(ht, "maximumRecordSize");
			preferredMessageSize =
				array_lookup_string(ht, "preferredMessageSize");
			otherInfo[0] = array_lookup_string(ht, "otherInfo0");
			otherInfo[1] = array_lookup_string(ht, "otherInfo1");
			otherInfo[2] = array_lookup_string(ht, "otherInfo2");
		} else if (Z_TYPE_P(user) == IS_STRING) {
#if PHP_API_VERSION >= 20150110
			user_str = ZSTR_VAL(user->value.str);
#else
			user_str = user->value.str.val;
#endif
		}
	} else {
		WRONG_PARAM_COUNT;
	}
	for (cp = zurl_str; *cp && strchr("\t\n ", *cp); cp++);
	if (!*cp) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Empty zurl");
		RETURN_LONG(0);
	}
	/* see if we have it already ... */
#ifdef ZTS
	tsrm_mutex_lock(yaz_mutex);
#endif
	for (i = 0; i < max_links; i++) {
		as = shared_associations[i];
		if (persistent && as && !as->in_use &&
			!strcmp_null(option_get(as, "host"), zurl_str) &&
			!strcmp_null(option_get(as, "proxy"), proxy_str) &&
			!strcmp_null(option_get(as, "sru"), sru_str) &&
			!strcmp_null(option_get(as, "sru_version"), sru_version_str) &&
			!strcmp_null(option_get(as, "user"), user_str) &&
			!strcmp_null(option_get(as, "group"), group_str) &&
			!strcmp_null(option_get(as, "pass"), pass_str) &&
			!strcmp_null(option_get(as, "cookie"), cookie_str) &&
			!strcmp_null(option_get(as, "charset"), charset_str))
			break;
	}
	if (i == max_links) {
		/* we didn't have it (or already in use) */
		int i0 = -1;
		int min_order = 2000000000;

		/* find completely free slot or the oldest one */
		for (i = 0; i < max_links && shared_associations[i]; i++) {
			as = shared_associations[i];
			if (persistent && !as->in_use && as->order < min_order) {
				min_order = as->order;
				i0 = i;
			}
		}

		if (i == max_links) {
			i = i0;
			if (i == -1) {
				char msg[80];
#ifdef ZTS
				tsrm_mutex_unlock(yaz_mutex);
#endif
				sprintf(msg, "No YAZ handles available. max_links=%d",
						max_links);
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
								 "No YAZ handles available. max_links=%ld",
								 (long) max_links);
				RETURN_LONG(0);			 /* no free slot */
			} else {					 /* "best" free slot */
				yaz_association_destroy(shared_associations[i]);
			}
		}
		shared_associations[i] = as = yaz_association_mk();

		option_set(as, "proxy", proxy_str);
		option_set(as, "sru", sru_str);
		option_set(as, "sru_version", sru_version_str);
		option_set(as, "user", user_str);
		option_set(as, "group", group_str);
		option_set(as, "pass", pass_str);
		option_set(as, "cookie", cookie_str);
		option_set(as, "charset", charset_str);
	}
	if (maximumRecordSize)
		option_set(as, "maximumRecordSize", maximumRecordSize);
	if (preferredMessageSize)
		option_set(as, "preferredMessageSize", preferredMessageSize);
	option_set(as, "otherInfo0", otherInfo[0]);
	option_set(as, "otherInfo1", otherInfo[1]);
	option_set(as, "otherInfo2", otherInfo[2]);
	option_set(as, "clientIP", client_IP);
	option_set(as, "piggyback", piggyback ? "1" : "0");
	option_set_int(as, "start", 0);
	option_set_int(as, "count", 0);
	ZOOM_connection_connect(as->zoom_conn, zurl_str, 0);
	as->in_use = 1;
	as->persistent = persistent;
	as->order = YAZSG(assoc_seq);
	as->time_stamp = time(0);

	if (as->zoom_set)
	{
		ZOOM_resultset_destroy(as->zoom_set);
		as->zoom_set = 0;
	}
#ifdef ZTS
	tsrm_mutex_unlock(yaz_mutex);
#endif

#if PHP_API_VERSION >= 20150101
    RETURN_RES(zend_register_resource(&shared_associations[i], le_link));
	as->zval_resource = Z_RES_P(return_value);
#else
	ZEND_REGISTER_RESOURCE(return_value, &shared_associations[i], le_link);
	as->zval_resource = Z_LVAL_P(return_value);
#endif
}
/* }}} */

/* {{{ proto bool yaz_close(resource id)
   Destroy and close target */
PHP_FUNCTION(yaz_close)
{
	Yaz_Association p;
	zval *id;

	if (ZEND_NUM_ARGS() != 1 || zend_parse_parameters(1 TSRMLS_CC, "z", &id)
		== FAILURE) {
		WRONG_PARAM_COUNT;
	}
	get_assoc(INTERNAL_FUNCTION_PARAM_PASSTHRU, id, &p);
	if (!p) {
		RETURN_FALSE;
	}
	release_assoc(p);
#if PHP_API_VERSION >= 20150101
	zend_list_close(Z_RES_P(id));
#else
	zend_list_delete(id->value.lval);
#endif
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool yaz_search(resource id, string type, string query)
   Specify query of type for search - returns true if successful */
PHP_FUNCTION(yaz_search)
{
	char *query_str, *type_str;
	zend_size_t query_len, type_len;
	zval *id;
	Yaz_Association p;

	if (ZEND_NUM_ARGS() != 3 ||
        zend_parse_parameters(3 TSRMLS_CC, "zss", &id,
							  &type_str, &type_len,
							  &query_str, &query_len) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	get_assoc(INTERNAL_FUNCTION_PARAM_PASSTHRU, id, &p);
	if (!p) {
		RETURN_FALSE;
	}

	ZOOM_resultset_destroy(p->zoom_set);
	p->zoom_set = 0;

	RETVAL_FALSE;

	if (!strcmp(type_str, "rpn")) {
		ZOOM_query q = ZOOM_query_create();
		if (ZOOM_query_prefix(q, query_str) == 0)
		{
			if (p->sort_criteria) {
				ZOOM_query_sortby(q, p->sort_criteria);
			}
			xfree(p->sort_criteria);
			p->sort_criteria = 0;
			p->zoom_set = ZOOM_connection_search(p->zoom_conn, q);
			RETVAL_TRUE;
		}
		ZOOM_query_destroy(q);
	}
	else if (!strcmp(type_str, "cql")) {
		ZOOM_query q = ZOOM_query_create();
		if (ZOOM_query_cql(q, query_str) == 0)
		{
			if (p->sort_criteria) {
				ZOOM_query_sortby(q, p->sort_criteria);
			}
			xfree(p->sort_criteria);
			p->sort_criteria = 0;
			p->zoom_set = ZOOM_connection_search(p->zoom_conn, q);
			RETVAL_TRUE;
		}
		ZOOM_query_destroy(q);
	}
	else
	{
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "Invalid query type %s", type_str);
	}
	release_assoc(p);
}
/* }}} */

/* {{{ proto bool yaz_present(resource id)
   Retrieve records */
PHP_FUNCTION(yaz_present)
{
	zval *id;
	Yaz_Association p;

	if (ZEND_NUM_ARGS() != 1 || zend_parse_parameters(1 TSRMLS_CC, "z", &id)
		== FAILURE) {
		WRONG_PARAM_COUNT;
	}
	get_assoc(INTERNAL_FUNCTION_PARAM_PASSTHRU, id, &p);
	if (!p) {
		RETURN_FALSE;
	}

	if (p->zoom_set) {
		size_t start = option_get_int(p, "start", 0);
		size_t count = option_get_int(p, "count", 0);
		if (count > 0) {
			ZOOM_resultset_records(p->zoom_set, 0 /* recs */, start, count);
		}
	}
	release_assoc(p);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool yaz_wait([array options])
   Process events. */
PHP_FUNCTION(yaz_wait)
{
	zval *pval_options = 0;
	int event_mode = 0;
	int no = 0;
	ZOOM_connection conn_ar[MAX_ASSOC];
	Yaz_Association conn_as[MAX_ASSOC];
	int i, timeout = 15;

	if (ZEND_NUM_ARGS() == 1) {
		zend_long *val = 0;
		long *event_bool = 0;
		HashTable *options_ht = 0;
		if (zend_parse_parameters(1 TSRMLS_CC, "a/", &pval_options) ==
			FAILURE) {
			WRONG_PARAM_COUNT;
		}
#if PHP_API_VERSION >= 20150101
		options_ht = Z_ARRVAL_P(pval_options);
#else
		options_ht = Z_ARRVAL_PP(&pval_options);
#endif
		val = array_lookup_long(options_ht, "timeout");
		if (val) {
			timeout = *val;
		}
		event_bool = array_lookup_bool(options_ht, "event");
		if (event_bool && *event_bool)
			event_mode = 1;
	}
	else if (ZEND_NUM_ARGS() > 1) {
		WRONG_PARAM_COUNT;
	}
#ifdef ZTS
	tsrm_mutex_lock(yaz_mutex);
#endif
	for (i = 0; i<YAZSG(max_links); i++) {
		Yaz_Association p = shared_associations[i];
		if (p && p->order == YAZSG(assoc_seq)) {
			char str[20];

			sprintf(str, "%d", timeout);
			ZOOM_connection_option_set(p->zoom_conn, "timeout", str);
			conn_as[no] = p;
			conn_ar[no++] = p->zoom_conn;
		}
	}
#ifdef ZTS
	tsrm_mutex_unlock(yaz_mutex);
#endif
	if (event_mode) {
		long ev = ZOOM_event(no, conn_ar);
		if (ev <= 0) {
			RETURN_FALSE;
		} else {
			Yaz_Association p = conn_as[ev-1];
			int event_code = ZOOM_connection_last_event(p->zoom_conn);

			if (pval_options) {
				add_assoc_long(pval_options, "connid", ev);
				add_assoc_long(pval_options, "eventcode", event_code);
			}
#if PHP_API_VERSION >= 20150101
			Z_RES_P(return_value) = p->zval_resource;
#else
			Z_TYPE_P(return_value) = IS_RESOURCE;
			zend_list_addref(p->zval_resource);
			Z_LVAL_P(return_value) = p->zval_resource;
#endif
			return;
		}
	}

	if (no) {
		while (ZOOM_event(no, conn_ar))
			;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto int yaz_errno(resource id)
   Return last error number (>0 for bib-1 diagnostic, <0 for other error, 0 for no error */
PHP_FUNCTION(yaz_errno)
{
	zval *id;
	Yaz_Association p;

	if (ZEND_NUM_ARGS() != 1 || zend_parse_parameters(1 TSRMLS_CC, "z", &id)
		== FAILURE) {
		WRONG_PARAM_COUNT;
	}
	get_assoc(INTERNAL_FUNCTION_PARAM_PASSTHRU, id, &p);
	if (!p) {
		RETURN_LONG(0);
	}
	RETVAL_LONG(ZOOM_connection_errcode(p->zoom_conn));
	release_assoc(p);
}
/* }}} */

/* {{{ proto string yaz_error(resource id)
   Return last error message */
PHP_FUNCTION(yaz_error)
{
	zval *id;
	Yaz_Association p;

	if (ZEND_NUM_ARGS() != 1 || zend_parse_parameters(1 TSRMLS_CC, "z", &id)
		== FAILURE) {
		WRONG_PARAM_COUNT;
	}
	get_assoc(INTERNAL_FUNCTION_PARAM_PASSTHRU, id, &p);
	if (p) {
		int code = ZOOM_connection_errcode(p->zoom_conn);
		const char *msg = ZOOM_connection_errmsg(p->zoom_conn);

		if (!code) {
			msg = "";
		}
#if PHP_API_VERSION >= 20150101
		zend_string *str = zend_string_init(msg, strlen(msg), 0);
		RETURN_STR(str);
#else
		return_value->value.str.len = strlen(msg);
		return_value->value.str.val = estrndup(msg, return_value->value.str.len);
		return_value->type = IS_STRING;
#endif
	}
	release_assoc(p);
}
/* }}} */

/* {{{ proto string yaz_addinfo(resource id)
   Return additional info for last error (empty string if none) */
PHP_FUNCTION(yaz_addinfo)
{
	zval *id;
	Yaz_Association p;

	if (ZEND_NUM_ARGS() != 1 || zend_parse_parameters(1 TSRMLS_CC, "z", &id)
		== FAILURE) {
		WRONG_PARAM_COUNT;
	}
	get_assoc(INTERNAL_FUNCTION_PARAM_PASSTHRU, id, &p);
	if (p) {
		const char *addinfo = ZOOM_connection_addinfo(p->zoom_conn);
#if PHP_API_VERSION >= 20150101
		zend_string *str = zend_string_init(addinfo, strlen(addinfo), 0);
		RETURN_STR(str);
#else
		return_value->value.str.len = strlen(addinfo);
		return_value->value.str.val = estrndup(addinfo, return_value->value.str.len);
		return_value->type = IS_STRING;
#endif
	}
	release_assoc(p);
}
/* }}} */

/* {{{ proto int yaz_hits(resource id [, array searchresult])
   Return number of hits (result count) for last search */
PHP_FUNCTION(yaz_hits)
{
	zval *id, *searchresult = 0;
	Yaz_Association p;

	if (ZEND_NUM_ARGS() == 1) {
		if (zend_parse_parameters(1 TSRMLS_CC, "z", &id) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
	} else if (ZEND_NUM_ARGS() == 2) {
		if (zend_parse_parameters(2 TSRMLS_CC, "zz/", &id, &searchresult)
			== FAILURE) {
			WRONG_PARAM_COUNT;
		}
	} else {
		WRONG_PARAM_COUNT;
	}
	if (searchresult) {
		array_init(searchresult);
	}
	get_assoc(INTERNAL_FUNCTION_PARAM_PASSTHRU, id, &p);

	if (p && p->zoom_set) {
		RETVAL_LONG(ZOOM_resultset_size(p->zoom_set));
		if (searchresult)
		{
			const char *str =
				ZOOM_resultset_option_get(p->zoom_set, "resultSetStatus");
			if (str)
				ADD_ASSOC_STRING(searchresult, "resultSetStatus",
								 (char *) str);
		}
		if (searchresult)
		{
			const char *sz_str =
				ZOOM_resultset_option_get(p->zoom_set, "searchresult.size");
			int i, sz = 0;

			if (sz_str && *sz_str)
				sz = atoi(sz_str);
			for (i = 0; i<sz; i++)
			{
				char opt_name[80];
				const char *opt_value;
				zval *zval_element;
#if PHP_API_VERSION >= 20150101
				zval zval_element0;
				zval_element = &zval_element0;
#else
				MAKE_STD_ZVAL(zval_element);
#endif
				array_init(zval_element);
				add_next_index_zval(searchresult, zval_element);

				sprintf(opt_name, "searchresult.%d.id", i);
				opt_value = ZOOM_resultset_option_get(p->zoom_set, opt_name);
				if (opt_value)
					ADD_ASSOC_STRING(zval_element, "id", (char *) opt_value);

				sprintf(opt_name, "searchresult.%d.count", i);
				opt_value = ZOOM_resultset_option_get(p->zoom_set, opt_name);
				if (opt_value)
					add_assoc_long(zval_element, "count", atoi(opt_value));

				sprintf(opt_name, "searchresult.%d.subquery.term", i);
				opt_value = ZOOM_resultset_option_get(p->zoom_set, opt_name);
				if (opt_value)
					ADD_ASSOC_STRING(zval_element, "subquery.term",
									 (char *) opt_value);

				sprintf(opt_name, "searchresult.%d.interpretation.term", i);
				opt_value = ZOOM_resultset_option_get(p->zoom_set, opt_name);
				if (opt_value)
					ADD_ASSOC_STRING(zval_element, "interpretation.term",
									 (char *) opt_value);

				sprintf(opt_name, "searchresult.%d.recommendation.term", i);
				opt_value = ZOOM_resultset_option_get(p->zoom_set, opt_name);
				if (opt_value)
					ADD_ASSOC_STRING(zval_element, "recommendation.term",
									 (char *) opt_value);
			}
		}

	} else {
		RETVAL_LONG(0);
	}
	release_assoc(p);
}
/* }}} */

static Z_GenericRecord *marc_to_grs1(const char *buf, ODR o)
{
	int entry_p;
	int record_length;
	int indicator_length;
	int identifier_length;
	int base_address;
	int length_data_entry;
	int length_starting;
	int max_elements = 256;
	Z_GenericRecord *r = odr_malloc(o, sizeof(*r));
	r->elements = odr_malloc(o, sizeof(*r->elements) * max_elements);
	r->num_elements = 0;

	record_length = atoi_n(buf, 5);
	if (record_length < 25) {
		return 0;
	}
	indicator_length = atoi_n(buf + 10, 1);
	identifier_length = atoi_n(buf + 11, 1);
	base_address = atoi_n(buf + 12, 5);

	length_data_entry = atoi_n(buf + 20, 1);
	length_starting = atoi_n(buf + 21, 1);

	for (entry_p = 24; buf[entry_p] != ISO2709_FS; ) {
		entry_p += 3 + length_data_entry + length_starting;
		if (entry_p >= record_length) {
			return 0;
		}
	}
	if (1)
	{
		Z_TaggedElement *tag;
		tag = r->elements[r->num_elements++] = odr_malloc(o, sizeof(*tag));
		tag->tagType = odr_malloc(o, sizeof(*tag->tagType));
		*tag->tagType = 3;
		tag->tagOccurrence = 0;
		tag->metaData = 0;
		tag->appliedVariant = 0;
		tag->tagValue = odr_malloc(o, sizeof(*tag->tagValue));
		tag->tagValue->which = Z_StringOrNumeric_string;
		tag->tagValue->u.string = odr_strdup(o, "leader");

		tag->content = odr_malloc(o, sizeof(*tag->content));
		tag->content->which = Z_ElementData_string;
		tag->content->u.string = odr_strdupn(o, buf, 24);
	}
	base_address = entry_p + 1;
	for (entry_p = 24; buf[entry_p] != ISO2709_FS; ) {
		Z_TaggedElement *tag;
		int data_length;
		int data_offset;
		int end_offset;
		int i;
		char tag_str[4];
		int identifier_flag = 1;

		memcpy(tag_str, buf+entry_p, 3);
		entry_p += 3;
		tag_str[3] = '\0';

		if ((r->num_elements + 1) >= max_elements) {
			Z_TaggedElement **tmp = r->elements;

			/* double array space, throw away old buffer (nibble memory) */
			r->elements = odr_malloc(o, sizeof(*r->elements) * (max_elements *= 2));
			memcpy(r->elements, tmp, r->num_elements * sizeof(*tmp));
		}
		tag = r->elements[r->num_elements++] = odr_malloc(o, sizeof(*tag));
		tag->tagType = odr_malloc(o, sizeof(*tag->tagType));
		*tag->tagType = 3;
		tag->tagOccurrence = 0;
		tag->metaData = 0;
		tag->appliedVariant = 0;
		tag->tagValue = odr_malloc(o, sizeof(*tag->tagValue));
		tag->tagValue->which = Z_StringOrNumeric_string;
		tag->tagValue->u.string = odr_strdup(o, tag_str);

		tag->content = odr_malloc(o, sizeof(*tag->content));
		tag->content->which = Z_ElementData_subtree;

		tag->content->u.subtree = odr_malloc(o, sizeof(*tag->content->u.subtree));
		tag->content->u.subtree->elements = odr_malloc(o, sizeof(*r->elements));
		tag->content->u.subtree->num_elements = 1;

		tag = tag->content->u.subtree->elements[0] = odr_malloc(o, sizeof(**tag->content->u.subtree->elements));

		tag->tagType = odr_malloc(o, sizeof(*tag->tagType));
		*tag->tagType = 3;
		tag->tagOccurrence = 0;
		tag->metaData = 0;
		tag->appliedVariant = 0;
		tag->tagValue = odr_malloc(o, sizeof(*tag->tagValue));
		tag->tagValue->which = Z_StringOrNumeric_string;
		tag->content = odr_malloc(o, sizeof(*tag->content));

		data_length = atoi_n(buf + entry_p, length_data_entry);
		entry_p += length_data_entry;
		data_offset = atoi_n(buf + entry_p, length_starting);
		entry_p += length_starting;
		i = data_offset + base_address;
		end_offset = i + data_length - 1;

		if (indicator_length > 0 && indicator_length < 5) {
			if (buf[i + indicator_length] != ISO2709_IDFS) {
				identifier_flag = 0;
			}
		} else if (!memcmp(tag_str, "00", 2)) {
			identifier_flag = 0;
		}

		if (identifier_flag && indicator_length) {
			/* indicator */
			tag->tagValue->u.string = odr_malloc(o, indicator_length + 1);
			memcpy(tag->tagValue->u.string, buf + i, indicator_length);
			tag->tagValue->u.string[indicator_length] = '\0';
			i += indicator_length;

			tag->content->which = Z_ElementData_subtree;

			tag->content->u.subtree = odr_malloc(o, sizeof(*tag->content->u.subtree));
			tag->content->u.subtree->elements = odr_malloc(o, 256 * sizeof(*r->elements));
			tag->content->u.subtree->num_elements = 0;

			while (buf[i] != ISO2709_RS && buf[i] != ISO2709_FS && i < end_offset) {
				int i0;
				/* prepare tag */
				Z_TaggedElement *parent_tag = tag;
				Z_TaggedElement *tag = odr_malloc(o, sizeof(*tag));

				if (parent_tag->content->u.subtree->num_elements < 256) {
					parent_tag->content->u.subtree->elements[
					parent_tag->content->u.subtree->num_elements++] = tag;
				}

				tag->tagType = odr_malloc(o, sizeof(*tag->tagType));
				*tag->tagType = 3;
				tag->tagOccurrence = 0;
				tag->metaData = 0;
				tag->appliedVariant = 0;
				tag->tagValue = odr_malloc(o, sizeof(*tag->tagValue));
				tag->tagValue->which = Z_StringOrNumeric_string;

				/* sub field */
				tag->tagValue->u.string = odr_malloc(o, identifier_length);
				memcpy(tag->tagValue->u.string, buf + i + 1, identifier_length - 1);
				tag->tagValue->u.string[identifier_length - 1] = '\0';
				i += identifier_length;

				/* data ... */
				tag->content = odr_malloc(o, sizeof(*tag->content));
				tag->content->which = Z_ElementData_string;

				i0 = i;
				while (	buf[i] != ISO2709_RS &&
						buf[i] != ISO2709_IDFS &&
						buf[i] != ISO2709_FS && i < end_offset) {
					i++;
				}

				tag->content->u.string = odr_malloc(o, i - i0 + 1);
				memcpy(tag->content->u.string, buf + i0, i - i0);
				tag->content->u.string[i - i0] = '\0';
			}
		} else {
			int i0 = i;

			tag->tagValue->u.string = "@";
			tag->content->which = Z_ElementData_string;

			while (buf[i] != ISO2709_RS && buf[i] != ISO2709_FS && i < end_offset) {
				i++;
			}
			tag->content->u.string = odr_malloc(o, i - i0 +1);
			memcpy(tag->content->u.string, buf + i0, i - i0);
			tag->content->u.string[i-i0] = '\0';
		}
	}
	return r;
}

struct cvt_handle {
	ODR odr;
	yaz_iconv_t cd;
	char *buf;
	int size;
};

static struct cvt_handle *cvt_open(const char *to, const char *from)
{
	ODR o = odr_createmem(ODR_ENCODE);

	struct cvt_handle *cvt = odr_malloc(o, sizeof(*cvt));
	cvt->odr = o;
	cvt->size = 10;
	cvt->buf = odr_malloc(o, cvt->size);
	cvt->cd = 0;
	if (to && from)
		cvt->cd = yaz_iconv_open(to, from);
	return cvt;
}

static void cvt_close(struct cvt_handle *cvt)
{
	if (cvt->cd)
		yaz_iconv_close(cvt->cd);
	odr_destroy(cvt->odr);
}

static const char *cvt_string(const char *input, struct cvt_handle *cvt)
{
	if (!cvt->cd)
		return input;
	while(1) {
		size_t inbytesleft = strlen(input);
		const char *inp = input;
		size_t outbytesleft = cvt->size - 1;
		char *outp = cvt->buf;
		size_t r = yaz_iconv(cvt->cd, (char**) &inp, &inbytesleft,
							 &outp, &outbytesleft);
		if (r == (size_t) (-1))	{
			int e = yaz_iconv_error(cvt->cd);
			if (e != YAZ_ICONV_E2BIG || cvt->size > 200000)
			{
				cvt->buf[0] = '\0';
				break;
			}
			cvt->size = cvt->size * 2 + 30;
			cvt->buf = (char*) odr_malloc(cvt->odr, cvt->size);
		} else {
			cvt->buf[outp - cvt->buf] = '\0';
			break;
		}
	}
	return cvt->buf;
}

static void retval_array3_grs1(zval *return_value, Z_GenericRecord *p,
							   struct cvt_handle *cvt)
{
	int i;
	struct tag_list {
		char *tag;
		zval *zval_list;
		struct tag_list *next;
	} *all_tags = 0;
	NMEM nmem = nmem_create();

	array_init(return_value);
	for (i = 0; i<p->num_elements; i++)
	{
		struct tag_list *tl;
		zval *zval_element;
		zval *zval_list;
#if PHP_API_VERSION >= 20150101
		zval zval_element0, zval_list0;
#endif
		Z_TaggedElement *e = p->elements[i];
		char tagstr[32], *tag = 0;

		if (e->tagValue->which == Z_StringOrNumeric_numeric)
		{
			sprintf(tagstr, ODR_INT_PRINTF, *e->tagValue->u.numeric);
			tag = tagstr;
		}
		else if (e->tagValue->which == Z_StringOrNumeric_string)
			tag = e->tagValue->u.string;

		if (!tag)
			continue;

		for (tl = all_tags; tl; tl = tl->next)
			if (!strcmp(tl->tag, tag))
				break;
		if (tl)
			zval_list = tl->zval_list;
		else
		{
#if PHP_API_VERSION >= 20150101
			zval_list = &zval_list0;
#else
			MAKE_STD_ZVAL(zval_list);
#endif
			array_init(zval_list);
			add_assoc_zval(return_value, tag, zval_list);

			tl = nmem_malloc(nmem, sizeof(*tl));
			tl->tag = nmem_strdup(nmem, tag);
			tl->zval_list = zval_list;
			tl->next = all_tags;
			all_tags = tl;
		}
#if PHP_API_VERSION >= 20150101
		zval_element = &zval_element0;
#else
		MAKE_STD_ZVAL(zval_element);
#endif
		array_init(zval_element);
		add_next_index_zval(zval_list, zval_element);
		if (e->content->which == Z_ElementData_subtree)
		{
			/* we have a subtree. Move to first child */
			Z_GenericRecord *sub = e->content->u.subtree;
			if (sub->num_elements >= 1)
				e = sub->elements[0];
			else
				e = 0;
		}
		if (e)
		{
			const char *tag = 0;
			if (e->tagValue->which == Z_StringOrNumeric_numeric)
			{
				sprintf(tagstr, ODR_INT_PRINTF, *e->tagValue->u.numeric);
				tag = tagstr;
			}
			else if (e->tagValue->which == Z_StringOrNumeric_string)
				tag = e->tagValue->u.string;
			if (tag && e->content->which == Z_ElementData_subtree)
			{
				/* Data field */
				Z_GenericRecord *sub = e->content->u.subtree;
				int i;
				for (i = 0; tag[i]; i++)
				{
					char ind_idx[5];
					char ind_val[2];

					sprintf(ind_idx, "ind%d", i+1);
					ind_val[0] = tag[i];
					ind_val[1] = '\0';

					ADD_ASSOC_STRING(zval_element, ind_idx, ind_val);
				}
				for (i = 0; i<sub->num_elements; i++)
				{
					Z_TaggedElement *e = sub->elements[i];
					const char *tag = 0;
					if (e->tagValue->which == Z_StringOrNumeric_numeric)
					{
						sprintf(tagstr, ODR_INT_PRINTF, *e->tagValue->u.numeric);
						tag = tagstr;
					}
					else if (e->tagValue->which == Z_StringOrNumeric_string)
						tag = e->tagValue->u.string;

					if (tag && e->content->which == Z_ElementData_string)
					{
						const char *v = cvt_string(e->content->u.string, cvt);
						ADD_ASSOC_STRING(zval_element, (char*) tag, (char*) v);
					}
				}
			}
			else if (tag && e->content->which == Z_ElementData_string)
			{
				/* Leader or control field */
				const char *tag = "control";
				const char *v = cvt_string(e->content->u.string, cvt);
				ADD_ASSOC_STRING(zval_element, (char*) tag, (char*) v);
			}
		}
	}
	nmem_destroy(nmem);
}

static void retval_array2_grs1(zval *return_value, Z_GenericRecord *p,
							   struct cvt_handle *cvt)
{
	int i;

	array_init(return_value);

	for (i = 0; i<p->num_elements; i++)
	{
		zval *zval_element;
		zval *zval_sub;
		Z_TaggedElement *e = p->elements[i];

#if PHP_API_VERSION >= 20150101
		zval zval_element0, zval_sub0;
		zval_element = &zval_element0;
		zval_sub = &zval_sub0;
#else
		MAKE_STD_ZVAL(zval_element);
#endif
		array_init(zval_element);

		if (e->tagType)
			add_assoc_long(zval_element, "tagType", (long) *e->tagType);

		if (e->tagValue->which == Z_StringOrNumeric_string)
			ADD_ASSOC_STRING(zval_element, "tag", e->tagValue->u.string);
		else if (e->tagValue->which == Z_StringOrNumeric_numeric)
			add_assoc_long(zval_element, "tag", (long) *e->tagValue->u.numeric);

		switch (e->content->which) {
		case Z_ElementData_string:
			if (1)
			{
				const char *v = cvt_string(e->content->u.string, cvt);
				ADD_ASSOC_STRING(zval_element, "content", (char*) v);
			}
			break;
		case Z_ElementData_numeric:
			add_assoc_long(zval_element, "content", (long) *e->content->u.numeric);
			break;
		case Z_ElementData_trueOrFalse:
			add_assoc_bool(zval_element, "content",*e->content->u.trueOrFalse);
			break;
		case Z_ElementData_subtree:
#if PHP_API_VERSION >= 20150101
#else
			MAKE_STD_ZVAL(zval_sub);
#endif
			retval_array2_grs1(zval_sub, e->content->u.subtree, cvt);
			add_assoc_zval(zval_element, "content", zval_sub);
		}
		add_next_index_zval(return_value, zval_element);
	}
}

static void retval_array1_grs1(zval *return_value, Z_GenericRecord *p,
							   struct cvt_handle *cvt)
{
	Z_GenericRecord *grs[20];
	int eno[20];
	int level = 0;

	array_init(return_value);
	eno[level] = 0;
	grs[level] = p;

	while (level >= 0) {
		zval *my_zval;
		Z_TaggedElement *e = 0;
		Z_GenericRecord *p = grs[level];
		int i;
		char tag[256];
		size_t taglen = 0;
#if PHP_API_VERSION >= 20150101
		zval my_zval0;
#endif
		if (eno[level] >= p->num_elements) {
			--level;
			if (level >= 0)
				eno[level]++;
			continue;
		}
		*tag = '\0';
		for (i = 0; i <= level; i++) {
			long tag_type = 3;
			e = grs[i]->elements[eno[i]];

			if (e->tagType) {
				tag_type = (long) *e->tagType;
			}
			taglen = strlen(tag);
			sprintf(tag + taglen, "(%ld,", tag_type);
			taglen = strlen(tag);

			if (e->tagValue->which == Z_StringOrNumeric_string) {
				size_t len = strlen(e->tagValue->u.string);

				memcpy(tag + taglen, e->tagValue->u.string, len);
				tag[taglen+len] = '\0';
			} else if (e->tagValue->which == Z_StringOrNumeric_numeric) {
				sprintf(tag + taglen, ODR_INT_PRINTF, *e->tagValue->u.numeric);
			}
			taglen = strlen(tag);
			strcpy(tag + taglen, ")");
		}

#if PHP_API_VERSION >= 20150101
		my_zval = &my_zval0;
#else
		ALLOC_ZVAL(my_zval);
#endif
		array_init(my_zval);
#if PHP_API_VERSION >= 20150101
#else
		INIT_PZVAL(my_zval);
#endif
		ADD_NEXT_INDEX_STRING(my_zval, tag);

		switch (e->content->which) {
			case Z_ElementData_string:
				if (1)
				{
					const char *v = cvt_string(e->content->u.string, cvt);
					ADD_NEXT_INDEX_STRING(my_zval, (char*) v);
				}
				break;
			case Z_ElementData_numeric:
				add_next_index_long(my_zval, (long) *e->content->u.numeric);
				break;
			case Z_ElementData_trueOrFalse:
				add_next_index_long(my_zval, *e->content->u.trueOrFalse);
				break;
			case Z_ElementData_subtree:
				if (level < 20)	{
					level++;
					grs[level] = e->content->u.subtree;
					eno[level] = -1;
				}
		}
#if PHP_API_VERSION >= 20150101
		zend_hash_next_index_insert(Z_ARRVAL_P(return_value), my_zval);
#else
		zend_hash_next_index_insert(return_value->value.ht, (void *) &my_zval, sizeof(zval *), NULL);
#endif
		eno[level]++;
	}
}

static void ext_grs1(zval *return_value, char type_args[][60],
					 ZOOM_record r,
					 void (*array_func)(zval *, Z_GenericRecord *,
										struct cvt_handle *))
{
	Z_External *ext = (Z_External *) ZOOM_record_get(r, "ext", 0);
	if (ext && ext->which == Z_External_OPAC)
		ext = ext->u.opac->bibliographicRecord;
	if (ext) {
		struct cvt_handle *cvt = 0;
		if (type_args[2][0])
			cvt = cvt_open(type_args[3], type_args[2]);
		else
			cvt = cvt_open(0, 0);

		if (ext->which == Z_External_grs1) {
			retval_array1_grs1(return_value, ext->u.grs1, cvt);
		} else if (ext->which == Z_External_octet) {
			Z_GenericRecord *rec = 0;
			if (yaz_oid_is_iso2709(ext->direct_reference))
			{
				char *buf = (char *) (ext->u.octet_aligned->buf);
				rec = marc_to_grs1(buf, cvt->odr);
			}
			if (rec) {
				(*array_func)(return_value, rec, cvt);
			}
		}
		cvt_close(cvt);
	}
}


/* {{{ proto string yaz_record(resource id, int pos, string type)
   Return record information at given result set position */
PHP_FUNCTION(yaz_record)
{
	zval *pval_id;
	Yaz_Association p;
	zend_long pos;
	char *type;
	zend_size_t type_len;

	if (ZEND_NUM_ARGS() != 3) {
		WRONG_PARAM_COUNT;
	}

	if (zend_parse_parameters(3 TSRMLS_CC, "zls", &pval_id, &pos,
							  &type, &type_len) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	get_assoc(INTERNAL_FUNCTION_PARAM_PASSTHRU, pval_id, &p);

	if (p && p->zoom_set) {
		ZOOM_record r;
		r = ZOOM_resultset_record(p->zoom_set, pos-1);
		if (r) {
			char *type_tmp = 0;
			char type_args[4][60];  /*  0; 1=2,3  (1 is assumed charset) */
			type_args[0][0] = 0;
			type_args[1][0] = 0;
			type_args[2][0] = 0;
			type_args[3][0] = 0;
			sscanf(type, "%59[^;];%59[^=]=%59[^,],%59[^,]", type_args[0],
			   type_args[1], type_args[2], type_args[3]);

			if (!strcmp(type_args[0], "string")) {
				type_tmp = xstrdup(type);
				strcpy(type_tmp, "render");
				strcat(type_tmp, type + 6);
				type = type_tmp;
			}
			if (!strcmp(type_args[0], "array") ||
				!strcmp(type_args[0], "array1"))
			{
				ext_grs1(return_value, type_args, r, retval_array1_grs1);
			} else if (!strcmp(type_args[0], "array2")) {
				ext_grs1(return_value, type_args, r, retval_array2_grs1);
			} else if (!strcmp(type_args[0], "array3")) {
				ext_grs1(return_value, type_args, r, retval_array3_grs1);
			} else {
				int rlen;
				const char *info = ZOOM_record_get(r, type, &rlen);
				if (info) {
#if PHP_API_VERSION >= 20150101
					zend_string *str =
						zend_string_init(info,
										 rlen > 0 ? rlen : strlen(info), 0);
					RETURN_STR(str);
#else
					return_value->value.str.len = (rlen > 0) ? rlen : 0;
					return_value->value.str.val =
						estrndup(info, return_value->value.str.len);
					return_value->type = IS_STRING;
#endif
				}
				else
				{
					php_error_docref(NULL TSRMLS_CC, E_WARNING,
									 "Bad yaz_record type %s - or unable "
									 "to return record with type given", type);
				}
			}
			xfree(type_tmp);
		}
	}
	release_assoc(p);
}
/* }}} */

/* {{{ proto void yaz_syntax(resource id, string syntax)
   Set record syntax for retrieval */
PHP_FUNCTION(yaz_syntax)
{
	zval *pval_id;
	const char *syntax;
	zend_size_t syntax_len;
	Yaz_Association p;

	if (ZEND_NUM_ARGS() != 2 ||
		zend_parse_parameters(2 TSRMLS_CC, "zs", &pval_id,
							  &syntax, &syntax_len) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	get_assoc(INTERNAL_FUNCTION_PARAM_PASSTHRU, pval_id, &p);
	option_set(p, "preferredRecordSyntax", syntax);
	release_assoc(p);
}
/* }}} */

/* {{{ proto void yaz_element(resource id, string elementsetname)
   Set Element-Set-Name for retrieval */
PHP_FUNCTION(yaz_element)
{
	zval *pval_id;
	const char *element;
	zend_size_t element_len;
	Yaz_Association p;

	if (ZEND_NUM_ARGS() != 2 ||
		zend_parse_parameters(2 TSRMLS_CC, "zs", &pval_id,
							  &element, &element_len) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	get_assoc(INTERNAL_FUNCTION_PARAM_PASSTHRU, pval_id, &p);

	option_set(p, "elementSetName", element);
	release_assoc(p);
}
/* }}} */

/* {{{ proto void yaz_schema(resource id, string schema)
   Set Schema for retrieval */
PHP_FUNCTION(yaz_schema)
{
	zval *pval_id;
	const char *schema;
	zend_size_t schema_len;
	Yaz_Association p;

	if (ZEND_NUM_ARGS() != 2 ||
		zend_parse_parameters(2 TSRMLS_CC, "zs", &pval_id,
							  &schema, &schema_len) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	get_assoc(INTERNAL_FUNCTION_PARAM_PASSTHRU, pval_id, &p);
	option_set(p, "schema", schema);
	release_assoc(p);
}
/* }}} */

/* {{{ proto void yaz_set_option(resource id, mixed options)
   Set Option(s) for connection */
PHP_FUNCTION(yaz_set_option)
{
	Yaz_Association p;

	if (ZEND_NUM_ARGS() == 2) {
		zval *pval_ar, *pval_id;
		if (zend_parse_parameters(2 TSRMLS_CC, "za",
								  &pval_id, &pval_ar) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		get_assoc(INTERNAL_FUNCTION_PARAM_PASSTHRU, pval_id, &p);
		if (p) {
			HashTable *ht;

#if PHP_API_VERSION >= 20150101
			zend_string *key;
			zval *val;

			ht = Z_ARRVAL_P(pval_ar);
			ZEND_HASH_FOREACH_STR_KEY_VAL(ht, key, val) {
				if (key) { //HASH_KEY_IS_STRING
					if (Z_TYPE_P(val) == IS_STRING)
						option_set(p, key->val, val->value.str->val);
				}
			}
			ZEND_HASH_FOREACH_END();
#else
			zval **ent;
			HashPosition pos;
			ht = Z_ARRVAL_PP(&pval_ar);
			for (zend_hash_internal_pointer_reset_ex(ht, &pos);
				zend_hash_get_current_data_ex(ht, (void**) &ent, &pos) == SUCCESS;
				zend_hash_move_forward_ex(ht, &pos)
			) {
				char *key;
				ulong idx;
#if PHP_API_VERSION > 20010101
				int type = zend_hash_get_current_key_ex(ht, &key, 0, &idx, 0, &pos);
#else
				int type = zend_hash_get_current_key_ex(ht, &key, 0, &idx, &pos);
#endif
				if (type != HASH_KEY_IS_STRING || Z_TYPE_PP(ent) != IS_STRING) {
					continue;
				}
				option_set(p, key, (*ent)->value.str.val);
			}
#endif
			release_assoc(p);
		}
	} else if (ZEND_NUM_ARGS() == 3) {
		zval *pval_id;
		char *name, *value;
		zend_size_t name_len, value_len;
		if (zend_parse_parameters(3 TSRMLS_CC, "zss",
								  &pval_id, &name, &name_len,
								  &value, &value_len) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		get_assoc (INTERNAL_FUNCTION_PARAM_PASSTHRU, pval_id, &p);
		option_set(p, name, value);
		release_assoc(p);
	} else {
		WRONG_PARAM_COUNT;
	}
}
/* }}} */

/* {{{ proto string yaz_get_option(resource id, string name)
   Set Option(s) for connection */
PHP_FUNCTION(yaz_get_option)
{
	zval *pval_id;
	char *name;
	zend_size_t name_len;
	Yaz_Association p;

	if (ZEND_NUM_ARGS() != 2 ||
		zend_parse_parameters(2 TSRMLS_CC, "zs", &pval_id, &name, &name_len)
		== FAILURE) {
		WRONG_PARAM_COUNT;
	}
	get_assoc(INTERNAL_FUNCTION_PARAM_PASSTHRU, pval_id, &p);
	if (p) {
		const char *v;
		v = option_get(p, name);
		if (!v) {
			v = "";
		}
#if PHP_API_VERSION >= 20150101
		zend_string *str = zend_string_init(v, strlen(v), 0);
		RETURN_STR(str);
#else
		return_value->value.str.len = strlen(v);
		return_value->value.str.val = estrndup(v, return_value->value.str.len);
		return_value->type = IS_STRING;
#endif
	} else {
		RETVAL_FALSE;
	}
	release_assoc(p);
}
/* }}} */

/* {{{ proto void yaz_range(resource id, int start, int number)
   Set result set start point and number of records to request */
PHP_FUNCTION(yaz_range)
{
	zval *pval_id;
	Yaz_Association p;
	zend_long start, number;

	if (ZEND_NUM_ARGS() != 3 ||
		zend_parse_parameters(3 TSRMLS_CC, "zll", &pval_id, &start, &number)
		== FAILURE) {
		WRONG_PARAM_COUNT;
	}

	get_assoc(INTERNAL_FUNCTION_PARAM_PASSTHRU, pval_id, &p);
	option_set_int(p, "start", start > 0 ? start - 1 : 0);
	option_set_int(p, "count", number);
	release_assoc(p);
}
/* }}} */

/* {{{ proto void yaz_sort(resource id, string sortspec)
   Set result set sorting criteria */
PHP_FUNCTION(yaz_sort)
{
	zval *pval_id;
	const char *criteria;
	zend_size_t criteria_len;
	Yaz_Association p;

	if (ZEND_NUM_ARGS() != 2 ||
		zend_parse_parameters(2 TSRMLS_CC, "zs", &pval_id, &criteria,
							  &criteria_len) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	get_assoc(INTERNAL_FUNCTION_PARAM_PASSTHRU, pval_id, &p);
	if (p) {
		xfree(p->sort_criteria);
		p->sort_criteria = xstrdup(criteria);
		if (p->zoom_set)
			ZOOM_resultset_sort(p->zoom_set, "yaz", criteria);
	}
	release_assoc(p);
}
/* }}} */

const char *ill_array_lookup(void *handle, const char *name)
{
	return array_lookup_string((HashTable *) handle, name);
}

/* {{{ proto void yaz_itemorder(resource id, array package)
   Sends Item Order request */
PHP_FUNCTION(yaz_itemorder)
{
	zval *pval_id, *pval_package;
	Yaz_Association p;

	if (ZEND_NUM_ARGS() != 2 ||
		zend_parse_parameters(2 TSRMLS_CC, "za", &pval_id, &pval_package) ==
		FAILURE) {
		WRONG_PARAM_COUNT;
	}
	get_assoc(INTERNAL_FUNCTION_PARAM_PASSTHRU, pval_id, &p);
	if (p) {
		ZOOM_options options = ZOOM_options_create();

		ZOOM_options_set_callback(options,
								  ill_array_lookup,
#if PHP_API_VERSION >= 20150101
								  Z_ARRVAL_P(pval_package)
#else
								  Z_ARRVAL_PP(&pval_package)
#endif
								  );
		ZOOM_package_destroy(p->zoom_package);
		p->zoom_package = ZOOM_connection_package(p->zoom_conn, options);
		ZOOM_package_send(p->zoom_package, "itemorder");
		ZOOM_options_set_callback(options, 0, 0);
		ZOOM_options_destroy(options);
	}
	release_assoc(p);
}
/* }}} */

/* {{{ proto void yaz_es(resource id, string type, array package)
   Sends Extended Services Request */
PHP_FUNCTION(yaz_es)
{
	zval *pval_id, *pval_package;
	const char *type;
	zend_size_t type_len;
	Yaz_Association p;

	if (ZEND_NUM_ARGS() != 3 ||
		zend_parse_parameters(3 TSRMLS_CC, "zsa", &pval_id,
							  &type, &type_len, &pval_package) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	get_assoc(INTERNAL_FUNCTION_PARAM_PASSTHRU, pval_id, &p);
	if (p) {
		ZOOM_options options = ZOOM_options_create();

		ZOOM_options_set_callback(options, ill_array_lookup,
#if PHP_API_VERSION >= 20150101
								  Z_ARRVAL_P(pval_package)
#else
								  Z_ARRVAL_PP(&pval_package)
#endif
								  );
		ZOOM_package_destroy(p->zoom_package);
		p->zoom_package = ZOOM_connection_package(p->zoom_conn, options);
		ZOOM_package_send(p->zoom_package, type);
		ZOOM_options_set_callback(options, 0, 0);
		ZOOM_options_destroy(options);
	}
	release_assoc(p);
}
/* }}} */

/* {{{ proto void yaz_scan(resource id, type, query [, flags])
   Sends Scan Request */
PHP_FUNCTION(yaz_scan)
{
	zval *pval_id, *pval_flags;
	char *type, *query;
	zend_size_t type_len, query_len;
	HashTable *flags_ht = 0;
	Yaz_Association p;

	if (ZEND_NUM_ARGS() == 3) {
		if (zend_parse_parameters(3 TSRMLS_CC, "zss",
								  &pval_id, &type, &type_len,
								  &query, &query_len) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
	} else if (ZEND_NUM_ARGS() == 4) {
		if (zend_parse_parameters(4 TSRMLS_CC, "zssa",
								  &pval_id, &type, &type_len,
								  &query, &query_len, &pval_flags) == FAILURE) {
			WRONG_PARAM_COUNT;
		}

		flags_ht =
#if PHP_API_VERSION >= 20150101
			Z_ARRVAL_P(pval_flags)
#else
			Z_ARRVAL_PP(&pval_flags)
#endif
			;
	} else {
		WRONG_PARAM_COUNT;
	}
	get_assoc(INTERNAL_FUNCTION_PARAM_PASSTHRU, pval_id, &p);
	ZOOM_scanset_destroy(p->zoom_scan);
	p->zoom_scan = 0;
	if (p) {
		if (flags_ht) {
			option_set(p, "number", array_lookup_string(flags_ht, "number"));
			option_set(p, "position", array_lookup_string(flags_ht, "position"));
			option_set(p, "stepSize", array_lookup_string(flags_ht, "stepsize"));
		}
		p->zoom_scan = ZOOM_connection_scan(p->zoom_conn, query);
	}
	release_assoc(p);
}
/* }}} */

/* {{{ proto array yaz_es_result(resource id)
   Inspects Extended Services Result */
PHP_FUNCTION(yaz_es_result)
{
	zval *pval_id;
	Yaz_Association p;

	if (ZEND_NUM_ARGS() != 1 || zend_parse_parameters(1 TSRMLS_CC, "z",
													  &pval_id)	== FAILURE) {
		WRONG_PARAM_COUNT;
	}

	array_init(return_value);

	get_assoc(INTERNAL_FUNCTION_PARAM_PASSTHRU, pval_id, &p);
	if (p && p->zoom_package) {
		const char *str = ZOOM_package_option_get(p->zoom_package,
												  "targetReference");

		if (str) {
			ADD_ASSOC_STRING(return_value, "targetReference", (char *) str);
		}
		str = ZOOM_package_option_get(p->zoom_package,
									  "xmlUpdateDoc");
		if (str) {
			ADD_ASSOC_STRING(return_value, "xmlUpdateDoc", (char *) str);
		}
	}
	release_assoc(p);
}
/* }}} */

/* {{{ proto array yaz_scan_result(resource id [, array options])
   Inspects Scan Result */
PHP_FUNCTION(yaz_scan_result)
{
	zval *pval_id, *pval_opt = 0;
	Yaz_Association p;

	if (ZEND_NUM_ARGS() == 2) {
		if (zend_parse_parameters(2 TSRMLS_CC, "zz/",
								  &pval_id, &pval_opt) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
	} else if (ZEND_NUM_ARGS() == 1) {
		if (zend_parse_parameters(1 TSRMLS_CC, "z",
								  &pval_id) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
	} else {
		WRONG_PARAM_COUNT;
	}

	array_init(return_value);
	if (pval_opt) {
			array_init(pval_opt);
	}

	get_assoc(INTERNAL_FUNCTION_PARAM_PASSTHRU, pval_id, &p);
	if (p && p->zoom_scan) {
		size_t pos = 0;
		/* ZOOM_scanset_term changed from YAZ 3 to YAZ 4 */
#if YAZ_VERSIONL >= 0x040000
		size_t occ, len;
#else
		int occ, len;
#endif
		size_t size = ZOOM_scanset_size(p->zoom_scan);

		for (pos = 0; pos < size; pos++) {
			const char *term = ZOOM_scanset_term(p->zoom_scan, pos, &occ, &len);
			zval *my_zval;
#if PHP_API_VERSION >= 20150101
			zval my_zval0;
			my_zval = &my_zval0;
#else
			ALLOC_ZVAL(my_zval);
#endif
			array_init(my_zval);
#if PHP_API_VERSION >= 20150101
#else
			INIT_PZVAL(my_zval);
#endif
			ADD_NEXT_INDEX_STRING(my_zval, "term");

			if (term) {
				ADD_NEXT_INDEX_STRINGl(my_zval, (char*) term, len);
			} else {
				ADD_NEXT_INDEX_STRING(my_zval, "?");
			}
			add_next_index_long(my_zval, occ);

			term = ZOOM_scanset_display_term(p->zoom_scan, pos, &occ, &len);

			if (term) {
				ADD_NEXT_INDEX_STRINGl(my_zval, (char*) term, len);
			} else {
				ADD_NEXT_INDEX_STRING(my_zval, "?");
			}

#if PHP_API_VERSION >= 20150101
               zend_hash_next_index_insert(Z_ARRVAL_P(return_value), my_zval);
#else
			   zend_hash_next_index_insert(return_value->value.ht, (void *) &my_zval, sizeof(zval *), NULL);
#endif
		}

		if (pval_opt) {
			const char *v;

			add_assoc_long(pval_opt, "number", size);

			v = ZOOM_scanset_option_get(p->zoom_scan, "stepSize");
			if (v) {
				add_assoc_long(pval_opt, "stepsize", atoi(v));
			}
			v = ZOOM_scanset_option_get(p->zoom_scan, "position");
			if (v) {
				add_assoc_long(pval_opt, "position", atoi(v));
			}
			v = ZOOM_scanset_option_get(p->zoom_scan, "scanStatus");
			if (v) {
				add_assoc_long(pval_opt, "status", atoi(v));
			}
		}
	}
	release_assoc(p);
}
/* }}} */

/* {{{ proto void yaz_ccl_conf(resource id, array package)
   Configure CCL package */
PHP_FUNCTION(yaz_ccl_conf)
{
	zval *pval_id, *pval_package;
	Yaz_Association p;

	if (ZEND_NUM_ARGS() != 2 ||
		zend_parse_parameters(2 TSRMLS_CC, "za", &pval_id, &pval_package)
		== FAILURE) {
		WRONG_PARAM_COUNT;
	}
	get_assoc(INTERNAL_FUNCTION_PARAM_PASSTHRU, pval_id, &p);
	if (p) {
		ccl_qual_rm(&p->bibset);
		p->bibset = ccl_qual_mk();
	}
	if (p) {
#if PHP_API_VERSION >= 20150101
		HashTable *ht = Z_ARRVAL_P(pval_package);
		zend_string *key;
		zval *val;

		ZEND_HASH_FOREACH_STR_KEY_VAL(ht, key, val) {
			if (key) { //HASH_KEY_IS_STRING
				if (Z_TYPE_P(val) == IS_STRING)
					ccl_qual_fitem(p->bibset, val->value.str->val, key->val);
			}
		}
		ZEND_HASH_FOREACH_END();
#else
		HashTable *ht = Z_ARRVAL_PP(&pval_package);
		HashPosition pos;
		zval **ent;
		char *key;
		for (zend_hash_internal_pointer_reset_ex(ht, &pos);
			 zend_hash_get_current_data_ex(ht, (void**) &ent, &pos) == SUCCESS;
			 zend_hash_move_forward_ex(ht, &pos)
			 ) {
			ulong idx;
#if PHP_API_VERSION > 20010101
			int type = zend_hash_get_current_key_ex(ht, &key, 0, &idx, 0, &pos);
#else
			int type = zend_hash_get_current_key_ex(ht, &key, 0, &idx, &pos);
#endif
			if (type != HASH_KEY_IS_STRING || Z_TYPE_PP(ent) != IS_STRING) {
				continue;
			}
			ccl_qual_fitem(p->bibset, (*ent)->value.str.val, key);
		}
#endif
	}
	release_assoc(p);
}
/* }}} */

/* {{{ proto bool yaz_ccl_parse(resource id, string query, array res)
   Parse a CCL query */
PHP_FUNCTION(yaz_ccl_parse)
{
	zval *pval_id, *pval_res = 0;
	char *query;
	zend_size_t query_len;
	Yaz_Association p;

	if (ZEND_NUM_ARGS() != 3 ||
		zend_parse_parameters(3 TSRMLS_CC, "zsz/",
							  &pval_id, &query, &query_len, &pval_res)
		== FAILURE) {
		WRONG_PARAM_COUNT;
	}

	zval_dtor(pval_res);
	array_init(pval_res);
	get_assoc(INTERNAL_FUNCTION_PARAM_PASSTHRU, pval_id, &p);
	if (p) {
		struct ccl_rpn_node *rpn;
		int error_pos;
		int error_code;
		CCL_parser ccl_parser = ccl_parser_create(p->bibset);

		rpn = ccl_parser_find_str(ccl_parser, query);

		error_code = ccl_parser_get_error(ccl_parser, &error_pos);
		add_assoc_long(pval_res, "errorcode", error_code);

		if (error_code)
		{
			ADD_ASSOC_STRING(pval_res, "errorstring",
							 (char *) ccl_err_msg(error_code));
			add_assoc_long(pval_res, "errorpos", error_pos);
			RETVAL_FALSE;
		}
		else
		{
			WRBUF wrbuf_pqf = wrbuf_alloc();
			ccl_stop_words_t csw = ccl_stop_words_create();
			int r = ccl_stop_words_tree(csw, p->bibset, &rpn);

			if (r)
			{
				/* stop words were removed. Return stopwords info */
				zval *zval_stopwords;
				int idx;

#if PHP_API_VERSION >= 20150101
				zval zval_stopwords0;
				zval_stopwords = &zval_stopwords0;
#else
				MAKE_STD_ZVAL(zval_stopwords);
#endif
				array_init(zval_stopwords);
				for (idx = 0; ; idx++)
				{
					zval *zval_stopword;

					const char *qname;
					const char *term;
					if (!ccl_stop_words_info(csw, idx, &qname, &term))
						break;

#if PHP_API_VERSION >= 20150101
					zval zval_stopword0;
					zval_stopword = &zval_stopword0;
#else
					MAKE_STD_ZVAL(zval_stopword);
#endif
					array_init(zval_stopword);

					ADD_ASSOC_STRING(zval_stopword, "field", (char *) qname);
					ADD_ASSOC_STRING(zval_stopword, "term", (char *) term);
					add_next_index_zval(zval_stopwords, zval_stopword);
				}
				add_assoc_zval(pval_res, "stopwords", zval_stopwords);
			}
			ccl_pquery(wrbuf_pqf, rpn);
			ADD_ASSOC_STRING(pval_res, "rpn", (char*) wrbuf_cstr(wrbuf_pqf));
			wrbuf_destroy(wrbuf_pqf);
			ccl_stop_words_destroy(csw);
			RETVAL_TRUE;
		}
		ccl_rpn_delete(rpn);
	} else {
		RETVAL_FALSE;
	}
	release_assoc(p);
}
/* }}} */


#if YAZ_VERSIONL >= 0x050100

/* {{{ proto bool yaz_cql_parse(resource id, string cql, array res, bool rev)
   Parse a CQL query */
PHP_FUNCTION(yaz_cql_parse)
{
	zval *pval_id, *pval_res = 0;
	char *query;
	zend_size_t query_len;
	Yaz_Association p;
	zend_bool reverse = 0;

	if (ZEND_NUM_ARGS() != 4 ||
		zend_parse_parameters(4 TSRMLS_CC, "zsz/b",
							  &pval_id, &query, &query_len, &pval_res, &reverse)
		== FAILURE) {
		WRONG_PARAM_COUNT;
	}

	zval_dtor(pval_res);
	array_init(pval_res);
	get_assoc(INTERNAL_FUNCTION_PARAM_PASSTHRU, pval_id, &p);
	if (p) {
		if (reverse) {
            ODR odr = odr_createmem(ODR_ENCODE);
            YAZ_PQF_Parser pp = yaz_pqf_create();
            Z_RPNQuery *rpn = yaz_pqf_parse(pp, odr, query);
			WRBUF wrbuf_cql = wrbuf_alloc();
			int r;
			if (!rpn) {
				add_assoc_long(pval_res, "errorcode", 0);
				ADD_ASSOC_STRING(pval_res, "addinfo",
								 (char *) "PQF syntax error");
				RETVAL_FALSE;
			} else if ((r = cql_transform_rpn2cql_stream(p->ct, wrbuf_vp_puts,
														 wrbuf_cql, rpn))) {
				add_assoc_long(pval_res, "errorcode", r);
				RETVAL_FALSE;
			} else {
				ADD_ASSOC_STRING(pval_res, "cql",
								 (char *) wrbuf_cstr(wrbuf_cql));
				RETVAL_TRUE;
			}
			wrbuf_destroy(wrbuf_cql);
			yaz_pqf_destroy(pp);
            odr_destroy(odr);
		} else {
			CQL_parser cp = cql_parser_create();
			int r = cql_parser_string(cp, query);
			if (r) {
				add_assoc_long(pval_res, "errorcode", 0);
				ADD_ASSOC_STRING(pval_res, "addinfo",
								 (char *) "syntax error");
				RETVAL_FALSE;
			} else {
				WRBUF wrbuf_addinfo = wrbuf_alloc();
				WRBUF wrbuf_pqf = wrbuf_alloc();
				r = cql_transform_r(p->ct, cql_parser_result(cp), wrbuf_addinfo,
									wrbuf_vp_puts, wrbuf_pqf);
				if (r) {
					add_assoc_long(pval_res, "errorcode", r);
					if (wrbuf_len(wrbuf_addinfo))
						ADD_ASSOC_STRING(pval_res, "addinfo",
										 (char *) wrbuf_cstr(wrbuf_addinfo));
					RETVAL_FALSE;
				} else {
					wrbuf_chop_right(wrbuf_pqf);
					ADD_ASSOC_STRING(pval_res, "rpn",
									 (char *) wrbuf_cstr(wrbuf_pqf));
					RETVAL_TRUE;
				}
				wrbuf_destroy(wrbuf_pqf);
				wrbuf_destroy(wrbuf_addinfo);
			}
			cql_parser_destroy(cp);
		}
	} else {
		RETVAL_FALSE;
	}
	release_assoc(p);
}
/* }}} */

#endif

#if YAZ_VERSIONL >= 0x050100
/* {{{ proto void yaz_cql_conf(resource id, array package)
   Configure CQL package */
PHP_FUNCTION(yaz_cql_conf)
{
	zval *pval_id, *pval_package;
	Yaz_Association p;

	if (ZEND_NUM_ARGS() != 2 ||
		zend_parse_parameters(2 TSRMLS_CC, "za", &pval_id, &pval_package)
		== FAILURE) {
		WRONG_PARAM_COUNT;
	}
	get_assoc(INTERNAL_FUNCTION_PARAM_PASSTHRU, pval_id, &p);
	if (p) {
		cql_transform_close(p->ct);
		p->ct = cql_transform_create();
	}
	if (p) {
#if PHP_API_VERSION >= 20150101
		HashTable *ht = Z_ARRVAL_P(pval_package);
		zend_string *key;
		zval *val;

		ZEND_HASH_FOREACH_STR_KEY_VAL(ht, key, val) {
			if (key) { //HASH_KEY_IS_STRING
				if (Z_TYPE_P(val) == IS_STRING)
					cql_transform_define_pattern(p->ct, key->val,
												 val->value.str->val);
			}
		}
		ZEND_HASH_FOREACH_END();
#else
		HashTable *ht = Z_ARRVAL_PP(&pval_package);
		HashPosition pos;
		zval **ent;
		char *key;

		for (zend_hash_internal_pointer_reset_ex(ht, &pos);
			zend_hash_get_current_data_ex(ht, (void**) &ent, &pos) == SUCCESS;
			zend_hash_move_forward_ex(ht, &pos)
		) {
			ulong idx;
#if PHP_API_VERSION > 20010101
			int type = zend_hash_get_current_key_ex(ht, &key, 0, &idx, 0, &pos);
#else
			int type = zend_hash_get_current_key_ex(ht, &key, 0, &idx, &pos);
#endif
			if (type != HASH_KEY_IS_STRING || Z_TYPE_PP(ent) != IS_STRING) {
				continue;
			}

			cql_transform_define_pattern(p->ct, key, (*ent)->value.str.val);
		}
#endif
	}
	release_assoc(p);
}
/* }}} */
#endif

/* {{{ proto bool yaz_database (resource id, string databases)
   Specify the databases within a session */
PHP_FUNCTION(yaz_database)
{
	zval *pval_id;
	char *database;
	zend_size_t database_len;
	Yaz_Association p;

	if (ZEND_NUM_ARGS() != 2 ||
		zend_parse_parameters(2 TSRMLS_CC, "zs", &pval_id,
							  &database, &database_len) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	get_assoc(INTERNAL_FUNCTION_PARAM_PASSTHRU, pval_id, &p);
	option_set(p, "databaseName", database);
	RETVAL_TRUE;
	release_assoc(p);
}
/* }}} */

/* {{{ php_yaz_init_globals
 */
static void php_yaz_init_globals(zend_yaz_globals *yaz_globals)
{
	yaz_globals->assoc_seq = 0;
	yaz_globals->max_links = 100;
	yaz_globals->keepalive = 120;
	yaz_globals->log_file = NULL;
	yaz_globals->log_mask = NULL;
}
/* }}} */

static void yaz_close_session(Yaz_Association *as TSRMLS_DC)
{
	if (*as && (*as)->order == YAZSG(assoc_seq)) {
		if ((*as)->persistent) {
			(*as)->in_use = 0;
		} else {
			yaz_association_destroy(*as);
			*as = 0;
		}
	}
}


#if PHP_API_VERSION >= 20150101
static void yaz_close_link(zend_resource *res TSRMLS_DC)
{
	Yaz_Association *as = (Yaz_Association *)
		zend_fetch_resource(res, "YAZ link", le_link);
	yaz_close_session(as TSRMLS_CC);
}
#else
static void yaz_close_link(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
	Yaz_Association *as = (Yaz_Association *) rsrc->ptr;
	yaz_close_session(as TSRMLS_CC);
}
#endif

/* {{{ PHP_INI_BEGIN
 */
PHP_INI_BEGIN()
#if PHP_MAJOR_VERSION >= 5
	STD_PHP_INI_ENTRY("yaz.max_links", "100", PHP_INI_ALL, OnUpdateLong, max_links, zend_yaz_globals, yaz_globals)
#else
	STD_PHP_INI_ENTRY("yaz.max_links", "100", PHP_INI_ALL, OnUpdateInt, max_links, zend_yaz_globals, yaz_globals)
#endif
#if PHP_MAJOR_VERSION >= 5
	STD_PHP_INI_ENTRY("yaz.keepalive", "120", PHP_INI_ALL, OnUpdateLong, keepalive, zend_yaz_globals, yaz_globals)
#else
	STD_PHP_INI_ENTRY("yaz.keepalive", "120", PHP_INI_ALL, OnUpdateInt, keepalive, zend_yaz_globals, yaz_globals)
#endif
	STD_PHP_INI_ENTRY("yaz.log_file", NULL, PHP_INI_ALL, OnUpdateString, log_file, zend_yaz_globals, yaz_globals)
	STD_PHP_INI_ENTRY("yaz.log_mask", NULL, PHP_INI_ALL, OnUpdateString, log_mask, zend_yaz_globals, yaz_globals)
PHP_INI_END()
/* }}} */

PHP_MINIT_FUNCTION(yaz)
{
	int i;
	const char *fname;
	const char *mask;
#ifdef ZTS
	yaz_mutex = tsrm_mutex_alloc();
#endif

	ZEND_INIT_MODULE_GLOBALS(yaz, php_yaz_init_globals, NULL);

	REGISTER_INI_ENTRIES();

	REGISTER_LONG_CONSTANT("ZOOM_EVENT_NONE", ZOOM_EVENT_NONE,
						   CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("ZOOM_EVENT_CONNECT", ZOOM_EVENT_CONNECT,
						   CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("ZOOM_EVENT_SEND_DATA", ZOOM_EVENT_SEND_DATA,
						   CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("ZOOM_EVENT_RECV_DATA", ZOOM_EVENT_RECV_DATA,
						   CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("ZOOM_EVENT_TIMEOUT", ZOOM_EVENT_TIMEOUT,
						   CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("ZOOM_EVENT_UNKNOWN", ZOOM_EVENT_UNKNOWN,
						   CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("ZOOM_EVENT_SEND_APDU", ZOOM_EVENT_SEND_APDU,
						   CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("ZOOM_EVENT_RECV_APDU", ZOOM_EVENT_RECV_APDU,
						   CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("ZOOM_EVENT_RECV_RECORD", ZOOM_EVENT_RECV_RECORD,
						   CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("ZOOM_EVENT_RECV_SEARCH", ZOOM_EVENT_RECV_SEARCH,
						   CONST_CS|CONST_PERSISTENT);

	fname = YAZSG(log_file);
	mask = YAZSG(log_mask);
	if (fname && *fname)
	{
		yaz_log_init_file(fname);
		if (!mask)
			mask = "all";
		yaz_log_init_level(yaz_log_mask_str(mask));
	}
	else
		yaz_log_init_level(0);

	le_link = zend_register_list_destructors_ex(yaz_close_link, 0, "YAZ link", module_number);

	order_associations = 1;
	shared_associations = xmalloc(sizeof(*shared_associations) * MAX_ASSOC);
	for (i = 0; i < MAX_ASSOC; i++) {
		shared_associations[i] = 0;
	}
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(yaz)
{
	int i;

	if (shared_associations) {
		for (i = 0; i < MAX_ASSOC; i++) {
			yaz_association_destroy (shared_associations[i]);
		}
		xfree(shared_associations);
		shared_associations = 0;
	}
#ifdef ZTS
	tsrm_mutex_free(yaz_mutex);
#endif

	yaz_log_init_file(0);

	UNREGISTER_INI_ENTRIES();

	return SUCCESS;
}

PHP_MINFO_FUNCTION(yaz)
{
	char version_str[20];

	strcpy(version_str, "unknown");
	yaz_version(version_str, 0);
	php_info_print_table_start();
	php_info_print_table_row(2, "YAZ Support", "enabled");
	php_info_print_table_row(2, "PHP/YAZ Version", PHP_YAZ_VERSION);
	php_info_print_table_row(2, "YAZ Version", version_str);
	php_info_print_table_row(2, "Compiled with YAZ version", YAZ_VERSION);
	php_info_print_table_end();
}

PHP_RSHUTDOWN_FUNCTION(yaz)
{
	time_t now = time(0);
	int i;

#ifdef ZTS
	tsrm_mutex_lock(yaz_mutex);
#endif
	for (i = 0; i < YAZSG(max_links); i++) {
		Yaz_Association *as = shared_associations + i;
		if (*as)
		{
			if (now - (*as)->time_stamp > YAZSG(keepalive))
			{
				yaz_association_destroy(*as);
				*as = 0;
			}
		}
	}
#ifdef ZTS
	tsrm_mutex_unlock(yaz_mutex);
#endif
	return SUCCESS;
}

PHP_RINIT_FUNCTION(yaz)
{
	char pidstr[20];

	sprintf(pidstr, "%ld", (long) getpid());
#ifdef ZTS
	tsrm_mutex_lock(yaz_mutex);
#endif
	YAZSG(assoc_seq) = order_associations++;
#ifdef ZTS
	tsrm_mutex_unlock(yaz_mutex);
#endif
	yaz_log_init_prefix(pidstr);
	return SUCCESS;
}

zend_module_entry yaz_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"yaz",
	yaz_functions,
	PHP_MINIT(yaz),
	PHP_MSHUTDOWN(yaz),
	PHP_RINIT(yaz),
	PHP_RSHUTDOWN(yaz),
	PHP_MINFO(yaz),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_YAZ_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};


#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
