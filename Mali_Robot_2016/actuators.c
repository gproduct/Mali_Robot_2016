#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include "system.h"
#include "can.h"
#include "actuators.h"

void servo_init(unsigned int f_pwm)
{
	DDRE |= (1 << PINE3) | (1 << PINE4) | (1 << PINE5);
	
	TCNT3 = 0;
	OCR3A = 0;
	OCR3B = 0;
	OCR3C = 0;
	
	TCCR3A = (1 << COM3A1) | (1 << COM3A0) | (1 << COM3B1) | (1 << COM3B0) | (1 << COM3C1) | (1 << COM3C0) | (1 << WGM31);
	TCCR3B = (1 << WGM32) | (1 << WGM33) | (1 << CS31); // PRESKALER = 1
	ICR3 = ((double)F_CPU) / (8 * f_pwm) - 0.5; // FREKVENCIJA PWMA JE ~19kHz
}//END OF servo_init


void servo_position1(unsigned char dutyCycle)
{
	/*if((dutyCycle < 65) || (dutyCycle > 175))
	{
		dutyCycle = 150;
	}*/
	OCR3A = ((double)ICR3 / 255) * dutyCycle + 0.5;
	
}//END OF servo_position