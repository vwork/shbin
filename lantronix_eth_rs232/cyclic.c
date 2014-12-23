#include <string.h>
#include "typedefs.h"
#include "cyclic.h"

#define min(a,b) ((a) > (b))?(b):(a)
#define max(a,b) ((a) < (b))?(b):(a)
CYC_BUF_T  snd_rs232_buf;//буфер отправки в rs232 порт
CYC_BUF_T  rcv_rs232_buf;//буфер приема из rs232 порта
unsigned char linear_buffer_snd[RS232_SND_BUF_LENGTH];
unsigned char linear_buffer_rcv[RS232_RCV_BUF_LENGTH];

//первичная инициализация всех циклических буферов
BOOL init_cyclic_buffers()
{
	snd_rs232_buf.len = sizeof(linear_buffer_snd);
	snd_rs232_buf.buf = linear_buffer_snd;
	clear_cyclic_buf(&snd_rs232_buf);

	rcv_rs232_buf.len = sizeof(linear_buffer_rcv);
	rcv_rs232_buf.buf = linear_buffer_rcv;
    	clear_cyclic_buf(&rcv_rs232_buf);

    	return TRUE;
}


//запрос количества свободных для записи ячеек в кольцевом буфере pcyc_buf
static unsigned int __get_cyclic_buf_free(CYC_BUF_T * pcyc_buf)
{
    if (pcyc_buf -> write_pos > pcyc_buf -> read_pos)
        return pcyc_buf -> len - (pcyc_buf -> write_pos - pcyc_buf -> read_pos) - 1;
    else
        if  (pcyc_buf -> write_pos == pcyc_buf -> read_pos)
            return pcyc_buf -> len - 1;
        else
            return pcyc_buf -> read_pos - pcyc_buf -> write_pos - 1;
}

//функция определяет, пуст ли циклический буфер
static inline BOOL __is_cyclic_buf_empty(CYC_BUF_T * pcyc_buf)
{
    return (__get_cyclic_buf_free(pcyc_buf) == pcyc_buf -> len - 1 ? TRUE:FALSE);
}


//сброс циклического буфера(очистка)
void clear_cyclic_buf(CYC_BUF_T * pcyc_buf)
{
    memset(pcyc_buf -> buf,0,pcyc_buf -> len);
    pcyc_buf -> read_pos = 0;
    pcyc_buf -> write_pos = 0;
}

static inline BOOL __put_to_cyclic_buf(CYC_BUF_T * pcyc_buf, unsigned char * in_buf,unsigned int len)
{
    register unsigned int write_pos = pcyc_buf -> write_pos;//для того чтобы другой поток не отхватил часть пакета
    unsigned int cpy = 0,cpy_index = 0;
    if (__get_cyclic_buf_free(pcyc_buf) < len)
        return FALSE;
    while (len)
    {
        cpy = min(len,pcyc_buf -> len - write_pos);
        if (cpy)
            memcpy(&pcyc_buf ->buf[write_pos],&in_buf[cpy_index],cpy);
        else
            continue;
        write_pos += cpy;
        write_pos %= pcyc_buf -> len;
        len -= cpy;
        cpy_index += cpy;
    }
    pcyc_buf -> write_pos = write_pos;
    return TRUE;
}

static  inline BOOL __get_from_cyclic_buf(CYC_BUF_T * pcyc_buf, unsigned char * out_buf,unsigned int * len)
{
    unsigned int __len = 0;
    unsigned int cpy = 0,cpy_index = 0;

    //если буфер пуст возвращаем 0 байт и значение false
    if (__is_cyclic_buf_empty(pcyc_buf))
    {
        *len = 0;
        return FALSE;
    }

    //смотрим, сколько байт мы можем прочитать
   *len = min(*len, pcyc_buf -> len - __get_cyclic_buf_free(pcyc_buf) - 1);
    __len = *len;
    //читаем
    while (__len)
    {
        cpy = min(__len,pcyc_buf -> len - pcyc_buf -> read_pos);
        if (cpy)
            memcpy(&out_buf[cpy_index],&pcyc_buf ->buf[pcyc_buf -> read_pos],cpy);
        pcyc_buf -> read_pos += cpy;
        pcyc_buf -> read_pos %= pcyc_buf -> len;
        __len -= cpy;
        cpy_index += cpy;
    }
    return TRUE;
}


//Положить данные по адресу in_buf длиной len байт в кольцевой буфер rs232 на передачу
//возвращает true, если данные успешно положены, иначе false
BOOL put_to_rs232_xmt_buf(unsigned char * in_buf,unsigned int len)
{
    return __put_to_cyclic_buf(&snd_rs232_buf, in_buf,len);
}

//Запросить данные из кольцевого буфера rs232 для передачи в порт
//возвращает true, если данные успешно извлечены, иначе false
//len- размер запрашиваемых данных, возвращает количество действительно запрошенных данных
BOOL get_from_rs232_xmt_buf(unsigned char * out_buf,unsigned int * len)
{
    return __get_from_cyclic_buf(&snd_rs232_buf, out_buf, len);
}

//пуст ли кольцевой буфер отсылки на rs232
BOOL is_rs232_xmt_buf_empty()
{
    return __is_cyclic_buf_empty(&snd_rs232_buf);
}

//Положить данные по адресу in_buf длиной len байт в кольцевой буфер rs232 на прием
//возвращает true, если данные успешно положены, иначе false
BOOL put_to_rs232_rcv_buf(unsigned char * in_buf,unsigned int len)
{
    return __put_to_cyclic_buf(&rcv_rs232_buf, in_buf,len);
}

//Запросить данные из кольцевого буфера rs232 принятого с порта
//возвращает true, если данные успешно извлечены, иначе false
//len- размер запрашиваемых данных, возвращает количество действительно запрошенных данных
BOOL get_from_rs232_rcv_buf(unsigned char * out_buf,unsigned int * len)
{
    return __get_from_cyclic_buf(&rcv_rs232_buf, out_buf, len);
}

//пуст ли кольцевой буфер приема из rs232
BOOL is_rs232_rcv_buf_empty()
{
    return __is_cyclic_buf_empty(&rcv_rs232_buf);
}
