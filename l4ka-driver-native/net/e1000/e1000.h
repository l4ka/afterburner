/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     net/e1000/e1000.h
 * Description:   
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
 * $Id$
 *                
 ********************************************************************/

#ifndef __E1000_H__
#define __E1000_H__

#define E1000_FC_HIGH_THRESH	0xA9C8
#define E1000_FC_LOW_THRESH	0xA9C0
#define E1000_FC_PAUSE_TIME	0x0680

#define AUTO_ALL_MODES	0

#define E1000_MAX_INTR	15

/* Supported Rx Buffer Sizes */
#define E1000_RXBUFFER_2048	2048
#define E1000_RXBUFFER_4096	4096
#define E1000_RXBUFFER_8192	8192
#define E1000_RXBUFFER_16384	16384

#define E1000_JUMBO_PBA		0x00000028
#define E1000_DEFAULT_PBA	0x00000030

#define E1000_GET_DESC(R, i, type)      (&(((struct type *)((R).desc))[i]))
#define E1000_RX_DESC(R, i)             E1000_GET_DESC(R, i, e1000_rx_desc)
#define E1000_TX_DESC(R, i)             E1000_GET_DESC(R, i, e1000_tx_desc)
#define E1000_CONTEXT_DESC(R, i)	E1000_GET_DESC(R, i, e1000_context_desc)

/* How many Rx buffers do we bundle into one write to the hardware? */
#define E1000_RX_BUFFER_WRITE	16

#endif	/* __E1000_H__ */
