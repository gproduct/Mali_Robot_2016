#ifndef ODOMETRY_H_
#define ODOMETRY_H_

#define HARD_STOP					'S'
#define SOFT_STOP					's'

#define ODOMETRY_SUCCESS			0
#define ODOMETRY_FAIL				1
#define ODOMETRY_STUCK				2
#define ODOMETRY_CALLBACK_RETURN	3

#define LOW_SPEED					50
#define NORMAL_SPEED				90
#define HIGH_SPEED					250

#define FORWARD						1
#define BACKWARD					-1

typedef struct
{
	signed int x;
	signed int y;
	signed int angle;
}position;

typedef enum
{
	IDLE = 'I',
	MOVING = 'M',
	ROTATING = 'R',
	STUCK = 'S',
	ERROR = 'E'
}states;


// callback za odometriju vraca sledece vrednosti:
// 0- nista se ne desava, ostani u odometrijskoj funkciji
// 1- zelis da ispadnes iz funkcije, stop se realizuje ili u callbacku ili posle ispada, vraca ODOMETRY_FAIL
// 2- zelis da ispadnes iz funkcije, stop se realizuje kako hoces, vraca ODOMETRY_CALLBACK_RETURN
char stop(char type);
char moveOnDirection(int distance, unsigned char speed, char (*callback)(unsigned long startTime));
char gotoXY(position coordinates, unsigned char speed, signed char direction, char (*callback)(unsigned long startTime));
char setPosition(position coordinates);
char rotateFor(int angle,unsigned char speed, char (*callback)(unsigned long startTime));
char setAngle(int angle, unsigned char speed, char (*callback)(unsigned long startTime));
char getState(void);
position getPosition(void);



#endif /* ODOMETRY_H_ */