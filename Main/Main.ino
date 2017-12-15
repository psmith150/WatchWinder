#include <SparkFun_TB6612.h>

enum rotation {CCW, CW, Both, Off}; //Enumeration for rotation selection
enum state {Init, Winding, Paused, QuickCharge, Disabled}; //Enumberation for state machine

// Pins for all inputs, keep in mind the PWM defines must be on PWM pins
#define AIN1 2
#define BIN1 7
#define AIN2 4
#define BIN2 8
#define PWMA 5
#define PWMB 6
#define STBY 9

// these constants are used to allow you to make your motor configuration 
// line up with function names like forward.  Value can be 1 or -1
const int offsetA = 1;
const int offsetB = 1;

// Initializing motors.  The library will allow you to initialize as many
// motors as you have memory for.  If you are using functions like forward
// that take 2 motors as arguements you can either write new functions or
// call the function more than once.
const int numMotors = 2;
Motor motor1 = Motor(AIN1, AIN2, PWMA, offsetA, STBY);
Motor motor2 = Motor(BIN1, BIN2, PWMB, offsetB, STBY);
Motor motors[] = {motor1, motor2};

// Constant winding parameters
const long quickChargeDuration = 30*60*1000L; //Time to run the quick charge sequence, in ms
const int maxSpeed = 10; //Motor max speed in RPM
const int cycleTime = 60; //Desired cycle time in minutes

// Variable declarations for controlling the rotation
int turnsPerDay[numMotors]; //Stores the desired number of rotations for each watch
rotation motorDirections[numMotors]; //Stores the allowable direction of rotation for each watch
int motorSpeed[numMotors]; //Motor speed for each watch, in RPM
unsigned long onTime[numMotors]; //Time to run (ms)
unsigned long offTime[numMotors]; //Time to pause (ms)
state winderState[numMotors]; //Tracks the state of each watch
bool transitioning[numMotors]; //One-shot used to track state transitions
unsigned long startMillis[numMotors]; //Used to keep track of the watch run/pause timers
int rotationDirection[numMotors]; //Used to switch between CW (-1) and CCW (+1) rotation

void setup() 
{
  for (int i=0; i<numMotors; i++)
  {
    //Initialize variables to default values
    turnsPerDay[i] = 750;
    motorDirections[i] = Both;
    motorSpeed[i] = maxSpeed;
    onTime[numMotors] = 600000L; //10 minutes on
    offTime[numMotors] = 5400000L; //90 minutes off
    startMillis[i] = 0L;
    rotationDirection[i] = 1;
    winderState[i] = Init; //Set each winder to the initialization state
    transitioning[i] = false;
  }
  Serial.begin(9600);
}

void loop() 
{
  int motorPWM = 0;
  float cyclesPerDay = 0.0;
  float calcTemp = 0.0; //Temporary float for intermediate calculations
  for (int i=0; i<numMotors;i++)
  {
    switch (winderState[i])
    {
      case Init: //Used to initialize on/off times
        //Initialize drive data here
        cyclesPerDay = 24.0 * 60.0 / cycleTime;
        calcTemp = turnsPerDay[i] / (cyclesPerDay * motorSpeed[i]); //Required on time in min
        onTime[i] = calcTemp * 60 * 1000; //Convert to ms and round
        offTime[i] = cycleTime * 60 * 1000 - onTime[i]; //Off time to fill out the cycle
        Serial.print("Motor ");
        Serial.print(i);
        Serial.print(" on/off time: ");
        Serial.print(onTime[i]);
        Serial.print(" / ");
        Serial.println(offTime[i]);
        delay(5000);
        winderState[i] = Winding;
        transitioning[i] = true;
        Serial.print("Motor ");
        Serial.print(i);
        Serial.println(" initialized.");
        break;
      case Winding: //Runs the motor in the correct direction to wind the watch
        if (transitioning[i])
        {
          switch (motorDirections[i])
          {
            case CCW:
              rotationDirection[i] = 1;
              break;
            case CW:
              rotationDirection[i] = -1;
              break;
            case Both:
              rotationDirection[i] = -rotationDirection[i];
              break;
            case Off:
              break;
            default:
              break;
          }
          startMillis[i] = millis();
          motorPWM = map(motorSpeed[i], 0, maxSpeed, 0, 255) * rotationDirection[i]; //Converts the RPM setpoint to a PWM value
          motors[i].drive(motorPWM);
          transitioning[i] = false;
          Serial.print("Motor ");
          Serial.print(i);
          Serial.println(" winding.");
        }
        if (timerExpired(startMillis[i], onTime[i]))
        {
          winderState[i] = Paused;
          transitioning[i] = true;
          Serial.print("Motor ");
          Serial.print(i);
          Serial.println(" finished winding.");
        }
        break;
      case Paused: //Idles for the remainder of the cycle
        if (transitioning[i])
        {
          startMillis[i] = millis();
          motors[i].brake();
          transitioning[i] = false;
          Serial.print("Motor ");
          Serial.print(i);
          Serial.println(" pausing.");
        }
        if (timerExpired(startMillis[i], offTime[i]))
        {
          winderState[i] = Winding;
          transitioning[i] = true;
          Serial.print("Motor ");
          Serial.print(i);
          Serial.println(" finished pausing.");
        }
        break;
      case QuickCharge: //Continuously rotates to charge up a watch from low power
        if (transitioning[i])
        {
          startMillis[i] = millis();
          motorPWM = map(maxSpeed, 0, maxSpeed, 0, 255); //Run at max speed
          motors[i].drive(motorPWM);
          transitioning[i] = false;
          Serial.print("Motor ");
          Serial.print(i);
          Serial.println(" charging quickly.");
        }
        if (timerExpired(startMillis[i], quickChargeDuration))
        {
          winderState[i] = Paused;
          transitioning[i] = true;
          Serial.print("Motor ");
          Serial.print(i);
          Serial.println(" finished charging quickly.");
        }
        break;
      case Disabled: //Watch is disabled; do nothing until re-enabled
        if (transitioning[i])
        {
          motors[i].brake();
          transitioning[i] = false;
          Serial.print("Motor ");
          Serial.print(i);
          Serial.println(" disabled.");
        }
        if (motorDirections[i] != Off)
        {
          winderState[i] = Init;
          transitioning[i] = true;
          Serial.print("Motor ");
          Serial.print(i);
          Serial.println(" re-enabled.");
        }
        break;
      default:
        break;
    }
    delay(100); //No need for fast cycle times
  }
}

//This function testes if the specified timer has expired
bool timerExpired(unsigned long startTime, int duration)
{
  return (millis() - startTime > duration);
}

