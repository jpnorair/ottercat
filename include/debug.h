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

#ifndef debug_h
#define debug_h

#include "cliopt.h"
#include "ottercat_cfg.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>


/// Set __DEBUG__ during compilation to enable debug features (mainly printing)

#define _HEX_(HEX, SIZE, ...)  do { \
    fprintf(stderr, _E_YEL"DEBUG: "_E_NRM __VA_ARGS__); \
    fprintf(stderr, _E_YEL"DEBUG: "_E_NRM); \
    for (int i=0; i<(SIZE); i++) {   \
        fprintf(stderr, "%02X ", (HEX)[i]);   \
    } \
    fprintf(stderr, "\n"); \
} while (0)


#if defined(__DEBUG__)
#   define DEBUG_RUN(CODE)      do { CODE } while(0)
#   define DEBUG_PRINTF(...)    do { if (cliopt_isdebug()) {fprintf(stderr, _E_YEL"DEBUG: " __VA_ARGS__); fprintf(stderr, _E_NRM);}} while(0)
#   define HEX_DUMP(HEX, SIZE, ...) do { if (cliopt_isdebug()) { _HEX_(HEX, SIZE, __VA_ARGS__); } } while(0)

#else
#   define DEBUG_RUN(CODE)      do { } while(0)
#   define DEBUG_PRINTF(...)    do { } while(0)
#   define HEX_DUMP(HEX, SIZE, ...) do { } while(0)

#endif

#define ERRMARK                 _E_RED"ERR: "_E_NRM
#define ERR_PRINTF(...)         do { if (cliopt_isverbose()) { fprintf(stdout, _E_RED "ERR: " _E_NRM __VA_ARGS__); fflush(stdout); }} while(0)
#define VERBOSE_PRINTF(...)     do { if (cliopt_isverbose()) { fprintf(stdout, _E_CYN "MSG: " _E_NRM __VA_ARGS__); fflush(stdout); }} while(0)
#define VDATA_PRINTF(...)       do { if (cliopt_isverbose()) { fprintf(stdout, _E_GRN "DATA: " _E_NRM __VA_ARGS__); fflush(stdout); }} while(0)
#define VCLIENT_PRINTF(...)     do { if (cliopt_isverbose()) { fprintf(stdout, _E_MAG "CLIENT: " _E_NRM __VA_ARGS__); fflush(stdout); }} while(0)





#endif /* debug_h */
