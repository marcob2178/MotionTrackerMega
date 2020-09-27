#include <Arduino.h>
#include <Settings.h>
#include <Joystick.h>
#include <accels/Accelerometer.h>
#include <accels/AccelBno055.h>
#include <accels/AccelBno080.h>
#include <ChestSensors.h>
#include <WeightSensor.h>
#include <Chest.h>
#include <Foot.h>

/*  
TODO: 

1) disconnecting sometimes accelerometer at the shoe
2) walking requires more sensible behavior, we have no code problem, more understanding about the what do we need to get.
3) left back weight sensor requires tuning, we'll tune that
4) crouch require assembling and implementing(guess we can try this flexible things, I have an idea how to reliable detect the crouch)
5) lateral pressure sensors work only if pressing the sensors whe shoe is flat. If i bend sideway to put pressure on the sensors, the lat moivements do not work (will check)
6) button cause joystick drift. (not sure, but we can try. Maybe that's not fixable by hardware)

STACK:
- usb injection (https://github.com/TrueOpenVR/TrueOpenVR-Drivers)  
- GUI for smoother configuration of the system

- the sensitivity of the chest when walking/running with feet, can be changed and made less sensitive compared to how the chest works in normal condition when using only chest for moving
*/

#define NO_OUTPUT 0
#define RIGHT_SHOE_OUTPUT 1
#define LEFT_SHOE_OUTPUT 2
#define CHEST_OUTPUT 3
#define MOVEMENT_CHEST_OUTPUT 4
#define MOVEMENT_RIGHT_OUTPUT 5
#define MOVEMENT_LEFT_OUTPUT 6
#define MOVEMENT_TRANSLATING_OUTPUT 7
#define TIMING_OUTPUT 8
int currentOutput = 0;

void printAcceleration();
void printRawValues();
void printAccelerationOffset();

void printRawChest();
void printRawRightShoe();
void printRawLeftShoe();
void updateRawData();
void translateTheMovement();
void parseSerial();
void printTheMovement();
void updateJoysticks();

Joystick leftJoystick;
Joystick rightJoystick; //jump and crouch
AccelBNO080 *chestAccel;
AccelBNO055 *rightShoeAccel;
AccelBNO055 *leftShoeAccel;
WeightSensor *rightSideFoot;
WeightSensor *rightBackFoot;
WeightSensor *leftSideFoot;
WeightSensor *leftBackFoot;

//logic
Chest *chest;
Foot *rightFoot;
Foot *leftFoot;

void setup()
{
  Serial.begin(2000000);
  while (!Serial)
    ;
  Serial.println("Started!");
  Wire.flush();
  Wire.begin();
  Wire.setClock(100000); //Increase I2C data rate to 400kHz

  delay(500);

  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);
  SPI.setClockDivider(SPI_CLOCK_DIV8);
  SPI.begin();

  delay(500);

  Serial.println("Create instances!");
  chestAccel = new AccelBNO080();
  rightShoeAccel = new AccelBNO055(1, LEFT_ACCEL_TWI_ADRESS);
  leftShoeAccel = new AccelBNO055(2, RIGHT_ACCEL_TWI_ADRESS);

  rightSideFoot = new WeightSensor(Sensor_SR);
  rightBackFoot = new WeightSensor(Sensor_BR);
  leftSideFoot = new WeightSensor(Sensor_SL);
  leftBackFoot = new WeightSensor(Sensor_BL);

  chest = new Chest(chestAccel);
  rightFoot = new Foot(rightShoeAccel, rightSideFoot, rightBackFoot);
  leftFoot = new Foot(leftShoeAccel, leftSideFoot, leftBackFoot);

  delay(200);

  Serial.println("Begin all");

  leftJoystick.begin(POT_0_CS, LEFT_BUTTON_PIN);
  rightJoystick.begin(POT_1_CS, RIGHT_BUTTON_PIN);
  leftJoystick.setCalibrationData(3, 252, 128,
                                  3, 252, 128);
  rightJoystick.setCalibrationData(HORIZONT_MIN, HORIZONT_MAX, HORIZONT_MIDDLE,
                                   VERTICAL_MIN, VERTICAL_MAX, VERTICAL_MIDDLE);

  rightShoeAccel->begin();
  delay(200);
  leftShoeAccel->begin();
  delay(200);
  chestAccel->setIntPin(CHEST_ACCEL_INT_PIN);
  chestAccel->begin();
  delay(200);

  Serial.println("Calibrating");
  unsigned long timer = millis();
  while (millis() - timer < 3000)
  {
    leftShoeAccel->calibrate();
    rightShoeAccel->calibrate();
    chestAccel->calibrate();
    Serial.print(".");
    delay(50);
  }
  Serial.println("\nDone!");
  Serial.println("Program started!");
}

long long timer = 0;
void loop()
{
  if (millis() > timer)
  {
    timer = millis() + CALCULATING_PERIOD;
    updateRawData();

    if (currentOutput == TIMING_OUTPUT)
    {
      long time = CALCULATING_PERIOD - (timer - millis());
      Serial.println(String(time));
    }

    parseSerial();
    printTheMovement();
    translateTheMovement();
  }
}

//   printAccelerationOffset();
//   calculate();

//   //Serial.println("\tLoop time = " + String(int(timer - millis())));
// }

//=====================================================================
//  Serial Controller
//=====================================================================

void parseSerial()
{
  if (Serial.available() > 0)
  {
    char mess = Serial.read();
    if (mess == 'r')
      currentOutput = RIGHT_SHOE_OUTPUT;
    if (mess == 'l')
      currentOutput = LEFT_SHOE_OUTPUT;
    if (mess == 'c')
      currentOutput = CHEST_OUTPUT;
    if (mess == 'm')
      currentOutput = MOVEMENT_CHEST_OUTPUT;
    if (mess == '.')
      currentOutput = MOVEMENT_RIGHT_OUTPUT;
    if (mess == ',')
      currentOutput = MOVEMENT_LEFT_OUTPUT;
    if (mess == 'n')
      currentOutput = NO_OUTPUT;
    if (mess == 't')
      currentOutput = MOVEMENT_TRANSLATING_OUTPUT;
    if (mess == 'u')
      currentOutput = TIMING_OUTPUT;
  }
}

//=================WALK DETECTION=======================

//=====================================================================
//  RAW data updating
//=====================================================================

void updateRawData()
{
  chestAccel->update();
  rightShoeAccel->update();
  leftShoeAccel->update();

  rightSideFoot->update();
  rightBackFoot->update();
  leftSideFoot->update();
  leftBackFoot->update();

  switch (currentOutput)
  {
  case RIGHT_SHOE_OUTPUT:
    printRawRightShoe();
    break;
  case LEFT_SHOE_OUTPUT:
    printRawLeftShoe();
    break;
  case CHEST_OUTPUT:
    printRawChest();
    break;
  }
}

//=====================================================================
//  Movement Translating
//=====================================================================

/*
the boneworks has:
1) Moving with left joystick/left controller in all direction to walk (no press of the joystick button)
2) Moving with left joystick/left controller in all direction while keeping pressed the left joystick button . This is to run
3) jump = is the press of the right joystick
4) crouch = is the moving of the right joystick down

*/

int left_x, left_y;
int left_button_state;
int right_x, right_y;
int right_button_state;
int delay_value = 0;

bool xchanged = false;
bool ychanged = false;

//=====================================================================
//  Walking

//Tunable parameters - all defined for range 0-100
float walk_sensitivity = 35;      //physical movement intensity to game walking speed
                                  //higher value - less motion required to move faster
float responsiveness_coeff = 100; //how fast changes in motion would affect game walking speed
float dead_zone = 30;             //minimal non-zero motion would result in this speed, if dead_zone = 30 then
                                  //walking speed of 5 would result in game action of 35
float hysteresis_coeff = 90;      //higher value results in more constant speed, but low sensitivity
                                  //for speed change. Lower value results in high sensitivity to speed change, but unstable speed

int use_digipot_remap = 0;          //1 for on, 0 for off
float digipot_resistance_kOhm = 50; //resistance of installed digipot

//calculated parameters - don't edit
float avg_a1 = 0;
float avg_a2 = 0;
float loc_max_a1 = 0;
float loc_max_a2 = 0;

float hyst_size = 20 * 0.01 * hysteresis_coeff;
float accum_err = 0;

float walk_speed_v = 0;
float walk_speed_max = 0;

float walk_speed = 0;

void translateWalking()
{
  float ax1 = rightShoeAccel->getLinX();
  float ay1 = rightShoeAccel->getLinY();
  float az1 = rightShoeAccel->getLinZ();
  float ax2 = leftShoeAccel->getLinX();
  float ay2 = leftShoeAccel->getLinY();
  float az2 = leftShoeAccel->getLinZ();
  float a1 = sqrt(ax1 * ax1 + ay1 * ay1 + az1 * az1);
  float a2 = sqrt(ax2 * ax2 + ay2 * ay2 + az2 * az2);

  float avg_param = 0.7;
  float loc_max_param = 0.99;
  avg_a1 = avg_param * avg_a1 + (1.0 - avg_param) * a1;
  avg_a2 = avg_param * avg_a2 + (1.0 - avg_param) * a2;
  loc_max_a1 *= loc_max_param;
  loc_max_a2 *= loc_max_param;
  if (avg_a1 > loc_max_a1)
    loc_max_a1 = avg_a1;
  if (avg_a2 > loc_max_a2)
    loc_max_a2 = avg_a2;

  if (loc_max_a1 < 2.5)
    loc_max_a1 = 2.5;
  if (loc_max_a2 < 2.5)
    loc_max_a2 = 2.5;

  if (avg_a1 < loc_max_a1 * 0.05 && avg_a2 < loc_max_a2 * 0.05)
    walk_speed_v *= 0.9;

  if (avg_a1 < loc_max_a1 * 0.3 && avg_a2 < loc_max_a2 * 0.3)
    walk_speed_v *= 0.99;

  float v_high = 0, v_low = 0;
  if (avg_a1 > loc_max_a1 * 0.3 && avg_a2 < loc_max_a2 * 0.3)
  {
    v_high = avg_a1;
    v_low = avg_a2;
  }
  if (avg_a1 < loc_max_a1 * 0.3 && avg_a2 > loc_max_a2 * 0.3)
  {
    v_high = avg_a2;
    v_low = avg_a1;
  }

  if (v_high < 0.5 && v_low < 0.5)
    walk_speed_v *= 0.99; //it's ok for a few frames, but not for too long
  else
  {
    walk_speed_v = walk_speed_v * 0.97 + 0.03 * (v_high * 10.0 * walk_sensitivity * 0.03);
  }

  if (walk_speed_v > walk_speed_max)
    walk_speed_max = walk_speed_v;

  walk_speed_max *= (0.995 - responsiveness_coeff * 0.0005);

  accum_err += 0.01 * (walk_speed_max - walk_speed);

  if (walk_speed_max > walk_speed + hyst_size || accum_err > 3 * hyst_size)
  {
    walk_speed = walk_speed_max;
    accum_err = 0;
  }

  if (walk_speed_max < walk_speed - hyst_size || accum_err < -3 * hyst_size || walk_speed_max < hyst_size / 2)
  {
    walk_speed = walk_speed_max;
    accum_err = 0;
  }

  if (walk_speed > 120)
    walk_speed = 120;
  if (walk_speed < 10)
    walk_speed = 0;
  if (walk_speed > 0 && walk_speed < 2 * dead_zone)
    walk_speed = dead_zone + walk_speed * 0.5;

  if (use_digipot_remap && walk_speed < 100)
  {
    float j_res = 4.3;
    float d_res = digipot_resistance_kOhm / 2;
    float tgt_res = 2.5 - walk_speed * 0.025;
    walk_speed = (d_res - tgt_res) / d_res * 100;
  }

  if (walk_speed != 0)
  {
    left_y = walk_speed;
    ychanged = true;
  }
}

void translateBending()
{
  if (chest->isBending())
  {
    //if(chestAccel->getRoll() <CHEST_BACKWARD_MIN)
    if (chestAccel->getRoll() < 0)
      left_y = map(-chestAccel->getRoll(), 0, CHEST_BACKWARD_MAX, 0, -100);
    else
      left_y = map(-chestAccel->getRoll(), 0, -CHEST_FORWARD_MAX, 0, 100);

    //calculating of right-left/horizontal movement
    if (chestAccel->getPitch() < 0)
      left_x = map(-chestAccel->getPitch(), 0, CHEST_RIGHT_MAX, 0, 100);
    else
      left_x = map(-chestAccel->getPitch(), 0, -CHEST_LEFT_MAX, 0, -100);
    xchanged = true;
    ychanged = true;
  }
}

void translateCruiseControl()
{
  if (rightFoot->isCruiseControl())
  {
    left_y = map(rightFoot->getCruiseControlPower(), 10, 25, 25, 100);
    ychanged = true;
  }
  else if (leftFoot->isCruiseControl())
  {
    left_y = map(leftFoot->getCruiseControlPower(), 10, 25, 25, 100);
    ychanged = true;
  }
}

void translateSideMoving()
{
  if (rightFoot->isSideStep())
  {
    left_x = map(rightFoot->getSidePower(), 200, 950, 0, 100);
    xchanged = true;
  }
  else if (leftFoot->isSideStep())
  {
    left_x = -map(leftFoot->getSidePower(), 200, 950, 0, 100);
    xchanged = true;
  }
}

void translateBackMoving()
{
  if (rightFoot->isStepBack())
  {
    left_y = -map(rightFoot->getStepBackPower(), 200, 850, 0, 100);
    ychanged = true;
  }

  else if (leftFoot->isStepBack())
  {
    left_y = -map(leftFoot->getStepBackPower(), 200, 850, 0, 100);
    ychanged = true;
  }
}

void translateTheMovement()
{
  xchanged = false;
  ychanged = false;
  //bending control
  translateBending();
  //walking
  translateWalking();

  //cruise control
  translateCruiseControl();
  //side moving
  translateSideMoving();
  //back moving
  translateBackMoving();

  //default for moving left joystick

  if (!xchanged)
    left_x = 0;
  if (!ychanged)
    left_y = 0;

  right_x = 0;
  right_y = 0;

  //if running very fast - press the button
  if ((abs(left_x) >= 110) || (abs(left_y) >= 110))
    left_button_state = 1;
  else
    left_button_state = 0;

  //jump
  if (chest->isJumping())
    right_button_state = 1;
  else
    right_button_state = 0;

  updateJoysticks();

  if (currentOutput == MOVEMENT_TRANSLATING_OUTPUT)
  {
    Serial.print("Left joystick:\t" + String(left_x) + "\t" + String(left_y) + "\t" + "button\t" + String(left_button_state) + "\t");
    Serial.println("Right joystick:\t" + String(right_x) + "\t" + String(right_y) + "\t" + "button\t" + String(right_button_state) + "\t");
  }
}

void updateJoysticks()
{
  if (left_x > 100)
    left_x = 100;
  if (left_x < -100)
    left_x = -100;

  if (left_y > 100)
    left_y = 100;
  if (left_y < -100)
    left_y = -100;

  if (right_x > 100)
    right_x = 100;
  if (right_x < -100)
    right_x = -100;

  if (right_y > 100)
    right_y = 100;
  if (right_y < -100)
    right_y = -100;

  leftJoystick.setHor(-left_x);
  leftJoystick.setVer(-left_y);

  if (left_button_state == 1)
    leftJoystick.pressButton();
  else
    leftJoystick.releaseButton();

  rightJoystick.setHor(right_x);
  rightJoystick.setVer(right_y);

  if (right_button_state == 1)
    rightJoystick.pressButton();
  else
    rightJoystick.releaseButton();
}

//=====================================================================
//  Prints
//=====================================================================

void printTheMovement()
{
  if (currentOutput == MOVEMENT_CHEST_OUTPUT)
  {
    //jump output
    Serial.print("\tJump:" + String(chest->isJumping() ? "\tDetected" : "\tNothing") + String("\t") + String(chest->isJumping() ? String(chest->getJumpingPower()) : "\t") + String("\t"));

    //bend output
    Serial.print("Chest\tBend:");

    if (!chest->isBending())
      Serial.print("\tStraight\t");
    else
    {
      if ((chest->getBendingDirection() > 315) || (chest->getBendingDirection() <= 45))
        Serial.print("\tbackward");

      else if ((chest->getBendingDirection() > 225) && (chest->getBendingDirection() <= 315))
        Serial.print("\tleft");

      else if ((chest->getBendingDirection() > 135) && (chest->getBendingDirection() <= 225))
        Serial.print("\tforward");

      else if ((chest->getBendingDirection() > 45) && (chest->getBendingDirection() <= 135))
        Serial.print("\tright");

      Serial.print(String("\t") + chest->getBendingDirection());
      Serial.print(String("\t") + chest->getBendingPower());
    }

    //crouch output F

    // Serial.print("\tCrouch:");
    // if (isCrouch())
    // {
    //   Serial.print("\tDetected" + String("\t") + String(getCrouchPower()));
    // }
    // else
    //   Serial.print("\tNothing");

    Serial.println();
  }

  // right

  if (currentOutput == MOVEMENT_RIGHT_OUTPUT)
  {
    //running output right

    Serial.print("Right foot\tWalk:");
    if (rightFoot->isWalking())
      Serial.print(String("\t") + String(rightFoot->getWalkingPower()));
    else
      Serial.print("\tNothing\t");

    //special things

    Serial.print("\tCruise");
    if (rightFoot->isCruiseControl())
    {
      Serial.print(String("\t") + String(rightFoot->getCruiseControlPower()));
    }
    else
      Serial.print("\tNothing\t");

    //step back

    Serial.print("\tStepback");
    if (rightFoot->isStepBack())
    {
      Serial.print(String("\t") + String(rightFoot->getStepBackPower()));
    }
    else
      Serial.print("\tNothing\t");

    //step side

    Serial.print("\tStepside");
    if (rightFoot->isSideStep())
    {
      Serial.print(String("\t") + String(rightFoot->getSidePower()));
    }
    else
      Serial.print("\tNothing\t");
    Serial.println();
  }

  // left

  if (currentOutput == MOVEMENT_LEFT_OUTPUT)
  {
    //running output right

    Serial.print("Left foot\tWalk:");
    if (leftFoot->isWalking())
      Serial.print(String("\t") + String(leftFoot->getWalkingPower()));

    else
      Serial.print("\tNothing\t");

    //special things

    Serial.print("\tCruise");
    if (leftFoot->isCruiseControl())
      Serial.print(String("\t") + String(leftFoot->getCruiseControlPower()));
    else
      Serial.print("\tNothing\t");

    //step back

    Serial.print("\tStepback");
    if (leftFoot->isStepBack())
    {
      Serial.print(String("\t") + String(leftFoot->getStepBackPower()));
    }
    else
      Serial.print("\tNothing\t");

    //step side

    Serial.print("\tStepside");
    if (leftFoot->isSideStep())
    {
      Serial.print(String("\t") + String(leftFoot->getSidePower()));
    }
    else
      Serial.print("\tNothing\t");
    Serial.println();
  }
}

void printRawChest()
{

  Serial.print("chest:");
  Serial.print("\tp\t" + String(chestAccel->getPitch()));
  Serial.print("\tr\t" + String(chestAccel->getRoll()));
  Serial.println("\taZ\t" + String(chestAccel->getLinZ()));
  //Serial.println("\talt\t" + String(chestAccel->getAltitude()));
}

void printRawRightShoe()
{
  Serial.print("rshoe:");
  Serial.print("\tax\t" + String(rightShoeAccel->getLinX()));
  Serial.print("\tay\t" + String(rightShoeAccel->getLinY()));
  Serial.print("\taz\t" + String(rightShoeAccel->getLinZ()));

  Serial.print("\ty\t" + String(rightShoeAccel->getYaw()));
  Serial.print("\tr\t" + String(rightShoeAccel->getRoll()));
  Serial.print("\tp\t" + String(rightShoeAccel->getPitch()));

  Serial.print("\tweight:");
  Serial.print("\ts\t" + String(rightSideFoot->readRaw()));
  Serial.println("\tb\t" + String(rightBackFoot->readRaw()));
}

void printRawLeftShoe()
{
  Serial.print("lshoe:");
  Serial.print("\tax\t" + String(leftShoeAccel->getLinX()));
  Serial.print("\tay\t" + String(leftShoeAccel->getLinY()));
  Serial.print("\taz\t" + String(leftShoeAccel->getLinZ()));

  Serial.print("\ty\t" + String(leftShoeAccel->getYaw()));
  Serial.print("\tr\t" + String(leftShoeAccel->getRoll()));
  Serial.print("\tp\t" + String(leftShoeAccel->getPitch()));

  Serial.print("\tweight:");
  Serial.print("\ts\t" + String(leftSideFoot->readRaw()));
  Serial.println("\tb\t" + String(leftBackFoot->readRaw()));
}

void printRawValues()
{
  Serial.println("(\\/)*===*(\\/)");
  printRawChest();
  printRawRightShoe();
  printRawLeftShoe();
}
