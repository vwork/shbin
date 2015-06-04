#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/signal.h>
#include <sys/types.h>
#define	USB_bFLAG_DATA		0x7e
#define	USB_bSTAF_DATA		0x7d

int rs232_init( char* port_name ) {
	fprintf( stderr, "opening %s\n", port_name );

	struct termios options;
	int tty;

	tty = open( port_name, O_RDWR | O_NOCTTY | O_NONBLOCK );
	if ( tty < 0 )
	{
		fprintf( stderr, "Error: open rs232 port\n" );
		perror( "Code:" );
		return -1;
	}
	tcgetattr( tty, &options ); /*читает пораметры порта*/

	cfsetispeed( &options, B4800 ); /*установка скорости порта*/
	cfsetospeed( &options, B4800 ); /*установка скорости порта*/

	options.c_iflag &= ( ~( ICRNL | INLCR ) ); /* Stop \r -> \n & \n -> \r translation on input */
	options.c_iflag |= ( /*IGNCR |*/ IGNBRK );  /* Ignore \r & XON/XOFF on input & ignore break*/
	options.c_iflag &= ~( INPCK | IGNPAR | PARMRK | ISTRIP | IXANY | ICRNL | IGNCR );
	options.c_iflag &= ~( IXON | IXOFF );

	//output
	options.c_oflag &= ~( OPOST | OCRNL | ONLCR );/*stop \r -> \n & \n -> \r translations on output*/

	//control
	options.c_cflag &= ~( CSTOPB | CSIZE | PARODD | PARENB ); /*выкл 2-х стобит, вкл 1 стопбит*/
	options.c_cflag |= ( CS8 | CLOCAL | CREAD ); /*вкл 8бит*/
	options.c_cflag  &= ~CRTSCTS;

	//local
	options.c_lflag &= ~( ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG );

	//characters
	options.c_cc[ VMIN ] = 1;
	options.c_cc[ VTIME ] = 0;

	tcsetattr( tty, TCSANOW, &options ); /*сохранение параметров порта*/

	return tty;
}

typedef int( *READ )( char* );
typedef int( *WRITE )( char*, int );

int read_msg( char* buf, READ read ) {
	int msg_bytes = 0;
	int msg_len = 0;
	int nread;
	char byte;
	int stuff;
	int body_len = 0;
	while ( msg_len != body_len + 7 ) {
		nread = read( &byte );
		if ( nread < 0 )
			return -1;
		if ( nread == 0 ) {
			// fprintf( stderr, "." );
			usleep( 1000 * 1000 ); // 10ms
			continue;
		}
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
	fprintf( stderr, "#%d\n", msg_bytes );
	return msg_bytes;
}

int copy_msg( READ read, READ clear, WRITE write ) {
	char buf[ 256 + 7 ];
	int rlen, wlen;
	rlen = read_msg( buf, read );
	if ( rlen <= 0 ) {
		fprintf( stderr, "cannot read message\n" );
		return 0;
	}
	if ( clear )
		while ( clear( buf ) > 0 );
	wlen = write( buf, rlen );
	if ( wlen != rlen ) {
		fprintf( stderr, "cannot write message\n" );
		return 0;
	}
	return 1;
}

void copy_messages( READ tread, WRITE twrite, READ tclear, READ sread, WRITE swrite ) {
	char buf[ 1 ];
	while ( 1 ) {
		fprintf( stderr, "copying to tty\n" );
		if ( !copy_msg( sread, tclear, twrite ) ) {
			fprintf( stderr, "error while copying message to tty\n" );
			return;
		}
		fprintf( stderr, "copying from tty\n" );
		if ( !copy_msg( tread, NULL, swrite ) ) {
			fprintf( stderr, "error while copying message from tty\n" );
			return;
		}
	}
}

int tty;

#define WAIT_MS		200
#define TIMEOUT_MS	9000

int tread( char* buf ) {
	int ret;
	int t = 0;
	while ( 1 ) {
		errno = 0;
		ret = read( tty, buf, 1 );
		if ( ret < 0 && errno == 11 ) { // Resource temporarily unavailable
			t += WAIT_MS;
			if ( t > TIMEOUT_MS )
				break;
			usleep( WAIT_MS * 1000 );
			continue;
		}
		break;
	}
	if ( ret > 0 )
		fprintf( stderr, "%02x ", ( unsigned char )( *buf ) );
	return ret;
}

int tclear( char* buf ) {
	return read( tty, buf, 1 );
}

int twrite( char* buf, int len ) {
	return write( tty, buf, len );
}

int sread( char* buf ) {
	return fread( buf, 1, 1, stdin ) == 1 ? 1 : -1;
}

int swrite( char* buf, int len ) {
	int ret = fwrite( buf, 1, len, stdout );
	fflush( stdout );
	return ret;
}

int main(int argc, char *argv[]) {
	fprintf( stderr, "Stdin-to-RS232 gateway\n" );
	char* port_name = "/dev/ttyACM0";
	if ( argc > 1 )
		port_name = argv[ 1 ];
	tty = rs232_init( port_name );
	if ( tty < 0 )
		exit( 3 );
	fprintf( stderr, "start to copying\n" );
	copy_messages( tread, twrite, tclear, sread, swrite );
	close( tty );
}
