#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/types.h>
#include "typedefs.h"
#include "cyclic.h"
#include "rs232_interface.h"
#include "eth_interface.h"

void * rs232_thread(void * args);
//основная функция автомата rs232 порта
void rs232_state_proc();
//чтение данных с ком порта, заполнение буфера
static inline int __rs232_read();

pthread_t rs232_thread_id;//id потока чтения данных с сокетов
int tty;//дескриптор порта
RS232_STATE_T rs232_state = RS232_STATE_IDLE;
unsigned char rs232_sbuf[MAX_SOCKET_PACKAGE_SIZE];//буфер для отправки в ком порт
SOCKET_DATA_T rs232_rbuf;//буфер для приема из ком порта(сделан по прототипу сокета)

void close_tty() {
	close( tty );
}

//инициализация и настройка rs232 порта, запуск потока автомата порта
int rs232_init( char* port_name )
{
	printf( "opening %s\n", port_name );

	int thread_error;
	struct termios options;

	tty = open(port_name, O_RDWR | O_NOCTTY | O_NONBLOCK);
      	if (tty < 0)
      	{
        	printf("Error: open rs232 port\n");
			perror("Code:");
        	return 1;
      	}
      	at_quick_exit( close_tty );
        tcgetattr(tty, &options); /*читает пораметры порта*/

        cfsetispeed(&options, B4800); /*установка скорости порта*/
        cfsetospeed(&options, B4800); /*установка скорости порта*/

	options.c_iflag &= (~(ICRNL | INLCR)); /* Stop \r -> \n & \n -> \r translation on input */
     	options.c_iflag |= (/*IGNCR |*/ IGNBRK);  /* Ignore \r & XON/XOFF on input & ignore break*/
 	options.c_iflag &= ~(INPCK | IGNPAR | PARMRK | ISTRIP | IXANY | ICRNL | IGNCR);
	options.c_iflag &= ~(IXON | IXOFF);

	//output
	options.c_oflag &= ~(OPOST | OCRNL | ONLCR);/*stop \r -> \n & \n -> \r translations on output*/

	//control
        options.c_cflag &= ~(CSTOPB | CSIZE | PARODD | PARENB); /*выкл 2-х стобит, вкл 1 стопбит*/
        options.c_cflag |= (CS8 | CLOCAL | CREAD); /*вкл 8бит*/
	options.c_cflag  &= ~CRTSCTS;

	//local
 	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG);

	//characters
 	options.c_cc[VMIN] = 1;
 	options.c_cc[VTIME] = 0;

        tcsetattr(tty, TCSANOW, &options); /*сохранение параметров порта*/

	thread_error = pthread_create(&rs232_thread_id,NULL,rs232_thread,NULL);

	if (thread_error)
	{
		printf("Error: create ethernet thread\n");
		return 1;
	}
	return 0;
}


void * rs232_thread(void * args)
{
	while (1)
	{
		rs232_state_proc();
		usleep(RS232_LATENCY_MS*1000);
	}

	pthread_exit(NULL);
	close(tty);

}


//основная функция автомата rs232 порта
void rs232_state_proc()
{
	static int current_socket = -1;
	int read_length,msg_length;
	int write_length;
	static int timeout;
	unsigned char tmp_byte;
	int i;

	switch(rs232_state)
	{
	case RS232_STATE_IDLE:
		while (read(tty,&tmp_byte,1) == 1);//(printf("!"));//очистка буфера от ненужных данных ком порта. Так как ком порт может быть лишь слейвом

		if (is_rs232_xmt_buf_empty())
			break;
		read_length = sizeof(int);

		if (!get_from_rs232_xmt_buf((unsigned char *)&msg_length,&read_length))
		{
			printf("Error: cyclic buffer read message length\n");
			return;
		}
		read_length = sizeof(int);
		if (!get_from_rs232_xmt_buf((unsigned char *)&current_socket,&read_length))
		{
			printf("Error: cyclic buffer read socket id\n");
			return;
		}

		read_length = msg_length;
		if (read_length > MAX_SOCKET_PACKAGE_SIZE)
		{
			printf("Error: cyclic buffer long message");
			return;
		}

		if (!get_from_rs232_xmt_buf(rs232_sbuf,&read_length))
		{
			printf("Error: cyclic buffer read message\n");
			return;
		}

		if (read_length != msg_length)
		{
			printf("Error:cyclic buffer part message,read %d,need %d\n",read_length,msg_length);
			return;
		}

		write_length = write(tty,rs232_sbuf,msg_length);
		if (write_length == -1)
		{
			printf("Error: rs232 write operation\n");
			quick_exit( EXIT_FAILURE );
		}
		printf("ETH -> RS232: %d bytes\n",write_length);
		rs232_state = RS232_STATE_GET;
		timeout = 15;
		break;

	case RS232_STATE_SEND:
		break;

	case RS232_STATE_GET:

		if (read_length = __rs232_read())
		{
			//кидаем его в сокет
			eth_socket_write(current_socket,rs232_rbuf.buf,read_length);
			rs232_rbuf.buf_index = 0;
			rs232_rbuf.staf_bytes = 0;
			rs232_rbuf.staf_flag = FALSE;
			rs232_state = RS232_STATE_IDLE;
			break;//если прочитали целиком сообщение
		}
		timeout--;
		if (timeout <= 0)
		{	rs232_state = RS232_STATE_ERROR;
               		printf("get %d, need %d\n",rs232_rbuf.buf_index,5 + rs232_rbuf.buf[4] + 2 + rs232_rbuf.staf_bytes);
		}
		break;

	case RS232_STATE_ERROR:
		printf("Error: RS232 timeout\n");
		rs232_rbuf.buf_index = 0;
		rs232_rbuf.staf_bytes = 0;
		rs232_rbuf.staf_flag = FALSE;
		rs232_state = RS232_STATE_IDLE;
		break;
	}

}


//чтение данных с ком порта, заполнение буфера
static inline int __rs232_read()
{
	unsigned char data_byte;
	int i;
	int read_byte;
	while (1)
	{
		read_byte = read(tty,&data_byte,1);
		if (read_byte < 1)
			return 0;
		if ((data_byte != USB_bFLAG_DATA) && (rs232_rbuf.buf_index == 0))
               	{
			printf("?");
			break;//ошибка во время начала последовательности приняли неверный айт начала последоательности. игнорируем
		}
         	if ((data_byte == USB_bSTAF_DATA) && (!rs232_rbuf.staf_flag))//если мы получили первый стафсимвол
            	{
               		rs232_rbuf.staf_flag = TRUE;
               		rs232_rbuf.staf_bytes++;
            		rs232_rbuf.buf[rs232_rbuf.buf_index] = data_byte;
            		rs232_rbuf.buf_index++;
			break;
            	}

             	if (rs232_rbuf.staf_flag)//сброс стаф-флага и запись символа, следующего за стаф-символом, в буффер
               		rs232_rbuf.staf_flag = FALSE;

            	rs232_rbuf.buf[rs232_rbuf.buf_index] = data_byte;
            	rs232_rbuf.buf_index++;
            	if (rs232_rbuf.buf_index >= 5)
               		//в buf[4] хранится длина пакета
               		if ((rs232_rbuf.buf_index >= 5 + rs232_rbuf.buf[4] + 2 + rs232_rbuf.staf_bytes) ||//получили весь пакет?
			 (rs232_rbuf.buf_index >= MAX_SOCKET_PACKAGE_SIZE))//пакет превысил максимальный размер буфера
               		{
				//Здесь постановка пакета в очередь на com порт
				printf("Received package from modem: ");
				for (i = 0; i < rs232_rbuf.buf_index; i++)
					printf(" %x",rs232_rbuf.buf[i]);
				printf("\n");
				return rs232_rbuf.buf_index;
               		}
	}
	return 0;

}
