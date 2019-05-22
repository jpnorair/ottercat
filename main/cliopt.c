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


#include "cliopt.h"
#include "ottercat_cfg.h"

static cliopt_t* master;

cliopt_t* cliopt_init(cliopt_t* new_master) {
    master = new_master;
    master->verbose_on      = false;
    master->debug_on        = false;
    master->format          = FORMAT_JsonHex;
    master->mempool_size    = OTTERCAT_PARAM_MMAP_PAGESIZE;
    master->timeout_ms      = 500;
    master->tries           = 1;
    return master;
}

bool cliopt_isverbose(void) {
    return master->verbose_on;
}

bool cliopt_isdebug(void) {
    return master->debug_on;
}

void cliopt_setverbose(bool val) {
    master->verbose_on = val;
}

void cliopt_setdebug(bool val) {
    master->debug_on = val;
}


FORMAT_Type cliopt_getformat(void) {
    return master->format;
}


size_t cliopt_getpoolsize(void) {
    return master->mempool_size;
}
void cliopt_setpoolsize(size_t poolsize) {
    master->mempool_size = poolsize;
}

int cliopt_gettimeout(void) {
    return master->timeout_ms;
}
void cliopt_settimeout(int timeout_ms) {
    master->timeout_ms = timeout_ms;
}

int cliopt_gettries(void) {
    return master->tries;
}
void cliopt_settries(int retries) {
    master->tries = retries;
}
