#ifndef _RS232_INTERFACE_H_
#define _RS232_INTERFACE_H_

#define RS232_LATENCY_MS 100//Время неопределенности потока чтения из порта, записи в порт
typedef enum {
	RS232_STATE_IDLE,
	RS232_STATE_SEND,
	RS232_STATE_GET,
	RS232_STATE_ERROR
} RS232_STATE_T;
#endif
