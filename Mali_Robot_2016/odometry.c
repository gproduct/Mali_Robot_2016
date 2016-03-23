#include "odometry.h"
#include "can.h"
#include <stdlib.h>
#include <util/delay.h>
#include <math.h>

char stop(char type)
{
	unsigned char buffer[8];
	
	while(1)
	{
		buffer[0] = type;
		
		while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
		_delay_ms(50);
		
		buffer[0] = 'P';
		while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
		_delay_ms(50);
		CAN_Read(buffer, DRIVER_RX_IDENTIFICATOR);
		
		if(buffer[0] != MOVING && buffer[0] != ROTATING)
		return ODOMETRY_SUCCESS;
	}
}

char moveOnDirection(int distance, unsigned char speed, char (*callback)(unsigned long startTime))
{
	unsigned char buffer[8], callbackReturnValue;
	unsigned long time;
	
	buffer[0] = 'V';
	buffer[1] = speed;
	while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
	_delay_ms(50);
	
	buffer[0] = 'D';
	buffer[1] = distance >> 8;
	buffer[2] = distance & 0XFF;
	while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
	_delay_ms(50);
	
	time = getSystemTime();
	while (1)
	{
		buffer[0] = 'P';
		while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
		_delay_ms(50);
		CAN_Read(buffer, DRIVER_RX_IDENTIFICATOR);
		
		if(buffer[0] == IDLE)
		return ODOMETRY_SUCCESS;
		else if(buffer[0] == STUCK)
		return ODOMETRY_STUCK;
		else if(buffer[0] == ERROR)
		return ODOMETRY_FAIL;
		
		_delay_ms(50);
		
		if(callback != NULL)
		{
			callbackReturnValue = callback(time);
			if(callbackReturnValue == 1)
			return ODOMETRY_FAIL;
			if(callbackReturnValue == 2)
			return ODOMETRY_CALLBACK_RETURN;
		}
	}
}

char gotoXY(position coordinates, unsigned char speed, signed char direction, char (*callback)(unsigned long startTime))
{
	unsigned char buffer[8], callbackReturnValue;
	unsigned long time;
	
	buffer[0] = 'V';
	buffer[1] = speed;
	while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
	_delay_ms(50);
	
	buffer[0] = 'G';
	buffer[1] = coordinates.x >> 8;
	buffer[2] = coordinates.x & 0XFF;
	buffer[3] = coordinates.y >> 8;
	buffer[4] = coordinates.y & 0XFF;
	buffer[5] = 0;//Mozda ne treba 0
	buffer[6] = direction;
	while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
	_delay_ms(50);
	
	time = getSystemTime();
	while (1)
	{
		buffer[0] = 'P';
		while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
		_delay_ms(50);
		CAN_Read(buffer, DRIVER_RX_IDENTIFICATOR);
		
		if(buffer[0] == IDLE)
		return ODOMETRY_SUCCESS;
		else if(buffer[0] == STUCK)
		return ODOMETRY_STUCK;
		else if(buffer[0] == ERROR)
		return ODOMETRY_FAIL;
		
		_delay_ms(50);
		if(buffer[0] != 'R' && callback != NULL)
		{
			callbackReturnValue = callback(time);
			if(callbackReturnValue == 1)
			return ODOMETRY_FAIL;
			if(callbackReturnValue == 2)
			return ODOMETRY_CALLBACK_RETURN;
		}
	}
}

char setPosition(position coordinates)
{
	unsigned char buffer[8];
	position temp;
	
	while(1)
	{
		buffer[0] = 'I';
		buffer[1] = coordinates.x >> 8;
		buffer[2] = coordinates.x & 0XFF;
		buffer[3] = coordinates.y >> 8;
		buffer[4] = coordinates.y & 0XFF;
		buffer[5] = coordinates.angle << 8;
		buffer[6] = coordinates.angle & 0XFF;
		
		while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
		_delay_ms(50); // DA LI TREBA PROVERA ZA ERROR?
		
		buffer[0] = 'P';
		while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
		_delay_ms(50);
		CAN_Read(buffer, DRIVER_RX_IDENTIFICATOR);
		
		temp.x = (buffer[1] << 8) | buffer[2];
		temp.y = (buffer[3] << 8) | buffer[4];
		temp.angle = (buffer[5] << 8) | buffer[6];
		
		if(!((fabs(temp.x - coordinates.x) > 5) || (fabs(temp.y - coordinates.y) > 5 || (fabs(temp.angle - coordinates.angle) > 5))))
		return ODOMETRY_SUCCESS;
	}
}

char rotateFor(int angle,unsigned char speed, char (*callback)(unsigned long startTime))
{
	unsigned char buffer[8], callbackReturnValue;
	unsigned long time;
	
	buffer[0] = 'V';
	buffer[1] = speed;
	while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
	_delay_ms(50);
	
	buffer[0] = 'T';
	buffer[1] = angle >> 8;
	buffer[2] = angle & 0XFF;
	
	while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
	_delay_ms(50);
	
	time = getSystemTime();
	while(1)
	{
		buffer[0] = 'P';
		while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
		_delay_ms(50);
		CAN_Read(buffer, DRIVER_RX_IDENTIFICATOR);
		
		if(buffer[0] == IDLE)
		return ODOMETRY_SUCCESS;
		else if(buffer[0] == STUCK)
		return ODOMETRY_STUCK;
		else if(buffer[0] == ERROR)
		return ODOMETRY_FAIL;
		
		_delay_ms(50);
		if(callback != NULL)
		{
			callbackReturnValue = callback(time);
			if(callbackReturnValue == 1)
			return ODOMETRY_FAIL;
			if(callbackReturnValue == 2)
			return ODOMETRY_CALLBACK_RETURN;
		}
	}
}

char setAngle(int angle, unsigned char speed, char (*callback)(unsigned long startTime))
{
	unsigned char buffer[8], callbackReturnValue;
	unsigned long time;
	
	buffer[0] = 'V';
	buffer[1] = speed;
	while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
	_delay_ms(50);
	
	buffer[0] = 'A';
	buffer[1] = angle >> 8;
	buffer[2] = angle & 0XFF;
	while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
	_delay_ms(50);
	
	time = getSystemTime();
	while(1)
	{
		buffer[0] = 'P';
		while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
		_delay_ms(50);
		CAN_Read(buffer, DRIVER_RX_IDENTIFICATOR);
		
		if(buffer[0] == IDLE)
		return ODOMETRY_SUCCESS;
		else if(buffer[0] == STUCK)
		return ODOMETRY_STUCK;
		else if(buffer[0] == ERROR)
		return ODOMETRY_FAIL;
		
		_delay_ms(50);
		if(callback != NULL)
		{
			callbackReturnValue = callback(time);
			if(callbackReturnValue == 1)
			return ODOMETRY_FAIL;
			if(callbackReturnValue == 2)
			return ODOMETRY_CALLBACK_RETURN;
		}
	}
}

char getState(void)
{
	unsigned char buffer[8];
	
	buffer[0] = 'P';
	while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
	_delay_ms(50);
	CAN_Read(buffer, DRIVER_RX_IDENTIFICATOR);
	
	return buffer[0];
}

position getPosition(void)
{
	unsigned char buffer[8];
	position temp;
	
	buffer[0] = 'P';
	while(CAN_Write(buffer, DRIVER_TX_IDENTIFICATOR))
	_delay_ms(50);
	CAN_Read(buffer, DRIVER_RX_IDENTIFICATOR);
	
	temp.x = (buffer[1] << 8) | buffer[2];
	temp.y = (buffer[3] << 8) | buffer[4];
	temp.angle = (buffer[5] << 8) | buffer[6];
	
	return temp;
	
}