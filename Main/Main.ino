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
#define Motor1TPD 0
#define Motor2TPD 1
#define Motor1Direction 2
#define Motor2Direction 3

// Put pins in arrays to work within loop structure
int tpdPins[] = {Motor1TPD, Motor2TPD};
int directionPins[] = {Motor1Direction, Motor2Direction};

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
  Serial.begin(9600);
  //delay(2000);
  //Serial.print("Setting up ");
  //Serial.print(numMotors);
  //Serial.println(" motors");
  for (int i=0; i<numMotors; i++)
  {
    //Initialize variables to default values
    turnsPerDay[i] = 750;
    motorDirections[i] = Both;
    motorSpeed[i] = maxSpeed;
    onTime[i] = 600000L; //10 minutes on
    offTime[i] = 5400000L; //90 minutes off
    startMillis[i] = 0L;
    rotationDirection[i] = 1;
    winderState[i] = Init; //Set each winder to the initialization state
    transitioning[i] = false;
    Serial.print("Setup complete for motor ");
    Serial.println(i);
  }
}

void loop() 
{
  //unsigned long cycleStart = millis();
  int motorPWM = 0;
  float cyclesPerDay = 0.0;
  float calcTemp = 0.0; //Temporary float for intermediate calculations
  for(int i=0; i<numMotors; i++)
  {
    motorDirections[i] = readDirection(directionPins[i]);
    if (motorDirections[i] == Off)
    {
      winderState[i] = Disabled;
    }
    switch (winderState[i])
    {
      case Init: //Used to initialize on/off times
        //Initialize drive data here
        cyclesPerDay = 24.0 * 60.0 / cycleTime;
        turnsPerDay[i] = readTPD(tpdPins[i]);
        calcTemp = turnsPerDay[i] / (cyclesPerDay * motorSpeed[i]); //Required on time in min
        onTime[i] = calcTemp * 60 * 1000; //Convert to ms and round
        offTime[i] = cycleTime * 60 * 1000 - onTime[i]; //Off time to fill out the cycle
//        delay(5000);
//        Serial.print("Motor ");
//        Serial.print(i);
//        Serial.print(" on/off time: ");
//        Serial.print(onTime[i]);
//        Serial.print(" / ");
//        Serial.println(offTime[i]);
//        delay(5000);
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
//        Serial.print("Motor ");
//        Serial.print(i);
//        Serial.print(" time passed: ");
//        Serial.print((millis()-startMillis[i])/1000);
//        Serial.print(" / ");
//        Serial.println(onTime[i]/1000);
//        delay(1000);
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
        //winderState[i] = Init;
        break;
    }
  }
  delay(100);
  //Serial.print("Cycle time: ");
  //Serial.println(millis() - cycleStart);
}

//This function testes if the specified timer has expired
bool timerExpired(unsigned long startTime, unsigned long duration)
{
  return (millis() - startTime > duration);
}

rotation readDirection(int pin)
{
  int val = analogRead(pin);
  if (val >=0 && val < 256)
  {
    return CCW;
  }
  else if (val >= 256 && val < 512)
  {
    return CW;
  }
  else if (val >= 512 && val < 768)
  {
    return Both;
  }
  else if (val >= 768 && val < 1024)
  {
    return Off;
  }
  else
  {
    Serial.print("Invalid reading on analog pin ");
    Serial.println(pin);
    return CCW;
  }
}

int readTPD(int pin)
{
  int val = analogRead(pin);
  return map(val, 0, 1023, 200, 2000);
}

