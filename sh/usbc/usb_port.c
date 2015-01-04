#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/types.h>
#define	USB_bFLAG_DATA		0x7e
#define	USB_bSTAF_DATA		0x7d

int rs232_init( char* port_name )
{
	fprintf( stderr, "opening %s\n", port_name );

	struct termios options;
	int tty;

	tty = open(port_name, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (tty < 0)
	{
		fprintf( stderr, "Error: open rs232 port\n");
		perror("Code:");
		return -1;
	}
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

	return tty;
}
typedef int( *proc )( char*, int );

int read_msg( char* buf, proc read ) {
	int msg_bytes = 0;
	int msg_len = 0;
	int nread;
	char byte;
	int stuff;
	int body_len = 0;
	while ( msg_len != body_len + 7 ) {
		nread = read( &byte, 1 );
		if ( nread == 0 )
			return -1;
		if ( msg_bytes == 0 && byte != USB_bFLAG_DATA )
			continue;
		*buf = byte;
		++buf;
		if ( stuff )
			stuff = 0;
		else if ( byte == USB_bSTAF_DATA )
			stuff = 1;
		if ( !stuff ) {
			++msg_len;
			if ( msg_len == 5 )
				body_len = byte;
		}
		++msg_bytes;
	}
	return msg_bytes;
}

int copy_msg( proc read, proc write ) {
	char buf[ 256 + 7 ];
	int rlen, wlen;
	rlen = read_msg( buf, read );
	if ( rlen <= 0 ) {
		fprintf( stderr, "cannot read message\n" );
		return 0;
	}
	wlen = write( buf, rlen );
	if ( wlen != rlen ) {
		fprintf( stderr, "cannot write message\n" );
		return 0;
	}
	// fprintf( stderr, "\n" );
	return 1;
}

void copy_messages( proc tread, proc twrite, proc sread, proc swrite ) {
	char tmp_byte;
	while ( 1 ) {
		while ( tread( &tmp_byte, 1 ) == 1 ); // clear the tty buffer, because tty can be only a slave
		if ( !copy_msg( sread, twrite ) ) {
			fprintf( stderr, "error while copying message to tty\n" );
			return;
		}
		if ( !copy_msg( tread, swrite ) ) {
			fprintf( stderr, "error while copying message from tty\n" );
			return;
		}
	}
}

int tty;

int tread( char* buf, int len ) {
	return read( tty, buf, len );
}

int twrite( char* buf, int len ) {
	return write( tty, buf, len );
}

int sread( char* buf, int len ) {
	return fread( buf, 1, len, stdin );
}

int swrite( char* buf, int len ) {
	int ret = fwrite( buf, 1, len, stdout );
	fflush( stdout );
	return ret;
}

int main(int argc, char *argv[]) //здесь в будущем можно передавать параметры
{
	fprintf( stderr, "Stdin-to-RS232 gateway\n" );
	char* port_name = "/dev/ttyACM0";
	if ( argc > 1 )
		port_name = argv[ 1 ];
	tty = rs232_init( port_name );
	if ( tty < 0 )
		exit( 3 );
	copy_messages( tread, twrite, sread, swrite );
}
