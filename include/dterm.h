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

#ifndef dterm_h
#define dterm_h

// Configuration Header
#include "cliopt.h"
#include "ottercat_cfg.h"

// HB Libraries
#include <talloc.h>

// Standard C & POSIX Libraries
#include <stdbool.h>
#include <stdint.h>

//#include <stdlib.h>


#define LINESIZE            1024
#define CMDSIZE             LINESIZE        //CMDSIZE is deprecated

typedef enum {
    DFMT_Binary,
    DFMT_Text,
    DFMT_Native,
    DFMT_Max
} DFMT_Type;


typedef struct {
    // fd_in, fd_out are used by controlling interface.
    // Usage will differ in case of interactive, pipe, socket
    int in;
    int out;
    int squelch;
} dterm_fd_t;



typedef struct {
    // Client Thread I/O parameters.
    // Should be altered per client thread in cloned dterm_handle_t
    dterm_fd_t fd;
    
    // Process Context:
    // Thread Context: may be null if not using talloc
    TALLOC_CTX* pctx;
    TALLOC_CTX* tctx;

    bool    use_socket;
    void*   devmgr;

} dterm_handle_t;



///@todo rework the dterm module into a more normal object.
/// Presently the handle isn't a real handle, it's a struct.
int dterm_init(dterm_handle_t* dth, int fd_in, int fd_out, bool use_socket, void* devmgr_handle);
void dterm_deinit(dterm_handle_t* handle);

int dterm_cmdstream(dterm_handle_t* dth, char* stream);


#endif
