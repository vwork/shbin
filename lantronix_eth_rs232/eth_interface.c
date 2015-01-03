#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "typedefs.h"
#include "eth_interface.h"
#include "cyclic.h"

int server; 	//сокет сервера
SOCKET_DATA_T sockets[MAX_SOCKETS_NUM];//массив зарегистрированных в системе коннектов(сокетов)

pthread_t eth_thread_id;//id потока чтения данных с сокетов

//Поток, читающий данные с обслуживаемых сокетов в сети
void * eth_thread(void * args);
//чтение данных с сокета, индекс которой sock_index номер элемента массива сокетов
static inline void __eth_socket_read(int sock_index);

void close_server() {
	close( server )
}

//инициализация ethernet сервера, возвращает 0 в случае успеха, либо код ошибки
int eth_init( ETH_LISTEN_PORT )
{
   	int i;
	struct sockaddr_in6 addr;
	int thread_error;

	for (i = 0; i < MAX_SOCKETS_NUM; i++)
		sockets[i].socket = -1;//очищаем массив сокетов

	//создаем поток, который будет управлять чтением с зарегистрированных сокетов
	thread_error = pthread_create(&eth_thread_id, NULL, eth_thread,NULL);

	if (thread_error)
	{
		printf("Error: create ethernet thread\n");
		return 1;
	}

	server = socket(AF_INET6, SOCK_STREAM, 0);
    	if(server < 0)
    	{
        	perror("server socket");
        	return 2;
    	}
    	at_quick_exit( close_server );

    	addr.sin6_family = AF_INET6;		//протокол интернет
    	addr.sin6_port = htons(ETH_LISTEN_PORT);
    	addr.sin6_addr = in6addr_any;	//прослушиваем все сетевые интерфейсы
    	if(bind(server, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    	{
        	perror("server bind");
        	return 3;
    	}
	printf("Listen for incoming connections on port %d\n",ETH_LISTEN_PORT);
    	listen(server, 1);	//слушаем
	return 0;
}

//добавить новый коннект к обслуживаемым
static inline void __eth_add_connection(int sock_num)
{
	int i;
	//смотрим, есть ли свободная ячейка для нового сокета
	for (i = 0; i < MAX_SOCKETS_NUM; i++)
	{
		if (sockets[i].socket == -1)
			break;
	}
	if (i == MAX_SOCKETS_NUM)
	{
		printf("ERROR: too many sessions, closing new socket\n");
		close(sock_num);
	}
	else
	{
		sockets[i].socket  = sock_num;
		sockets[i].staf_flag = FALSE;
		sockets[i].buf_index = 0;
		sockets[i].staf_bytes = 0;

	}
}

//удалить коннект из обслуживаемых
static inline void __eth_del_connection(int sock_num)
{
	int i;
	//очистка массива сокетов от данного сокета
	for (i = 0; i < MAX_SOCKETS_NUM; i++)
	{
		if (sockets[i].socket == sock_num)
			sockets[i].socket = -1;
	}

	//закрытие сокета
	close(sock_num);
}

//основной цикл прослушивания порта сервера(обычно кидается в поток)
//содержит внутри цикл while
void eth_listen_loop()
{
	int sock;
	struct sockaddr_in6 client_addr;
	int client_addr_size;
	char str[ INET6_ADDRSTRLEN ];
	BOOL bOptVal;
	while(1)
    	{
        	sock = accept(server, (struct sockaddr *)&client_addr, &client_addr_size);	//принимаем входящее подключение- здесь поток подвисает до получение входящего коннекта
        	if(sock < 0)
        	{
            		perror("server accept");
            		exit(3);
        	}
		inet_ntop( AF_INET6, ( void * )&client_addr.sin6_addr, str, INET6_ADDRSTRLEN );
		printf( "Client %s was connected, socket %d\n", str, sock );
		fcntl(sock, F_SETFL, O_NONBLOCK);
		//TODO: сделать нормальный keepalive
		setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char*)&bOptVal, sizeof(bOptVal));
		__eth_add_connection(sock);
	}

}

//Поток, читающий данные с обслуживаемых сокетов в сети
void * eth_thread(void * args)
{
	int i;

	while (1)
	{
		//printf("hihi\n");
		for (i = 0; i < MAX_SOCKETS_NUM; i++)
			if (sockets[i].socket != -1)
				__eth_socket_read(i);
		usleep(1000 * ETH_LATENCY_MS);
	}

	pthread_exit(NULL);
}

//чтение данных с сокета, индекс которой sock_index номер элемента массива сокетов
static inline void __eth_socket_read(int sock_index)
{
	BOOL exit = FALSE;
	int i;
	unsigned char data_byte;

	do
	{
		switch (recv(sockets[sock_index].socket, &data_byte, sizeof(data_byte), 0))
		{
		case -1:
			exit = TRUE;
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
				break;
			//идем дальше- на отключение
		case 0:
			exit = TRUE;
			printf("Client was disconnected, socket %d\n",sockets[sock_index].socket);
			__eth_del_connection(sockets[sock_index].socket);
			break;

		case 1:
			if ((data_byte != USB_bFLAG_DATA) && (sockets[sock_index].buf_index == 0))
                		break;//ошибка во время начала последовательности приняли неверный айт начала последоательности. игнорируем

            		if ((data_byte == USB_bSTAF_DATA) && (!sockets[sock_index].staf_flag))//если мы получили первый стафсимвол
            		{
                		sockets[sock_index].staf_flag = TRUE;
            			sockets[sock_index].buf[sockets[sock_index].buf_index] = data_byte;
                		sockets[sock_index].staf_bytes++;
            			sockets[sock_index].buf_index++;
				break;
            		}

             		if (sockets[sock_index].staf_flag)//сброс стаф-флага и запись символа, следующего за стаф-символом, в буффер
                 		sockets[sock_index].staf_flag = FALSE;

            		sockets[sock_index].buf[sockets[sock_index].buf_index] = data_byte;
            		sockets[sock_index].buf_index++;
            		if (sockets[sock_index].buf_index >= 5)
                		//в buf[4] хранится длина пакета
                		if ((sockets[sock_index].buf_index >= 5 + sockets[sock_index].buf[4] + 2 + sockets[sock_index].staf_bytes) ||//получили весь пакет?
				 (sockets[sock_index].buf_index >= MAX_SOCKET_PACKAGE_SIZE))//пакет превысил максимальный размер буфера
                		{
					//Здесь постановка пакета в очередь на com порт
					printf("Received package from socket %d:",sockets[sock_index].socket);
					for (i = 0; i < sockets[sock_index].buf_index; i++)
						printf(" %x",sockets[sock_index].buf[i]);
					printf("\n");
					//вставляем в буфер его длину(основного сообщения) и номер отправившего сокета(8 байт вначале, перед основным буфером)

					memmove(&(sockets[sock_index].buf)[sizeof(sockets[sock_index].buf_index) + sizeof(sock_index)],sockets[sock_index].buf,sockets[sock_index].buf_index);
					memcpy(sockets[sock_index].buf,&sockets[sock_index].buf_index,sizeof(sockets[sock_index].buf_index));
					memcpy(&(sockets[sock_index].buf)[sizeof(sockets[sock_index].buf_index)],&sock_index,sizeof(sock_index));

					if (!put_to_rs232_xmt_buf(sockets[sock_index].buf,sockets[sock_index].buf_index + sizeof(sockets[sock_index].buf_index) + sizeof(sock_index)))
						printf("Error: put in rs232 send buffer\n");

					sockets[sock_index].buf_index = 0;
					sockets[sock_index].staf_bytes = 0;
					sockets[sock_index].staf_flag = FALSE;
                		}
			break;
		default:
			exit = TRUE;
			break;

		}
	}while (!exit);
}

//Запись в сокет с индексом socket_index массива pBuf длиной len. Возвращает FALSE(0) в случае ошибки
BOOL eth_socket_write(int socket_index,unsigned char * pBuf, int len)
{
	if (sockets[socket_index].socket == -1)
		return FALSE;//значит клиент уже отключился, ответ не высылаем

	printf("RS232 -> ETH: % d bytes to socket %d\n",len,sockets[socket_index].socket);
	if (send(sockets[socket_index].socket, pBuf, len, 0) == -1)
		if (errno == EWOULDBLOCK)
			return TRUE;
		else
		{
			printf("Client was disconnected, socket %d\n",sockets[socket_index].socket);
			__eth_del_connection(sockets[socket_index].socket);
			return FALSE;
		}
}
