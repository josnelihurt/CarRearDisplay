#define BAUDRATE 57600

#include <stdint.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/boot.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/io.h>
#include <stdio.h> 
#include "ht1632c.h"

#define UART_CALC_BAUDRATE(baudRate) ((uint32_t)((F_CPU) + ((uint32_t)baudRate * 8UL)) / ((uint32_t)(baudRate) * 16UL) - 1)

// TIMER0 with prescaler clkI/O/1024
#define TIMER0_PRESCALER (1 << CS02) | (1 << CS00)

//test 23AA55AA55AA55AA55AA55AA55AA55AA55AA55AA55AA55AA55AA55AA55AA55AA55

//#define DEBUG_PRINT printf
#define DEBUG_PRINT	//

enum rxCommand
{
	NONE = 0x00, SET_ROW = 0x20, BRIGHT = 0x21, BLINK = 0x22, GRAM = 0x23,
};

typedef enum rxCommand rxCommand_t;

static int uart_putchar(char c, FILE *stream);

static FILE uartStdout = FDEV_SETUP_STREAM(uart_putchar, NULL,
		_FDEV_SETUP_WRITE);

uint32_t timer0Counter = 0;
uint8_t isCommand = 1;
uint8_t rxParameters[32];
uint8_t parameterIndex;
rxCommand_t lastCommand;

void uart_puts(const char *s);
void sendchar(uint8_t data);
uint8_t recvchar(void);
void processCommand(uint8_t command);

ISR(TIMER0_OVF_vect)
{
	// handle interrupt
	if(timer0Counter++ > 4)
	{
		if(!isCommand)
		{
			isCommand = 1;
			uart_puts("\nt_out");
		}
		timer0Counter = 0;
		lastCommand = NONE;
		parameterIndex = 0;
	}
}

ISR(USART_RXC_vect)
{
	uint8_t value;
	char buffer[24];
	value = UDR; // Fetch the received byte value into the variable "ByteReceived"
	timer0Counter = 0;
	if(isCommand)
	{
		isCommand = 0;
		lastCommand = value;
		parameterIndex = 0;
	}
	else
	{
		rxParameters[parameterIndex++] = value;
		sprintf(buffer,"\n0x%2x[%d] = 0x%x",(int)lastCommand,parameterIndex - 1,value);
		DEBUG_PRINT(buffer);
		processCommand(lastCommand);
	}
}

static int uart_putchar(char c, FILE *stream)
{
	if (c == '\n')
		uart_putchar('\r', stream);
	loop_until_bit_is_set(UCSRA, UDRE);
	UDR = c;
	return 0;
}

void processCommand(uint8_t command)
{
	switch (command)
	{
	case SET_ROW:
		if (parameterIndex == 2)
		{
			ht1632c_data8(rxParameters[0], rxParameters[1]);
			isCommand = 1;
			printf("OK");
		}
		break;
	case BRIGHT:
		if (parameterIndex == 1)
		{
			ht1632c_bright(rxParameters[0]);
			isCommand = 1;
			printf("OK");
		}
		break;
	case BLINK:
		if (parameterIndex == 1)
		{
			ht1632c_blinkonoff(rxParameters[0]);
			isCommand = 1;
			printf("OK");
		}
		break;
	case GRAM:
		if (parameterIndex == 32)
		{
			ht1632c_flush_fb(rxParameters);
			isCommand = 1;
			printf("OK");
		}
		break;
	default:
		break;
	}
}

void sendchar(uint8_t data)
{
	while (!(UCSRA & (1 << UDRE)))
		;
	UDR = data;
}

void uart_puts(const char *s)
{
	do
	{
		sendchar(*s);
	} while (*s++);
}

uint8_t recvchar(void)
{
	while (!(UCSRA & (1 << RXC)))
		;
	return UDR;
}

void init()
{
	/* GPIO Configuration */
	DDRD &= ~(1 << PD5 | 1 << PD6 | 1 << PD7);
	PORTD = (1 << PD5 | 1 << PD6 | 1 << PD7);
	/* USART Configuration */
	UBRRH = (UART_CALC_BAUDRATE(BAUDRATE) >> 8) & 0xFF;
	UBRRL = (UART_CALC_BAUDRATE(BAUDRATE) & 0xFF);
	UCSRB = ((1 << TXEN) | (1 << RXEN));
	UCSRC = ((1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0));
	UCSRB |= (1 << RXEN) | (1 << TXEN);
	UCSRB |= (1 << RXCIE);
	/* TIMER Configuration */
	TIMSK |= (1 << TOIE0);        // interrupt enable - here overflow
	TCCR0 |= TIMER0_PRESCALER;    // use defined prescaler value
	stdout = &uartStdout;
	printf("\nBooting serial matrix display\n");
}

int check_reset()
{
	int result;
	result = PIND & (1 << PD5);
	result = !result;
	return result;
}

int main(void)
{
	uint8_t buffer[32];
	memset(buffer, 0xAA, 32);
	init();
	ht1632c_init();
	ht1632c_flush_fb(buffer);
	memset(buffer, 0x00, 32);
	_delay_ms(500);
	ht1632c_flush_fb(buffer);
	sei();
	for (;;)
	{
		if (check_reset())
		{
			printf("\nRST PB ON BOARD\n");
			wdt_enable (WDTO_15MS);
			while (1)
				;
		}
	}

	return 0;
}
