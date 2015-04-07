#ifndef _ETH_INTERFACE_H_
#define _RTH_INTERFACE_H_
#include "typedefs.h"
#define MAX_SOCKETS_NUM 8 //максимальное количество одновременно обслуживаемых сокетов
#define ETH_LATENCY_MS 50 //максимальная задержка обработки запроса по сети, мс
#define MAX_SOCKET_PACKAGE_SIZE 256//максимальное количество байт в пакете данных

#define	USB_bFLAG_DATA		0x7e
#define	USB_bSTAF_DATA		0x7d
//инициализация ethernet сервера, возвращает 0 в случае успеха, либо код ошибки
int eth_init();
//основной цикл прослушивания порта сервера(обычно кидается в поток)
//содержит внутри цикл while
void eth_listen_loop();

//Запись в сокет с индексом socket_index массива pBuf длиной len. Возвращает FALSE(0) в случае ошибки
BOOL eth_socket_write(int socket_index,unsigned char * pBuf, int len);

typedef struct _socket_data_t
{
	int socket;//номер сокета
	int buf_index;//текущий адрес байта для записи
	unsigned char buf[MAX_SOCKET_PACKAGE_SIZE];//буфер для принятых данных
	BOOL staf_flag;//последний принятый байт данных- это стаффинг байт??
	int staf_bytes;//количество стафф- байт в текущем полученном сообщении
} SOCKET_DATA_T;
#endif
