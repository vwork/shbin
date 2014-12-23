#ifndef CYCLIC_H
#define CYCLIC_H
#include "typedefs.h"

#define RS232_SND_BUF_LENGTH 256//длина кольцевого буфера на передачу в COM порт
#define RS232_RCV_BUF_LENGTH 50//длина кольцевоо буфера для приема из COM порта

typedef struct _cyc_buf_t {
	unsigned int read_pos;//позиция начала чтения кольцевого буфера
	unsigned int write_pos;//позиция начала записи в кольцевой буфер
	unsigned char * buf;//указатель на сам линейный в памяти буфер
	unsigned int len;//длина линейного в памяти буфера в байтах
} CYC_BUF_T;



BOOL init_cyclic_buffers();//первичная инициализация всех циклических буферов

//Запросить данные из кольцевого буфера rs232 для передачи в порт
//возвращает true, если данные успешно извлечены, иначе false
//len- размер запрашиваемых данных, возвращает количество действительно запрошенных данных
BOOL get_from_rs232_xmt_buf(unsigned char * out_buf,unsigned int * len);
//Положить данные по адресу in_buf длиной len байт в кольцевой буфер rs232 на передачу
//возвращает true, если данные успешно положены, иначе false
BOOL put_to_rs232_xmt_buf(unsigned char * in_buf,unsigned int len);
//пуст ли кольцевой буфер отсылки на rs232
BOOL is_rs232_xmt_buf_empty();

//Положить данные по адресу in_buf длиной len байт в кольцевой буфер rs232 на прием
//возвращает true, если данные успешно положены, иначе false
BOOL put_to_rs232_rcv_buf(unsigned char * in_buf,unsigned int len);
//Запросить данные из кольцевого буфера rs232 принятого с порта
//возвращает true, если данные успешно извлечены, иначе false
//len- размер запрашиваемых данных, возвращает количество действительно запрошенных данных
BOOL get_from_rs232_rcv_buf(unsigned char * out_buf,unsigned int * len);
//пуст ли кольцевой буфер приема из rs232
BOOL is_rs232_rcv_buf_empty();

void clear_cyclic_buf(CYC_BUF_T * pcyc_buf);//сброс циклического буфера(очистка)
#endif // CYCLIC_H 
