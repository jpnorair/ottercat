/* Copyright 2014, JP Norair
  *
  * Licensed under the OpenTag License, Version 1.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  * http://www.indigresso.com/wiki/doku.php?id=opentag:license_1_0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  */

///@todo harmonize this with the more slick dterm from otdb project

// Application Headers
#include "cliopt.h"         // to be part of dterm via environment variables
#include "cmds.h"
#include "dterm.h"

// Local Libraries/Headers
#include <argtable3.h>
#include <cJSON.h>

// Standard C & POSIX Libraries
//#include <pthread.h>
//#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>


#if 0 //OTTER_FEATURE_DEBUG
#   define PRINTLINE()     fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__)
#   define DEBUGPRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#   define PRINTLINE()     do { } while(0)
#   define DEBUGPRINT(...) do { } while(0)
#endif

#ifndef VERBOSE_PRINTF
#   define VERBOSE_PRINTF(...)  do { } while(0)
#endif
#ifndef DEBUG_PRINTF
#   define DEBUG_PRINTF(...)  do { } while(0)
#endif
#ifndef VCLIENT_PRINTF
#   define VCLIENT_PRINTF(...)  do { } while(0)
#endif


static void sub_str_sanitize(char* str, size_t max) {
    while ((*str != 0) && (max != 0)) {
        if (*str == '\r') {
            *str = '\n';
        }
        str++;
        max--;
    }
}

static size_t sub_str_mark(char* str, size_t max) {
    char* s1 = str;
    while ((*str!=0) && (*str!='\n') && (max!=0)) {
        max--;
        str++;
    }
    if (*str=='\n') *str = 0;
    
    return (str - s1);
}

static int sub_hexwrite(int fd, const uint8_t byte) {
    static const char convert[] = "0123456789ABCDEF";
    char dst[2];
    dst[0]  = convert[byte >> 4];
    dst[1]  = convert[byte & 0x0f];
    write(fd, dst, 2);
    return 2;
}

static int sub_hexswrite(char* dst, const uint8_t byte) {
    static const char convert[] = "0123456789ABCDEF";
    *dst++ = convert[byte >> 4];
    *dst++ = convert[byte & 0x0f];
    return 2;
}

static int sub_hexstream(int fd, const uint8_t* src, size_t src_bytes) {
    int bytesout = 0;
    
    while (src_bytes != 0) {
        src_bytes--;
        bytesout += sub_hexwrite(fd, *src++);
    }
    
    return bytesout;
}


static void iso_free(void* ctx) {
    talloc_free(ctx);
}

static TALLOC_CTX* iso_ctx;
static void* iso_malloc(size_t size) {
    return talloc_size(iso_ctx, size);
}

static void cjson_iso_allocators(void) {
    cJSON_Hooks hooks;
    hooks.free_fn   = &iso_free;
    hooks.malloc_fn = &iso_malloc;
    cJSON_InitHooks(&hooks);
}

static void cjson_std_allocators(void) {
    cJSON_InitHooks(NULL);
}





/** DTerm Control Functions <BR>
  * ========================================================================<BR>
  */

int dterm_init(dterm_handle_t* dth, int fd_in, int fd_out, bool use_socket, void* devmgr_handle) {
    int rc = 0;

    ///@todo ext data should be handled as its own module, but we can accept
    /// that it must be non-null.
    if ((dth == NULL) || (devmgr_handle == NULL)) {
        return -1;
    }
    
    talloc_disable_null_tracking();
    dth->pctx = talloc_new(NULL);
    dth->tctx = NULL;
    if (dth->pctx == NULL){
        rc = -2;
        goto dterm_init_TERM;
    }
    
    dth->use_socket = use_socket;
    dth->devmgr     = devmgr_handle;
    dth->fd.in      = fd_in;
    dth->fd.out     = fd_out;
    return 0;
    
    dterm_init_TERM:
    talloc_free(dth->tctx);
    talloc_free(dth->pctx);
    return rc;
}



void dterm_deinit(dterm_handle_t* dth) {
    talloc_free(dth->tctx);
    talloc_free(dth->pctx);
}




/** DTerm Threads <BR>
  * ========================================================================<BR>
  * <LI> dterm_piper()      : For use with input pipe option </LI>
  * <LI> dterm_prompter()   : For use with console entry (default) </LI>
  *
  * Only one of the threads will run.  Piper is much simpler because it just
  * reads stdin pipe as an atomic line read.  Prompter requires character by
  * character input and analysis, and it enables shell-like features.
  */

static int sub_proc_lineinput(dterm_handle_t* dth, int* cmdrc, char* loadbuf, int linelen) {
    uint8_t     protocol_buf[1024];
    cJSON*      cmdobj;
    uint8_t*    cursor  = protocol_buf;
    int         bufmax  = sizeof(protocol_buf);
    int         bytesout = 0;
    
    int bytesin;
    
    DEBUG_PRINTF("raw input (%i bytes) %.*s\n", linelen, linelen, loadbuf);

    // Isolation memory context
    iso_ctx = dth->tctx;

    // Set allocators for cJSON, argtable
    cjson_iso_allocators();
    arg_set_allocators(&iso_malloc, &iso_free);
    
    ///@todo set context for other data systems
    
    /// The input can be JSON of the form:
    /// { "type":"${cmd_type}", data:"${cmd_data}" }
    /// where we only truly care about the data object, which must be a string.
    cmdobj = cJSON_Parse(loadbuf);
    if (cJSON_IsObject(cmdobj)) {
        cJSON* dataobj;
        cJSON* typeobj;
        typeobj = cJSON_GetObjectItemCaseSensitive(cmdobj, "type");
        dataobj = cJSON_GetObjectItemCaseSensitive(cmdobj, "data");

        if (cJSON_IsString(typeobj) && cJSON_IsString(dataobj)) {
            int hdr_sz;
            VCLIENT_PRINTF("JSON Request (%i bytes): %.*s\n", linelen, linelen, loadbuf);
            loadbuf = dataobj->valuestring;
            hdr_sz  = snprintf((char*)cursor, bufmax-1, "{\"type\":\"%s\", \"data\":", typeobj->valuestring);
            cursor += hdr_sz;
            bufmax -= hdr_sz;
        }
        else {
            goto sub_proc_lineinput_FREE;
        }
    }
    
    // Null terminate the cursor: errors may report a string.
    *cursor     = 0;
    bytesin     = linelen;
    bytesout    = cmd_devmgr(dth, cursor, &bytesin, (uint8_t*)loadbuf, bufmax);
    if (cmdrc != NULL) {
        *cmdrc = bytesout;
    }
    
    ///@todo spruce-up the command error reporting, maybe even with
    ///      a cursor showing where the first error was found.
    if (bytesout < 0) {
        bytesout = snprintf((char*)protocol_buf, sizeof(protocol_buf)-1,
                    "{\"cmd\":\"" OTTERCAT_PARAM_NAME "\", \"err\":%d, \"desc\":\"execution error\"}\n", bytesout);
        write(dth->fd.out, (char*)protocol_buf, bytesout);
    }
    
    // If there are bytes to send to MPipe, do that.
    // If bytesout == 0, there is no error, but also nothing
    // to send to MPipe.
    else if (bytesout > 0) {
        if (cJSON_IsObject(cmdobj)) {
            VCLIENT_PRINTF("JSON Response (%i bytes): %.*s\n", bytesout, bytesout, (char*)cursor);
            cursor += bytesout;
            bufmax -= bytesout;
            cursor  = (uint8_t*)stpncpy((char*)cursor, "}\n", bufmax);
            bytesout= (int)(cursor - protocol_buf);
        }
        
        DEBUG_PRINTF("raw output (%i bytes) %.*s\n", bytesout, bytesout, protocol_buf);
        write(dth->fd.out, (char*)protocol_buf, bytesout);
    }
    
    sub_proc_lineinput_FREE:
    cJSON_Delete(cmdobj);
    
    // Return cJSON and argtable to generic context allocators
    cjson_std_allocators();
    arg_set_allocators(NULL, NULL);
    
    return bytesout;
}



int dterm_cmdstream(dterm_handle_t* dth, char* stream) {
    int     stream_sz;
    char*   streamcursor;
    int     rc = 0;

    if (stream == NULL) {
        rc = -2;
        goto dterm_cmdfile_END;
    }
    
    stream_sz = (int)strlen(stream);
    sub_str_sanitize(stream, (size_t)stream_sz);

    // Run the command on each line
    streamcursor = stream;
    while (stream_sz > 0) {
        int linelen;
        int cmdrc;
        size_t poolsize;
        size_t est_poolobj;
        
        // Burn whitespace ahead of command.
        while (isspace(*streamcursor)) { streamcursor++; stream_sz--; }
        linelen = (int)sub_str_mark(streamcursor, (size_t)stream_sz);

        // Create temporary context as a memory pool
        poolsize    = cliopt_getpoolsize();
        est_poolobj = 4; //(poolsize / 128) + 1;
        dth->tctx   = talloc_pooled_object(NULL, void, est_poolobj, poolsize);
        
        // Echo input line to dterm
        if (cliopt_isverbose()) {
            dprintf(dth->fd.out, _E_MAG"<< "_E_NRM"%s\n", streamcursor);
        }
        
        // Process the line-input command
        sub_proc_lineinput(dth, &cmdrc, streamcursor, linelen);
        
        // Free temporary memory pool context
        talloc_free(dth->tctx);
        dth->tctx = NULL;
        
        // Exit the command sequence on first detection of error.
        if (cmdrc < 0) {
            dprintf(dth->fd.out, _E_RED"ERR: "_E_NRM"Command Returned %i: stopping.\n\n", cmdrc);
            break;
        }
        
        // +1 eats the terminator
        stream_sz      -= (linelen + 1);
        streamcursor   += (linelen + 1);
    }
    
    dterm_cmdfile_END:

    return rc;
}




