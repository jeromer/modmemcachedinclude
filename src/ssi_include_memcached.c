/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "apr.h"
#include "apr_strings.h"
#include "apr_hash.h"
#include "apr_user.h"
#include "apr_lib.h"
#include "apr_optional.h"

#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_request.h"
#include "http_protocol.h"
#include "http_log.h"
#include "http_main.h"
#include "util_script.h"
#include "mod_include.h"

#include "apr_memcache.h"

typedef struct {
    apr_memcache_t     *memcached;
    apr_array_header_t *servers;

    apr_uint32_t conn_min;
    apr_uint32_t conn_smax;
    apr_uint32_t conn_max;
    apr_uint32_t conn_ttl;
    apr_uint16_t max_servers;
    apr_off_t    min_size;
    apr_off_t    max_size;
} ssi_include_dir_config;

typedef struct {
  const char *host;
  apr_port_t port;
  apr_memcache_server_t *server;
} memcached_server_t;

#define DEFAULT_MAX_SERVERS 1
#define DEFAULT_MIN_CONN    1
#define DEFAULT_SMAX        10
#define DEFAULT_MAX         15
#define DEFAULT_TTL         10
#define DEFAULT_MIN_SIZE    1
#define DEFAULT_MAX_SIZE    1048576

module AP_MODULE_DECLARE_DATA ssi_include_memcached_module;

static APR_OPTIONAL_FN_TYPE(ap_register_include_handler) *ssi_include_memcached_pfn_rih;
static APR_OPTIONAL_FN_TYPE(ap_ssi_get_tag_and_value)    *ssi_include_memcached_pfn_gtv;
static APR_OPTIONAL_FN_TYPE(ap_ssi_parse_string)         *ssi_include_memcached_pfn_ps;

/*
 * <!--#include virtual|file|memcached="..." [virtual|file="..."] ... -->
 */
static apr_status_t handle_include_memcached(include_ctx_t *ctx, ap_filter_t *f,
                                   apr_bucket_brigade *bb)
{
    request_rec *r = f->r;

    if (!ctx->argc) {
        ap_log_rerror(APLOG_MARK,
                      (ctx->flags & SSI_FLAG_PRINTING)
                          ? APLOG_ERR : APLOG_WARNING,
                      0, r, "missing argument for include element in %s",
                      r->filename);
    }

    if (!(ctx->flags & SSI_FLAG_PRINTING)) {
        return APR_SUCCESS;
    }

    if (!ctx->argc) {
        SSI_CREATE_ERROR_BUCKET(ctx, f, bb);
        return APR_SUCCESS;
    }

    while (1) {
        char *tag     = NULL;
        char *tag_val = NULL;
        request_rec *rr = NULL;
        char *error_fmt = NULL;
        char *parsed_string;

        ssi_include_memcached_pfn_gtv(ctx, &tag, &tag_val, SSI_VALUE_DECODED);
        if (!tag || !tag_val) {
            break;
        }

        if (strcmp(tag, "virtual") && strcmp(tag, "file") && strcmp(tag, "memcached")) {
            ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "unknown parameter "
                          "\"%s\" to tag include in %s", tag, r->filename);
            SSI_CREATE_ERROR_BUCKET(ctx, f, bb);
            break;
        }

        parsed_string = ssi_include_memcached_pfn_ps(ctx, tag_val, NULL, 0, SSI_EXPAND_DROP_NAME);

        /* Fetching files from Memcached */
        if (tag[0] == 'm') {

            ssi_include_dir_config *conf;
            memcached_server_t *svr;
            apr_status_t       rv;
            int                i;

            apr_uint16_t flags;
            char *strkey = NULL;
            char *value;
            apr_size_t value_len;

            strkey = ap_escape_uri(r->pool, parsed_string);

            conf = (ssi_include_dir_config *)ap_get_module_config(r->per_dir_config, &ssi_include_memcached_module);

            rv = apr_memcache_create(r->pool, conf->max_servers, 0, &(conf->memcached));

            if(rv != APR_SUCCESS) {
                ap_log_rerror(APLOG_MARK, APLOG_DEBUG, rv, r, "Unable to create memcached structure");
            }

            svr = (memcached_server_t *)conf->servers->elts;

            for(i = 0; i < conf->servers->nelts; i++) {

                rv = apr_memcache_server_create(r->pool, svr[i].host, svr[i].port,
                                                conf->conn_min, conf->conn_smax, 
                                                conf->conn_max, conf->conn_ttl,
                                                &(svr[i].server));

                if(rv != APR_SUCCESS) {
                    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, rv, r, "Unable to create memcache server for %s:%d is Memcached running ?", svr[i].host, svr[i].port);
                    continue;
                }    
                
                rv = apr_memcache_add_server(conf->memcached, svr[i].server);

                if(rv != APR_SUCCESS) {
                    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, rv, r, "Unable to add memcache server for %s:%d", svr[i].host, svr[i].port);
                }    

                ap_log_rerror(APLOG_MARK, APLOG_DEBUG, rv, r, "Memcached server successfully created %s:%d %d %d %d", svr[i].host, svr[i].port, conf->conn_smax, conf->conn_max, conf->conn_ttl);
            }

            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "Fetching the file with key : %s", strkey);

            rv = apr_memcache_getp(conf->memcached, r->pool, strkey, &value, &value_len, &flags);

            if( rv == APR_SUCCESS ) {
                ap_log_rerror(APLOG_MARK, APLOG_DEBUG, rv, r, "File found with key %s, value : %s", strkey, value);
                APR_BRIGADE_INSERT_TAIL(bb,
                                        apr_bucket_pool_create(apr_pmemdup(ctx->pool, value, value_len),
                                                               value_len,
                                                               ctx->pool,
                                                               f->c->bucket_alloc));
                return APR_SUCCESS;
            }
            else {
                error_fmt = "Unable to fetch file with key '%s' in parsed file %s";
            }
        }
        else if (tag[0] == 'f') {
            char *newpath;
            apr_status_t rv;

            /* be safe only files in this directory or below allowed */
            rv = apr_filepath_merge(&newpath, NULL, parsed_string,
                                    APR_FILEPATH_SECUREROOTTEST |
                                    APR_FILEPATH_NOTABSOLUTE, ctx->dpool);

            if (rv != APR_SUCCESS) {
                error_fmt = "unable to include file \"%s\" in parsed file %s";
            }
            else {
                rr = ap_sub_req_lookup_file(newpath, r, f->next);
            }
        }
        else {
            rr = ap_sub_req_lookup_uri(parsed_string, r, f->next);
        }

        if (!error_fmt && rr->status != HTTP_OK) {
            error_fmt = "unable to include \"%s\" in parsed file %s";
        }

        if (!error_fmt && (ctx->flags & SSI_FLAG_NO_EXEC) &&
            rr->content_type && strncmp(rr->content_type, "text/", 5)) {

            error_fmt = "unable to include potential exec \"%s\" in parsed "
                        "file %s";
        }

        /* See the Kludge in includes_filter for why.
         * Basically, it puts a bread crumb in here, then looks
         * for the crumb later to see if its been here.
         */
        if (rr) {
            ap_set_module_config(rr->request_config, &ssi_include_memcached_module, r);
        }

        if (!error_fmt && ap_run_sub_req(rr)) {
            error_fmt = "unable to include \"%s\" in parsed file %s";
        }

        if (error_fmt) {
            ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, error_fmt, tag_val,
                          r->filename);
            SSI_CREATE_ERROR_BUCKET(ctx, f, bb);
        }

        /* Do *not* destroy the subrequest here; it may have allocated
         * variables in this r->subprocess_env in the subrequest's
         * r->pool, so that pool must survive as long as this request.
         * Yes, this is a memory leak. */
        if (error_fmt) {
            break;
        }
    }

    return APR_SUCCESS;
}

static void *create_ssi_include_memcached_dir_config(apr_pool_t *p, char *dummy)
{
    ssi_include_dir_config *result = apr_palloc(p, sizeof(ssi_include_dir_config));

    result->servers = apr_array_make(p, 1, sizeof(memcached_server_t));

    result->max_servers = DEFAULT_MAX_SERVERS;
    result->min_size    = DEFAULT_MIN_SIZE;
    result->max_size    = DEFAULT_MAX_SIZE;
    result->conn_min    = DEFAULT_MIN_CONN;
    result->conn_smax   = DEFAULT_SMAX;
    result->conn_max    = DEFAULT_MAX;
    result->conn_ttl    = DEFAULT_TTL;

    return result;
}

static const char *set_memcached_host(cmd_parms *cmd, void *mconfig, const char *arg)
{
    char *host, *port;
    memcached_server_t *memcached_server = NULL;  
    ssi_include_dir_config *conf = mconfig;

    /*
     * I should consider using apr_parse_addr_port instead
     * [http://apr.apache.org/docs/apr/1.3/group__apr__network__io.html#g90c31b2f012c6b1e2d842a96c4431de3]
     */ 
    host = apr_pstrdup(cmd->pool, arg);

    port = strchr(host, ':');
    if(port) {
      *(port++) = '\0';
    }

    if(!*host || host == NULL || !*port || port == NULL) {
        return "MemcachedCacheServer should be in the correct format : host:port";
    }

    memcached_server = apr_array_push(conf->servers);
    memcached_server->host = host;
    memcached_server->port = apr_atoi64(port);

    return NULL;
}

static const char *set_memcached_timeout(cmd_parms *cmd, void *mconfig, const char *arg)
{
    ssi_include_dir_config *conf = mconfig;

    conf->conn_ttl = atoi(arg);

    return NULL;
}

static const char *set_memcached_soft_max_conn(cmd_parms *cmd, void *mconfig, const char *arg)
{
    ssi_include_dir_config *conf = mconfig;

    conf->conn_smax = atoi(arg);

    return NULL;
}

static const char *set_memcached_hard_max_conn(cmd_parms *cmd, void *mconfig, const char *arg)
{
    ssi_include_dir_config *conf = mconfig;

    conf->conn_max = atoi(arg);

    return NULL;
}

static int ssi_include_memcached_post_config(apr_pool_t *p, apr_pool_t *plog,
                                             apr_pool_t *ptemp, server_rec *s)
{
    ssi_include_memcached_pfn_gtv  = APR_RETRIEVE_OPTIONAL_FN(ap_ssi_get_tag_and_value);
    ssi_include_memcached_pfn_ps   = APR_RETRIEVE_OPTIONAL_FN(ap_ssi_parse_string);
    ssi_include_memcached_pfn_rih  = APR_RETRIEVE_OPTIONAL_FN(ap_register_include_handler);

    if (ssi_include_memcached_pfn_gtv &&
        ssi_include_memcached_pfn_ps  &&
        ssi_include_memcached_pfn_rih) {
        ssi_include_memcached_pfn_rih("include", handle_include_memcached);
    } else {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL, "unable to register a new include function");
    }

    return OK;
}

static void register_hooks(apr_pool_t *p)
{
    static const char * const prereq[] = { "mod_include.c", NULL };

    /*APR_REGISTER_OPTIONAL_FN(ap_ssi_parse_string);*/
    /*APR_REGISTER_OPTIONAL_FN(ap_ssi_include_memcached_register_handler);*/

    ap_hook_post_config(ssi_include_memcached_post_config, prereq, NULL, APR_HOOK_FIRST);
}

static const command_rec includes_cmds[] =
{
    AP_INIT_TAKE1("MemcachedHost",
                  set_memcached_host,
                  NULL,
                  OR_OPTIONS,
                  "Memcached hostname or IP address with portnumber, like this, 127.0.0.1:11211"),
    
    AP_INIT_TAKE1("MemcachedTTL",
                  set_memcached_timeout,
                  NULL,
                  OR_OPTIONS,
                  "Memcached timeout (in seconds)"),
    
    AP_INIT_TAKE1("MemcachedSoftMaxConn",
                  set_memcached_soft_max_conn,
                  NULL,
                  OR_OPTIONS,
                  "Memcached low max connexions"),
    
    AP_INIT_TAKE1("MemcachedHardMaxConn",
                  set_memcached_hard_max_conn,
                  NULL,
                  OR_OPTIONS,
                  "Memcached high maximum connexions"),

    {NULL}
};

module AP_MODULE_DECLARE_DATA ssi_include_memcached_module =
{
    STANDARD20_MODULE_STUFF,
    create_ssi_include_memcached_dir_config,   /* dir config creator  */
    NULL,                                      /* merge dir config    */
    NULL,                         /* create_ssi_include_memcached_server_config, server config       */
    NULL,                         /* merge server config */
    includes_cmds,                /* command apr_table_t */
    register_hooks                /* register hooks      */
};