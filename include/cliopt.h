/* Copyright 2017, JP Norair
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

#ifndef cliopt_h
#define cliopt_h

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    INTF_interactive  = 0,
    INTF_pipe = 1,
    INTF_socket = 2,
    INTF_max
} INTF_Type;


typedef enum {
    FORMAT_Default  = 0,
    FORMAT_Json     = 1,
    FORMAT_JsonHex  = 2,
    FORMAT_Bintex   = 3,
    FORMAT_Hex      = 4,
    FORMAT_MAX
} FORMAT_Type;


typedef struct {
    bool        verbose_on;
    bool        debug_on;

    FORMAT_Type format;
    
    size_t      mempool_size;
    int         timeout_ms;
    int         tries;
} cliopt_t;


cliopt_t* cliopt_init(cliopt_t* new_master);


void cliopt_setverbose(bool val);
bool cliopt_isverbose(void);

void cliopt_setdebug(bool val);
bool cliopt_isdebug(void);

FORMAT_Type cliopt_getformat(void);

size_t cliopt_getpoolsize(void);
void cliopt_setpoolsize(size_t poolsize);

int cliopt_gettimeout(void);
void cliopt_settimeout(int timeout_ms);

int cliopt_gettries(void);
void cliopt_settries(int timeout_ms);

#endif /* cliopt_h */
