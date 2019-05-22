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

#ifndef sockpush_h
#define sockpush_h

#include <stdio.h>
#include <stdint.h>


#define SP_SUB_OUTBOUND     2
#define SP_SUB_INBOUND      1



// External data types.  Use these in APIs and clients.
// ---------------------------------------------------------------------------
typedef void* sp_handle_t;
typedef void* sp_reader_t;

typedef int (*sp_action_t)(sp_handle_t, const uint8_t*, size_t);






// ---------------------------------------------------------------------------




int sp_open(sp_handle_t* handle, const char* socket_path, unsigned int flags);
int sp_close(sp_handle_t handle);

sp_reader_t sp_reader_create(void* ctx, sp_handle_t handle);
void sp_reader_purge(sp_reader_t reader);
void sp_reader_destroy(sp_reader_t reader);

int sp_read(sp_reader_t reader, uint8_t* readbuf, size_t readmax, size_t timeout_ms);

//int sp_comm(sp_handle_t handle, uint8_t* readbuf, size_t readmax, uint8_t* writebuf, size_t writesize);



int sp_sendcmd(sp_handle_t handle, uint8_t* writebuf, size_t writesize);
int sp_write(sp_handle_t handle, uint8_t* writebuf, size_t writesize);


int sp_subscribe(sp_handle_t handle, sp_action_t action, int flags, uint8_t* readbuf, size_t readmax);


//int sp_dispatch(sp_handle_t handle, sp_status_t action, uint8_t* writebuf, size_t writesize);


#endif /* sockpush_h */
