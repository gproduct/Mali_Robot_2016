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

struct odometry_position
{
	int16_t x;
	int16_t y;
	int16_t angle;
	int8_t  state;
};

enum odometry_states
{
	IDLE = 'I',
	MOVING = 'M',
	ROTATING = 'R',
	STUCK = 'S',
	ERROR = 'E'
};


// callback za odometriju vraca sledece vrednosti:
// 0- nista se ne desava, ostani u odometrijskoj funkciji
// 1- zelis da ispadnes iz funkcije, stop se realizuje ili u callbacku ili posle ispada, vraca ODOMETRY_FAIL
// 2- zelis da ispadnes iz funkcije, stop se realizuje kako hoces, vraca ODOMETRY_CALLBACK_RETURN
void	odometry_stop(uint8_t type);
uint8_t odometry_move_straight(int16_t distance, uint8_t speed, char (*callback)(uint32_t startTime));
char	gotoXY(struct odometry_position* position, uint8_t speed, int8_t direction, char (*callback)(uint32_t startTime));
void	setPosition(struct odometry_position* position);
char	rotateFor(int16_t angle,uint8_t speed, char (*callback)(uint32_t startTime));
char	setAngle(int16_t angle, uint8_t char speed, char (*callback)(uint32_t startTime));
char	getState(void);




#endif /* ODOMETRY_H_ */