#include <stdio.h>
#include <string.h>
#include "eth_interface.h"
#include "rs232_interface.h"
#include "cyclic.h"
#define SOFT_VERSION_FIX 0
#define SOFT_VERSION_MINOR 13
#define SOFT_VERSION_MAJOR 0

int main(int argc, char *argv[]) //здесь в будущем можно передавать параметры
{
	printf("Ethernet-to-RS232 gateway\n");
	printf("Version %d.%d-%d\n",SOFT_VERSION_MAJOR,SOFT_VERSION_MINOR,SOFT_VERSION_FIX);
	if (!init_cyclic_buffers())
		exit(1);
	if (eth_init())
		exit(2);
	if (rs232_init())
		exit(3);
	eth_listen_loop();
}
