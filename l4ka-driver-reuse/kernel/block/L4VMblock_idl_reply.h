/*********************************************************************
 *                
 * Copyright (C) 2004 Joshua LeVasseur
 *
 * File path:	linuxblock/L4VMblock_idl_reply.h
 * Description:	Implementation of idl4 compatible inlined functions 
 * 		to send propagated IPC.
 *
 * Proprietary!  DO NOT DISTRIBUTE!
 *
 * $Id: L4VMblock_idl_reply.h,v 1.1 2006/09/21 09:28:35 joshua Exp $
 *                
 ********************************************************************/
#ifndef __LINUXBLOCK__L4VMBLOCK_IDL_REPLY_H__
#define __LINUXBLOCK__L4VMBLOCK_IDL_REPLY_H__

static inline void IVMblock_Control_probe_propagate_reply(CORBA_Object _client, IVMblock_devprobe_t *probe_data, idl4_server_environment *_env)
{
  long dummy, msgtag;
  struct _reply_buffer {
    struct {
      long _msgtag;
      IVMblock_devprobe_t probe_data;
    } _out;
  } *_par = (struct _reply_buffer *)MyUTCB();

  /* marshal reply */

  if( IDL4_EXPECT_FALSE(_env && _env->_action) )
      msgtag = (1 << 12)+(_env->_action << 16)+(0 << 6)+0;
  else
  {
      msgtag = (1 << 12)+(0 << 6)+8;
      _par->_out.probe_data = *probe_data;
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


static inline void IVMblock_Control_attach_propagate_reply(CORBA_Object _client, IVMblock_handle_t *handle, idl4_server_environment *_env)
{
  long dummy, msgtag;
  struct _reply_buffer {
    struct {
      long _msgtag;
      IVMblock_handle_t handle;
    } _out;
  } *_par = (struct _reply_buffer *)MyUTCB();

  /* marshal reply */
  
  if( IDL4_EXPECT_FALSE(_env && _env->_action) )
      msgtag = (1 << 12)+(_env->_action << 16)+(0 << 6)+0;
  else
  {
      msgtag = (1 << 12)+(0 << 6)+1;
      _par->_out.handle = *handle;
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


static inline void IVMblock_Control_detach_propagate_reply(CORBA_Object _client, idl4_server_environment *_env)
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
      msgtag = (1 << 12)+(0 << 6)+0;
 
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

static inline void IVMblock_Control_register_propagate_reply(CORBA_Object _client, IVMblock_handle_t *client_handle, idl4_fpage_t *client_config, idl4_fpage_t *server_config, idl4_server_environment *_env)

{
  long dummy, msgtag;
  struct _reply_buffer {
    struct {
      long _msgtag;
      IVMblock_handle_t client_handle;
      idl4_mapitem client_config;
      idl4_mapitem server_config;
    } _out;
  } *_par = (struct _reply_buffer *)MyUTCB();

  /* marshal reply */
  
  if( IDL4_EXPECT_FALSE(_env && _env->_action) )
      msgtag = (1 << 12)+(_env->_action << 16)+(0 << 6)+0;
  else
  {
      msgtag = (1 << 12)+(_env->_action << 16)+(4 << 6)+1;
      _par->_out.client_handle = *client_handle;
      _par->_out.client_config = *client_config;
      _par->_out.server_config = *server_config;
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

static inline void IVMblock_Control_reattach_propagate_reply(CORBA_Object _client, idl4_fpage_t *client_config, idl4_fpage_t *server_config, idl4_server_environment *_env)

{
  long dummy, msgtag;
  struct _reply_buffer {
    struct {
      long _msgtag;
      idl4_mapitem client_config;
      idl4_mapitem server_config;
    } _out;
  } *_par = (struct _reply_buffer *)MyUTCB();

  /* marshal reply */
  
  if( IDL4_EXPECT_FALSE(_env && _env->_action) )
      msgtag = (1 << 12)+(_env->_action << 16)+(0 << 6)+0;
  else
  {
      msgtag = (1 << 12)+(_env->_action << 16)+(4 << 6);
      _par->_out.client_config = *client_config;
      _par->_out.server_config = *server_config;
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



#endif	/* __LINUXBLOCK__L4VMBLOCK_IDL_REPLY_H__ */
