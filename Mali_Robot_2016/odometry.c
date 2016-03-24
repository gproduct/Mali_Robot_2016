#include "odometry.h"
#include "can.h"
#include <stdlib.h>
#include <util/delay.h>
#include <math.h>

static uint8_t current_speed = 0;
static volatile struct odometry_position position = 
{
	.x     = 0,
	.y     = 0,
	.angle = 0,
	.state = IDLE
};

void odometry_set_speed(uint8_t speed)
{
	if(speed == current_speed)
		return;
		
	uint8_t buffer[8];
	buffer[0] = 'V';
	buffer[1] = speed;
	while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
		_delay_ms(50);
		
	current_speed = speed;
}

static void odometry_query_position(void)
{
	uint8_t buffer[8];	
	buffer[0] = 'P';
	while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
		_delay_ms(50);
	
	CAN_Read(buffer, DRIVER_RX_IDENTIFICATOR);
	
	position.state = buffer[0];
	position.x	   = (buffer[1] << 8) | buffer[2];
	position.y	   = (buffer[3] << 8) | buffer[4];
	position.angle = (buffer[5] << 8) | buffer[6];
}

static uint8_t odometry_wait_until_done(char (*callback)(uint32_t startTime))
{
	uint32_t time = getSystemTime();
	do
	{
		odometry_query_position();
		if(callback != NULL)
		{
			if(callback(time) == 1)
				return ODOMETRY_FAIL;
		}
	}while(position.state == MOVING || position.state == ROTATING);
	
	return ODOMETRY_SUCCESS;
}

void stop(int8_t type)
{
	unsigned char buffer[8];
	
	do
	{
		buffer[0] = type;
		
		while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
			_delay_ms(50);
		
		odometry_query_position(); 
	}while(position.state == MOVING || position.state == ROTATING);
}

uint8_t odometry_move_straight(int16_t distance, uint8_t speed, char (*callback)(uint32_t start_time))
{
	odometry_set_speed(speed);
	buffer[0] = 'D';
	buffer[1] = distance >> 8;
	buffer[2] = distance & 0xFF;
	while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
		_delay_ms(50);
	
	return odometry_wait_until_done(callback);
}

char gotoXY(struct odometry_position* position, uint8_t speed, uint8_t direction, char (*callback)(uint32_t startTime))
{
	uint8_t buffer[8];
	
	odometry_set_speed(speed);
	
	buffer[0] = 'G';
	buffer[1] = position.x >> 8;
	buffer[2] = position.x & 0XFF;
	buffer[3] = position.y >> 8;
	buffer[4] = position.y & 0XFF;
	buffer[5] = 0;//Mozda ne treba 0
	buffer[6] = direction;
	while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
		_delay_ms(50);
	
	return odometry_wait_until_done(callback);
}

void setPosition(struct odometry_position* new_position)
{
	uint8_t buffer[8];

	buffer[0] = 'I';
	buffer[1] = new_position->x >> 8;
	buffer[2] = new_position->x & 0xFF;
	buffer[3] = new_position->y >> 8;
	buffer[4] = new_position->y & 0xFF;
	buffer[5] = new_position->angle << 8;
	buffer[6] = new_position->angle & 0xFF;
		
	position.x	   = new_position->x;
	position.y	   = new_position->y;
	position.angle = new_position->angle;
		
	while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
		_delay_ms(50);
}
 
char rotateFor(uint16_t angle,uint8_t speed, char (*callback)(uint32_t startTime))
{
	uint8_t buffer[8];
	odometry_set_speed(speed);
	
	buffer[0] = 'T';
	buffer[1] = angle >> 8;
	buffer[2] = angle & 0xFF;
	
	while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
		_delay_ms(50);
	
	return odometry_wait_until_done(callback);
	
}

char setAngle(uint16_t angle, uint8_t speed, char (*callback)(uint32_t startTime))
{
	uint8_t buffer[8];
	odometry_set_speed(speed);
	
	buffer[0] = 'A';
	buffer[1] = angle >> 8;
	buffer[2] = angle & 0XFF;
	while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
		_delay_ms(50);
	
	return odometry_wait_until_done(callback);
}

uint8_t getState(void)
{
	return position.state;
}

int16_t odometry_get_x(void)
{
	return position.x;
}

int16_t odometry_get_y(void)
{
	return position.y;
}

int16_t odometry_get_angle(void)
{
	return position.angle;
}