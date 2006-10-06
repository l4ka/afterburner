/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:	l4ka-drivers/linux/net/L4VMnet_idl_reply.h
 * Description:	Hand-written deceiving IPC stubs; to be replaced with IDL4
 * 		automation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 ********************************************************************/
#ifndef __L4KA_DRIVERS__L4VMNET_IDL_REPLY_H__
#define __L4KA_DRIVERS__L4VMNET_IDL_REPLY_H__

static inline void IVMnet_Control_attach_propagate_reply(CORBA_Object _client, IVMnet_handle_t *handle, idl4_fpage_t *shared_window, idl4_fpage_t *server_status, idl4_server_environment *_env)

{
  long dummy, msgtag;
  struct _reply_buffer {
    struct {
      long _msgtag;
      IVMnet_handle_t handle;
      idl4_mapitem shared_window;
      idl4_mapitem server_status;
    } _out;
  } *_par = (struct _reply_buffer *)MyUTCB();

  /* marshal reply */
  
  if( IDL4_EXPECT_FALSE(_env && _env->_action) )
      msgtag = (1 << 12)+(_env->_action << 16)+(0 << 6)+0;
  else
  {
      _par->_out.handle = *handle;
      _par->_out.shared_window = *shared_window;
      _par->_out.server_status = *server_status;
      msgtag = (1 << 12)+(1+(4<<6));
  }

  /* send message */
  
  __asm__ __volatile__(
    "push %%ebp                         \n\t"
    "call __L4_Ipc                      \n\t"
    "pop %%ebp                          \n\t"
    : "=a" (dummy), "=c" (dummy), "=d" (dummy), "=S" (dummy), "=D" (dummy)
    : "a" (_client), "c" (0x4000000), "d" (0), "S" (msgtag), "D" (_par)
    : "ebx", "memory", "cc"
  );
}


static inline void IVMnet_Control_reattach_propagate_reply(CORBA_Object _client, idl4_fpage_t *shared_window, idl4_fpage_t *server_status, idl4_server_environment *_env)

{
  long dummy, msgtag;
  struct _reply_buffer {
    struct {
      long _msgtag;
      idl4_mapitem shared_window;
      idl4_mapitem server_status;
    } _out;
  } *_par = (struct _reply_buffer *)MyUTCB();

  /* marshal reply */
  
  if( IDL4_EXPECT_FALSE(_env && _env->_action) )
      msgtag = (1 << 12)+(_env->_action << 16)+(0 << 6)+0;
  else
  {
      _par->_out.shared_window = *shared_window;
      _par->_out.server_status = *server_status;
      msgtag = (1 << 12)+(0+(4<<6));
  }

  /* send message */
  
  __asm__ __volatile__(
    "push %%ebp                         \n\t"
    "call __L4_Ipc                      \n\t"
    "pop %%ebp                          \n\t"
    : "=a" (dummy), "=c" (dummy), "=d" (dummy), "=S" (dummy), "=D" (dummy)
    : "a" (_client), "c" (0x4000000), "d" (0), "S" (msgtag), "D" (_par)
    : "ebx", "memory", "cc"
  );
}



static inline void IVMnet_Control_start_propagate_reply(CORBA_Object _client, idl4_server_environment *_env)
{
  long dummy, msgtag;
  struct _reply_buffer {
    struct {
      long _msgtag;
    } _out;
  } *_par = (struct _reply_buffer *)MyUTCB();

  if( IDL4_EXPECT_FALSE(_env && _env->_action) )
      msgtag = (1 << 12)+(_env->_action << 16)+(0 << 6)+0;
  else
  {
      msgtag = (1 << 12)+(0+(0<<6));
  }

  /* send message */
  
  __asm__ __volatile__(
    "push %%ebp                         \n\t"
    "call __L4_Ipc                      \n\t"
    "pop %%ebp                          \n\t"
    : "=a" (dummy), "=c" (dummy), "=d" (dummy), "=S" (dummy), "=D" (dummy)
    : "a" (_client), "c" (0x4000000), "d" (0), "S" (msgtag), "D" (_par)
    : "ebx", "memory", "cc"
  );
}


static inline void IVMnet_Control_stop_propagate_reply(CORBA_Object _client, idl4_server_environment *_env)
{
  long dummy, msgtag;
  struct _reply_buffer {
    struct {
      long _msgtag;
    } _out;
  } *_par = (struct _reply_buffer *)MyUTCB();

  if( IDL4_EXPECT_FALSE(_env && _env->_action) )
      msgtag = (1 << 12)+(_env->_action << 16)+(0 << 6)+0;
  else
  {
      msgtag = (1 << 12)+(0+(0<<6));
  }

  /* send message */
  
  __asm__ __volatile__(
    "push %%ebp                         \n\t"
    "call __L4_Ipc                      \n\t"
    "pop %%ebp                          \n\t"
    : "=a" (dummy), "=c" (dummy), "=d" (dummy), "=S" (dummy), "=D" (dummy)
    : "a" (_client), "c" (0x4000000), "d" (0), "S" (msgtag), "D" (_par)
    : "ebx", "memory", "cc"
  );
}


static inline void IVMnet_Control_detach_propagate_reply(CORBA_Object _client, idl4_server_environment *_env)
{
  long dummy, msgtag;
  struct _reply_buffer {
    struct {
      long _msgtag;
    } _out;
  } *_par = (struct _reply_buffer *)MyUTCB();

  if( IDL4_EXPECT_FALSE(_env && _env->_action) )
      msgtag = (1 << 12)+(_env->_action << 16)+(0 << 6)+0;
  else
  {
      msgtag = (1 << 12)+(0+(0<<6));
  }

  /* send message */
  
  __asm__ __volatile__(
    "push %%ebp                         \n\t"
    "call __L4_Ipc                      \n\t"
    "pop %%ebp                          \n\t"
    : "=a" (dummy), "=c" (dummy), "=d" (dummy), "=S" (dummy), "=D" (dummy)
    : "a" (_client), "c" (0x4000000), "d" (0), "S" (msgtag), "D" (_par)
    : "ebx", "memory", "cc"
  );
}


static inline void IVMnet_Control_update_stats_propagate_reply(CORBA_Object _client, idl4_server_environment *_env)
{
  long dummy, msgtag;
  struct _reply_buffer {
    struct {
      long _msgtag;
    } _out;
  } *_par = (struct _reply_buffer *)MyUTCB();

  if( IDL4_EXPECT_FALSE(_env && _env->_action) )
      msgtag = (1 << 12)+(_env->_action << 16)+(0 << 6)+0;
  else
  {
      msgtag = (1 << 12)+(0+(0<<6));
  }

  /* send message */
  
  __asm__ __volatile__(
    "push %%ebp                         \n\t"
    "call __L4_Ipc                      \n\t"
    "pop %%ebp                          \n\t"
    : "=a" (dummy), "=c" (dummy), "=d" (dummy), "=S" (dummy), "=D" (dummy)
    : "a" (_client), "c" (0x4000000), "d" (0), "S" (msgtag), "D" (_par)
    : "ebx", "memory", "cc"
  );
}


static inline void IVMnet_Control_register_dp83820_tx_ring_propagate_reply(CORBA_Object _client, idl4_server_environment *_env)
{
  long dummy, msgtag;
  struct _reply_buffer {
    struct {
      long _msgtag;
    } _out;
  } *_par = (struct _reply_buffer *)MyUTCB();

  if( IDL4_EXPECT_FALSE(_env && _env->_action) )
      msgtag = (1 << 12)+(_env->_action << 16)+(0 << 6)+0;
  else
  {
      msgtag = (1 << 12)+(0+(0<<6));
  }

  /* send message */
  
  __asm__ __volatile__(
    "push %%ebp                         \n\t"
    "call __L4_Ipc                      \n\t"
    "pop %%ebp                          \n\t"
    : "=a" (dummy), "=c" (dummy), "=d" (dummy), "=S" (dummy), "=D" (dummy)
    : "a" (_client), "c" (0x4000000), "d" (0), "S" (msgtag), "D" (_par)
    : "ebx", "memory", "cc"
  );
}


#endif	/* __L4KA_DRIVERS__L4VMNET_IDL_REPLY_H__ */
