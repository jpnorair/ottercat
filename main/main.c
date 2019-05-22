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
/**
  * @file       main.c
  * @author     JP Norair
  * @version    R100
  * @date       20 May 2019
  * @brief      ottercat main() function and global data declarations
  * @defgroup   ottercat
  * @ingroup    ottercat
  * 
  * OTDB (OpenTag DataBase) is a threaded, POSIX-C app that provides a cache
  * for OpenTag device filesystems.  In other words, it allows the device 
  * filesystems to be mirrored and synchronized on a gateway device (typically
  * a computer of some sort, running some sort of Linux).
  *
  * See http://wiki.indigresso.com for more information and documentation.
  * 
  ******************************************************************************
  */

// Top Level Configuration Header
#include "ottercat_cfg.h"

// Application Headers
#include "cmds.h"
#include "cliopt.h"
#include "debug.h"
#include "sockpush.h"

// HBuilder Package Libraries
#include <argtable3.h>
#include <talloc.h>

// Standard C & POSIX Libraries
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


// Client Data type
static cliopt_t cliopts;

typedef struct {
    int exitcode;
} cli_struct;

cli_struct cli;





/** Local Functions <BR>
  * ========================================================================<BR>
  * otdb_main() is called by main().  The job of main() is to parse arguments
  * and then send them to otdb_main(), which deals with program setup and
  * management.
  */
static int sub_copy_stringarg(char** dststring, int argcount, const char* argstring) {
    if (argcount != 0) {
        size_t sz;
        if (*dststring != NULL) {
            free(*dststring);
        }
        sz = strlen(argstring) + 1;
        *dststring = malloc(sz);
        if (*dststring == NULL) {
            return -1;
        }
        memcpy(*dststring, argstring, sz);
        return argcount;
    }
    return 0;
}


static INTF_Type sub_intf_cmp(const char* s1) {
    INTF_Type selected_intf;

    if (strcmp(s1, "pipe") == 0) {
        selected_intf = INTF_pipe;
    }
    else if (strcmp(s1, "socket") == 0) {
        selected_intf = INTF_socket;
    }
    else {
        selected_intf = INTF_interactive;
    }
    
    return selected_intf;
}


static FORMAT_Type sub_fmt_cmp(const char* s1) {
    FORMAT_Type selected_fmt;
    
    if (strcmp(s1, "default") == 0) {
        selected_fmt = FORMAT_Default;
    }
    else if (strcmp(s1, "json") == 0) {
        selected_fmt = FORMAT_Json;
    }
    else if (strcmp(s1, "jsonhex") == 0) {
        selected_fmt = FORMAT_JsonHex;
    }
    else if (strcmp(s1, "bintex") == 0) {
        selected_fmt = FORMAT_Bintex;
    }
    else if (strcmp(s1, "hex") == 0) {
        selected_fmt = FORMAT_Hex;
    }
    else {
        selected_fmt = FORMAT_Default;
    }
    
    return selected_fmt;
}


static int sub_readline(size_t* bytesread, int fd, char* buf_a, int max) {
    size_t bytesin;
    char* start = buf_a;
    int rc = 0;
    char test;
    
    while (max > 0) {
        rc = (int)read(fd, buf_a, 1);
        if (rc != 1) {
            break;
        }
        max--;
        test = *buf_a++;
        if ((test == '\n') || (test == 0)) {
            break;
        }
    }
    
    bytesin = (buf_a - start);
    
    if (bytesread != NULL) {
        *bytesread = bytesin;
    }
    
    if (rc >= 0) {
        rc = (int)bytesin;
    }
    
    return rc;
}


int ottercat_main(INTF_Type intf_val, const char* socket, char* cmdstr) {
    sp_handle_t sockpush_handle;
    bool use_socket;
    void* devmgr_handle;
    dterm_handle_t dterm_handle;
    int cmdrc;
    
    cli.exitcode  = 0;
    
    DEBUG_PRINTF("Initializing Application Data\n");
    
    /// Start the devmgr childprocess, if one is specified.
    /// If it works, the devmgr command should be added using the name of the
    /// program used for devmgr.
    DEBUG_PRINTF("Initializing socket (%s) ...\n", socket);
    if (sp_open(&sockpush_handle, socket, 0) == 0) {
        use_socket      = true;
        devmgr_handle   = sockpush_handle;
    }
    else {
        fprintf(stderr, "Err: socket could not be opened.\n");
        cli.exitcode  = -2;
        use_socket    = false;
        devmgr_handle = NULL;
        goto ottercat_main_TERM;
    }
    
    DEBUG_PRINTF("--> done\n");
   
    /// Initialize DTerm data objects
    /// Non intrinsic dterm elements (cmdtab, devmgr, ext, tmpl) get attached
    /// following initialization
    DEBUG_PRINTF("Initializing DTerm ...\n");
    if (dterm_init(&dterm_handle, STDIN_FILENO, STDOUT_FILENO, use_socket, devmgr_handle) != 0) {
        cli.exitcode = -2;
        goto ottercat_main_TERM;
    }
    DEBUG_PRINTF("--> done\n");
    DEBUG_PRINTF("Finished startup\n");
    // ------------------------------------------------------------------------
    
    cmdrc = dterm_cmdstream(&dterm_handle, cmdstr);
    switch (cmdrc) {
        case 0:  VERBOSE_PRINTF("Command finished successfully\n");
                 break;
        case -1: ERR_PRINTF("dterm_cmdfile() out of memory. (-1)\n");
                 goto cmdstream_ERR;
        case -2: ERR_PRINTF("command cannot be opened. (-2)\n");
                 goto cmdstream_ERR;
        case -3: ERR_PRINTF("command cannot be read. (-3)\n");
                 goto cmdstream_ERR;
        case -4: ERR_PRINTF("command returned error. (-4)\n");
                 goto cmdstream_ERR;
        default:
        cmdstream_ERR:
            fprintf(stderr, ERRMARK"Error (%i) running command %s.\n", cmdrc, cmdstr);
            break;
    }

    // ------------------------------------------------------------------------
    DEBUG_PRINTF("Freeing dterm\n");
    dterm_deinit(&dterm_handle);
 
    ottercat_main_TERM:
    if (devmgr_handle != NULL) {
        if (use_socket) {
            DEBUG_PRINTF("Closing Device Manager socket\n");
            sp_close(devmgr_handle);
        }
    }
    
    // cli.exitcode is set to 0, unless sigint is raised.
    DEBUG_PRINTF("Exiting cleanly and flushing output buffers\n");
    fflush(stdout);
    fflush(stderr);
    
    VERBOSE_PRINTF("exiting (%i)\n", cli.exitcode);
    return cli.exitcode;
}



int main(int argc, char* argv[]) {
#   define FILL_STRINGARG(ARGITEM, VAR)   do { \
        size_t str_sz = strlen(ARGITEM->filename[0]) + 1;   \
        if (VAR != NULL) free(VAR);                         \
        VAR = malloc(str_sz);                               \
        if (VAR == NULL) goto main_FINISH;                  \
        memcpy(VAR, ARGITEM->filename[0], str_sz);          \
    } while(0);
    
    struct arg_lit  *help    = arg_lit0(NULL,"help",                    "print this help and exit");
    struct arg_lit  *version = arg_lit0(NULL,"version",                 "print version information and exit");
    struct arg_lit  *verbose = arg_lit0("v","verbose",                  "use verbose mode");
    struct arg_lit  *debug   = arg_lit0("d","debug",                    "Set debug mode on: requires compiling for debug");
    struct arg_int  *timeout = arg_int0("t","timeout","int",            "Integer number of milliseconds for response timeout: default 500ms");
    struct arg_int  *retries = arg_int0("r","retries","int",            "Integer number of request retries: default 0");
  //struct arg_str  *fmt     = arg_str0("f", "fmt", "format",           "\"default\", \"json\", \"jsonhex\", \"bintex\", \"hex\"");
    struct arg_file *socket  = arg_file1(NULL,NULL,"path/addr",         "Socket path/address of otter daemon");
    struct arg_str  *cmdstr  = arg_strn(NULL,NULL,"cmd",0,240,          "Command string to send to otter daemon");
    struct arg_end  *end     = arg_end(20);
    
    void* argtable[] = { help, version, verbose, debug, /*fmt,*/ socket, cmdstr, end };
    const char* progname = OTTERCAT_PARAM(NAME);
    int nerrors;
    bool bailout        = true;
    int exitcode        = 0;

    bool verbose_val    = false;
    bool debug_val      = false;
    int timeout_val     = 500;
    int tries_val       = 1;
    //FORMAT_Type fmt_val = FORMAT_JsonHex;
    INTF_Type intf_val  = INTF_socket;
    char* socket_val    = NULL;
    char* cmdstr_val    = NULL;
    size_t cmdstr_size;

    
    /// Initialize allocators in argtable lib to defaults
    arg_set_allocators(NULL, NULL);
    
    if (arg_nullcheck(argtable) != 0) {
        /// NULL entries were detected, some allocations must have failed 
        fprintf(stderr, "%s: insufficient memory\n", progname);
        exitcode = 1;
        goto main_FINISH;
    }

    /// Parse the command line as defined by argtable[]
    nerrors = arg_parse(argc, argv, argtable);

    /// special case: '--help' takes precedence over error reporting
    if (help->count > 0) {
        printf("Usage: %s", progname);
        arg_print_syntax(stdout, argtable, "\n");
        arg_print_glossary(stdout, argtable, "  %-25s %s\n");
        exitcode = 0;
        goto main_FINISH;
    }

    /// special case: '--version' takes precedence error reporting 
    if (version->count > 0) {
        printf("%s -- %s\n", OTTERCAT_PARAM_VERSION, OTTERCAT_PARAM_DATE);
        printf("Commit-ID: %s\n", OTTERCAT_PARAM_GITHEAD);
        printf("Designed by JP Norair (jpnorair@indigresso.com)\n");
        exitcode = 0;
        goto main_FINISH;
    }

    /// If the parser returned any errors then display them and exit
    /// - Display the error details contained in the arg_end struct.
    if (nerrors > 0) {
        arg_print_errors(stdout,end,progname);
        printf("Try '%s --help' for more information.\n", progname);
        exitcode = 1;
        goto main_FINISH;
    }

    /// Client Options.  These are read-only from internal modules
    if (verbose->count != 0) {
        verbose_val = true;
    }
    if (debug->count != 0) {
        debug_val = true;
    }
    //if (fmt->count != 0) {
    //    fmt_val = sub_fmt_cmp(fmt->sval[0]);
    //}
    if (timeout->count != 0) {
        timeout_val = timeout->ival[0];
    }
    if (retries->count != 0) {
        tries_val = 1 + retries->ival[0];
    }

    // Socket field is required, and argtable will flag an error if not present
    FILL_STRINGARG(socket, socket_val);
    intf_val = INTF_socket;
    
    /// Input command string may be taken from command line or fed by stdin.
    /// If no command string is present, then use pipe stdin.
    if (cmdstr->count > 0) {
        char* cursor;
        cmdstr_size = 0;
        
        for (int i=0; i<cmdstr->count; i++) {
            cmdstr_size += strlen(cmdstr->sval[i]) + 1;
        }
        
        cmdstr_val = malloc(cmdstr_size);
        if (cmdstr_val == NULL) {
            goto main_FINISH;
        }
        
        cursor = cmdstr_val;
        for (int i=0; i<cmdstr->count; i++) {
            cursor      = stpcpy(cursor, cmdstr->sval[i]);
            *cursor++   = ' ';
        }
    }
    else {
        ///@todo 1024 should be configurable
        cmdstr_val = malloc(1024);
        if (cmdstr_val == NULL) {
            goto main_FINISH;
        }
        if (sub_readline(&cmdstr_size, STDIN_FILENO, cmdstr_val, 1024) <= 0) {
            goto main_FINISH;
        }
    }
    if (cmdstr_size == 0) {
        goto main_FINISH;
    }

    /// Set cliopt struct with derived variables
    cliopt_init(&cliopts);
    cliopt_setverbose(verbose_val);
    cliopt_setdebug(debug_val);
    cliopt_settimeout(timeout_val);
    cliopt_settries(tries_val);
    
    /// All configuration is done.
    /// Send all configuration data to program main function.
    bailout = false;
    
    main_FINISH:
    arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
    
    if (bailout == false) {
        exitcode = ottercat_main(intf_val, (const char*)socket_val, cmdstr_val);
    }
    
    free(socket_val);
    free(cmdstr_val);

    return exitcode;
}



