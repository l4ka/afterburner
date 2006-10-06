
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
