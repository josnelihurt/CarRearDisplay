/*
 * main.c
 *
 *  Created on: Sep 18, 2016
 *      Author: jrb
 */
#include "rs232.h"
#include <stdio.h>
#include <memory.h>

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
unsigned int reverseBits(unsigned char num)
{
	unsigned char NO_OF_BITS = sizeof(num) * 8;
	unsigned char reverse_num = 0, i, temp;

	for (i = 0; i < NO_OF_BITS; i++)
	{
		temp = (num & (1 << i));
		if (temp)
			reverse_num |= (1 << ((NO_OF_BITS - 1) - i));
	}

	return reverse_num;
}

int i, cport_nr = 0, bdrate = 57600;
unsigned char gRamBuff[32];
extern int Cport[22];

void sortGRAM(unsigned char* gRam, unsigned char* gRamSorted)
{
	int i;
	int row;
	for (row = 0; row < 32; ++row)
	{
		gRamSorted[row] = reverseBits(gRam[row]);
	}
	return;
}

void sendSyncGRAM(unsigned char* gRam)
{
	unsigned char internalGRAM[32];
	sortGRAM(gRam, internalGRAM);
	for (i = 0; i < 32; ++i)
	{
		SendByte(cport_nr, 0x20);
		SendByte(cport_nr, i * 2);
		SendByte(cport_nr, internalGRAM[i]);
		char response[2];
		int n = read(Cport[cport_nr], response, 2);
		if (n == 2 && strcmp(response, "OK") == 0)
			printf("\nSync OK");

	}
}

void sentTest()
{
	int i;
	int iBit = 0;
	memset(gRamBuff, 0x00, sizeof(gRamBuff));
	for (i = 0; i < sizeof(gRamBuff); ++i)
	{
		for (iBit = 0; iBit < 8; ++iBit)
		{
			gRamBuff[i] = 1 << iBit;
			sendSyncGRAM(gRamBuff);
			printf("\n Sending GRAM");
			usleep(50000);
		}
	}
}

int main(int argc, char **argv)
{
	if (OpenComport(cport_nr, bdrate))
	{
		printf("Can not open comport\n");
		return (0);
	}
	else
	{
		printf("Opened Port\n");
	}

	sentTest();

	printf("Closing Port\n");
	CloseComport(cport_nr);

	return 0;
}
