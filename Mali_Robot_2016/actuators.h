#ifndef ACTUATORS_H_
#define ACTUATORS_H_

#include "can.h"


void servo_init(unsigned int f_pwm);
void servo_position1(unsigned char dutyCycle);
#endif