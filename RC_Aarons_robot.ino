/*
 Aaron's Robot V2.1
 designed with love by Papa:  Jim Thomas Jan 2014
 select: Arduino Mega 2560 controller
 ---- NRF 24L01+ 2.4Ghz Radio Receiver-----
 works with RC transmitterr (RC_controller_NRF2401)
 This sketch receives joystick values and button codes
 ----Differential Drive Robot Chassis----
 L298N Dual H Bridge
 Right DC motor, Left DC  motor as viewed from the castor in rear
 received analog values drive two motors L and R on the robot
 The analog values received are already scaled to -255 to +255
  postive value drive the motor forward  power 0 to 255 max
 negative value drive the motor reverse power absvalue 0-255 max
 v2.0 upgrades
 single joystick drive for motors using 45 degree coordinate rotation algorithm
 v2.1 upgrades
 adding WTV020-SD-16P module to produce variety of sounds, R2D2 beeps, robot voices
 parent hack thru TV remote control etc.
 remove Alert Pin beeper
 Thanks and (C) to Manicbug for the NRF24L01 libraries
 */
/* WTV020-SD-16P audio module library
 Created by Diego J. Arevalo, August 6th, 2012.
 slight modifications to timing inside by Jim Thomas
 Released into the public domain.
 */

#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
#include <Servo.h>

#include <Wtv020sd16p.h>

int joystick[3];

// values to be recieved from the RC controller
unsigned int s_register; // button states
int joy_RUD = 0;
int joy_RLR = 0;
int joy_LUD = 0;
int joy_LLR = 0;


// masks for each buttons in s_register
const int R1Mask = 0x10;
const int R2Mask = 0x20;
const int L1Mask = 0x1000;
const int L2Mask = 0x2000;
const int RPushMask = 0x40;
const int LPushMask = 0x4000;
const int B1Mask = 0x100;
const int B2Mask = 0x200;
const int B3Mask = 0x400;
const int B4Mask = 0x800;
const int LLMask = 0x1;
const int LRMask = 0x2;
const int LDMask = 0x4;
const int LUMask = 0x8;
const int PushMask = 0x4040; // push either joystick

// used for buttons that make a sound to indicate the button was already detected as pressed
// B1,B2,B3,B4,R1,R2
boolean B1Flag = false;
boolean B2Flag = false;
boolean B3Flag = false;
boolean B4Flag = false;
boolean R1Flag = false;
boolean R2Flag = false;

int soundPlay = 0;  // holds track number of last sound file played


// structure to hold the payload 10bytes
struct payload_r {
  unsigned int sreg;
  int j_RUD;
  int j_RLR;
  int j_LUD;
  int j_LLR;
};

/* NRF 24L01+ pin connections for Mega
 pin 53 CE; pin 48 CSN; pin 51 MOSI; pin 52 SCK; pin 50 MISO
 */
RF24 radio(53, 48);
const uint64_t pipe = 0xE8E8F0F0E1LL;

/*  Motor Drive Controller Board Module L298N Dual H Bridge
 connections to Ardunio pwm capable Outputs...
 */
const int RmotorPinF = 2;  // Right Forward connect to MC board in
const int RmotorPinR = 3;  // Right Reverse connect to MC board in
const int LmotorPinF = 4;  // Left Forward connect to MC board in
const int LmotorPinR = 5;  // Left Reverse connect to MC board in

// arrays of constants to convert tractor drive to joystick drive
// lookup motorPower = [j_RUD][j_RLR]
// 6x6 version
const int LmotorPower [7][7] = {
  { -255, -255, -255, -255, -200, -150, 0},
  { -255, -200, -200, -200, -150, 0, 150},
  { -255, -200, -150, -150, 0, 150, 200},
  { -255, -200, -150, 0, 150, 200, 255},
  { -200, -150, 0, 150, 150, 200, 255},
  { -100, 0, 100, 200, 200, 200, 255},
  {0, 150, 200, 255, 255, 255, 255}
};
const int RmotorPower [7][7] = {
  {0, -150, -200 - 255, -255, -255, -255},
  {150, 0, -150, -200, -200, -200, -255},
  {200, 150, 0, -150, -150, -200, -255},
  {255, 200, 150, 0, -150, -200, -255},
  {255, 200, 150, 150, 0, -150, -200},
  {255, 200, 200, 200, 150, 0, -150},
  {255, 255, 255, 255, 200, 150, 0}

};

// output pin definitions - to robot head
const int SpinMotor  = 28;  // blk - motor that puts spin on disc / red Vbatt
const int ShootMotor = 32;  // blue - motor that ejects disc / orange Vbatt
const int LeftEyeLed = 29;  // yellow - left eye led / gnd = black
const int RightEyeLed = 33; // red - right eye led / gnd = black

const int SpeakerPin = 6;   // speaker output pin

const int SpotPin = 7;  // spotlight output
const int SpotServo  = 12;  // small servo motor for LED spotlight
int SpotAngle = 0;  // spotlight servo angle in degrees
boolean SpotButton = false; // for toggle of spotlight
boolean SpotOn = false; // spotlight on state
Servo servo1;  // Spotlight servo

const int ArmServo = 13;  // large servo motor for Arm up/down
Servo servo2;  // Arm servo
int ArmAngle = 90;  // current arm angle.. constrain to 0-180
int ArmIncrement = 1;  // # degrees to increment servo angle

const int RotPin = 36; // blue rotating light pin (transistor drive)
boolean RotButton = false;
boolean RotOn = false;

boolean EyesButton = false; // robot eyes
boolean EyesOn  = false;

// LED's in robot's hand
const int HandLED1 = 35;
const int HandLED2 = 37;
const int HandLED3 = 39;
int LaserTone = 60; // variable tone for hand laser LEDs
int count = 0;
int lcount = 0;

// pins to control WTV020-SD-16P uSD Audio module in Wtv020sd16p
const int resetPin = 26;
const int clockPin = 27;
const int dataPin = 30;
const int busyPin = 31;


/*
Create an instance of the Wtv020sd16p class.
 1st parameter: Reset pin number.
 2nd parameter: Clock pin number.
 3rd parameter: Data pin number.
 4th parameter: Busy pin number.
 */
Wtv020sd16p wtv020sd16p(resetPin, clockPin, dataPin, busyPin);

void setup(void)
{
  Serial.begin(9600);
  printf_begin();
  // initialize sound module
  wtv020sd16p.reset();
  // motor pins as outputs, and motors off: set to 0V = LOW
  pinMode (RmotorPinF, OUTPUT);
  pinMode (RmotorPinR, OUTPUT);
  pinMode (LmotorPinF, OUTPUT);
  pinMode (LmotorPinR, OUTPUT);
  digitalWrite(RmotorPinF, LOW);
  digitalWrite(RmotorPinR, LOW);
  digitalWrite(LmotorPinF, LOW);
  digitalWrite(LmotorPinF, LOW);

  // Robot head
  pinMode (SpinMotor, OUTPUT);
  pinMode (ShootMotor, OUTPUT);
  pinMode (LeftEyeLed, OUTPUT);
  pinMode (RightEyeLed, OUTPUT);
  digitalWrite(SpinMotor, LOW);
  digitalWrite(ShootMotor, LOW);
  digitalWrite(LeftEyeLed, LOW);
  digitalWrite(RightEyeLed, LOW);

  pinMode (SpeakerPin, OUTPUT);
  pinMode (SpotPin, OUTPUT);
  pinMode (RotPin, OUTPUT);
  pinMode (HandLED1, OUTPUT);
  pinMode (HandLED2, OUTPUT);
  pinMode (HandLED3, OUTPUT);


  servo1.attach(SpotServo);
  servo2.attach(ArmServo);
  servo2.write(ArmAngle);   // initialize to 90

  // start the radio
  radio.begin();
  radio.setChannel(0x42); // change from default setting (0x4C) to avoid interference
  radio.setDataRate(RF24_250KBPS);  // slow data rate for better reliability
  radio.setPayloadSize(10);  // 10 byte payload
  radio.openReadingPipe(1, pipe);
  radio.startListening();
  radio.printDetails();

  // say Robie is awake
  wtv020sd16p.playVoice(0000);
}

void loop(void)
{
  int x, y;
  payload_r payload = {
    0, 0, 0, 0, 0
  };  // 10 byte payload
  if ( radio.available() )
  {
    // Dump the payloads until we've gotten everything
    bool done = false;
    while (!done)
    {

      done = radio.read(&payload, sizeof(payload));

      //Serial.println(payload.sreg,BIN);
      //printf(" %4i %4i %4i %4i ",payload.j_RUD,payload.j_RLR,payload.j_LUD,payload.j_LLR);
      //printf("\n");

      // right joystick controls motors,  -255 to 255
      // note: j_RLR value comes in inversed

      // diagonal drive
      //joystick[0] = -payload.j_RLR;
      //joystick[1] = payload.j_RUD;

      // quantize joystick values 0-6

      if (-payload.j_RLR < -200)     x = 0;
      else if (-payload.j_RLR < -150)x = 1;
      else if (-payload.j_RLR < -10) x = 2;
      else if (-payload.j_RLR < 10)  x = 3;
      else if (-payload.j_RLR < 150) x = 4;
      else if (-payload.j_RLR < 200) x = 5;
      else x = 6;

      if (payload.j_RUD < -200)     y = 0;
      else if (payload.j_RUD < -150)y = 1;
      else if (payload.j_RUD < -10) y = 2;
      else if (payload.j_RUD < 10)  y = 3;
      else if (payload.j_RUD < 150) y = 4;
      else if (payload.j_RUD < 200) y = 5;
      else y = 6;

      // lookup motor power values from the motor power table
      joystick[0] = LmotorPower[y][x];
      joystick[1] = RmotorPower[y][x];

     // Serial.print(joystick[0]);
     // Serial.print(' ');
     // Serial.println(joystick[1]);
     
     // play sounds from the sound card based on button pressed
     // Machinery in B1 button
     if (B1Mask & payload.sreg) {  // button pressed
       if (B1Flag == false) { // just got pressed, play the song
              // tracks of random machine sounds 
              soundPlay = random(49, 59);
              wtv020sd16p.stopVoice();
              wtv020sd16p.asyncPlayVoice(soundPlay);
              B1Flag = true; // set the flag showing button was pressed
       }
     } else {  // button is released reset the flag
     B1Flag = false;
     }
     // R2D2 on B2 button
     if (B2Mask & payload.sreg) {  // button pressed
       if (B2Flag == false) { // just got pressed, play the song
              // random R2D2 sounds 
              int soundPlay = random(59, 75);
              wtv020sd16p.stopVoice();
              wtv020sd16p.asyncPlayVoice(soundPlay);
              B2Flag = true; // set the flag showing button was pressed
       }
     } else {  // button is released reset the flag
     B2Flag = false;
     }
     // drum solos on B3 button
     if (B3Mask & payload.sreg) {  // button pressed
       if (B3Flag == false) { // just got pressed, play the song
              // random drum solos
              int soundPlay = random(75, 80);
              wtv020sd16p.stopVoice();
              wtv020sd16p.asyncPlayVoice(soundPlay);
              B3Flag = true; // set the flag showing button was pressed
       }
     } else {  // button is released reset the flag
     B3Flag = false;
     }
     // random voices on B4 button
     if (B4Mask & payload.sreg) {  // button pressed
       if (B4Flag == false) { // just got pressed, play the song
              // random voice recordings 
              int soundPlay = random(1, 26);
              wtv020sd16p.stopVoice();
              wtv020sd16p.asyncPlayVoice(soundPlay);
              B4Flag = true; // set the flag showing button was pressed
       }
     } else {  // button is released reset the flag
     B4Flag = false;
     }
     // space guns and animal sounds on R2 button
     if (R2Mask & payload.sreg) {  // button pressed
       if (R2Flag == false) { // just got pressed, play the song
              // space guns and animals
              int soundPlay = random(33, 49);
              wtv020sd16p.stopVoice();
              wtv020sd16p.asyncPlayVoice(soundPlay);
              R2Flag = true; // set the flag showing button was pressed
       }
     } else {  // button is released reset the flag
     R2Flag = false;
     }

      // LED Spot Light On / Off
      if (LUMask & payload.sreg) {
        if (SpotButton == false) { //  button low to transition so toggle the spot
          SpotButton = true;
          if (SpotOn == true) {
            SpotOn = false;
            digitalWrite (SpotPin, LOW);
          }
          else {
            SpotOn = true;
            digitalWrite (SpotPin, HIGH);
          }
        }
      }
      else {
        SpotButton = false;
      }

      // Blue Rotation Light On / Off
      if (LRMask & payload.sreg) {
        if (RotButton == false) { //  button low to transition so toggle the spot
          RotButton = true;
          if (RotOn == true) {
            RotOn = false;
            digitalWrite (RotPin, LOW);
          }
          else {
            RotOn = true;
            digitalWrite (RotPin, HIGH);
          }
        }
      }
      else {
        RotButton = false;
      }

      // Robot Eyes On / Off
      if (LLMask & payload.sreg) {
        if (EyesButton == false) { //  button low to transition so toggle the spot
          EyesButton = true;
          if (EyesOn == true) {
            EyesOn = false;
            digitalWrite(RightEyeLed, LOW);
            digitalWrite(LeftEyeLed, LOW);
          }
          else {
            EyesOn = true;
            digitalWrite(RightEyeLed, HIGH);
            digitalWrite(LeftEyeLed, HIGH);
          }
        }
      }
      else {
        EyesButton = false;
      }

      // LED Spot Light servo
      SpotAngle = (payload.j_LLR + 255) / 3; // 0 to 180ish
      servo1.write(SpotAngle);


      // Arm Servo L1 button decreases angle, L2 button increases angle
      // 0 all up, 180 all down
      if (L2Mask & payload.sreg) {
        if (ArmAngle + ArmIncrement < 180) {
          ArmAngle = ArmAngle + ArmIncrement;
          servo2.write(ArmAngle);
        }
      }
      if (L1Mask & payload.sreg) {
        if (ArmAngle - ArmIncrement > 0) {
          ArmAngle = ArmAngle - ArmIncrement;
          servo2.write(ArmAngle);
        }
      }

     // Serial.print(joystick[0]);
     // Serial.print(' ');
     // Serial.println(joystick[1]);
     
     
      // left (port) motor
      if (joystick[0] > 0) {      // forward
        analogWrite(LmotorPinF, joystick[0]);
        digitalWrite(LmotorPinR, LOW);
      }
      else if (joystick[0] < 0) { //reverse
        digitalWrite(LmotorPinF, LOW);
        analogWrite(LmotorPinR, -joystick[0]);
      }
      else {
        digitalWrite(LmotorPinF, LOW); // stopped
        digitalWrite(LmotorPinR, LOW);
      }
      // right (starbord) motor
      if (joystick[1] > 0) {      // forward
        analogWrite(RmotorPinF, joystick[1]);
        digitalWrite(RmotorPinR, LOW);
      }
      else if (joystick[1] < 0) { //reverse
        digitalWrite(RmotorPinF, LOW);
        analogWrite(RmotorPinR, -joystick[1]);
      }
      else {
        digitalWrite(RmotorPinF, LOW); // stopped
        digitalWrite(RmotorPinR, LOW);
      }

      // shoot disk motors
      if (R1Mask & payload.sreg) {
        digitalWrite(SpinMotor, HIGH);
        digitalWrite(ShootMotor, HIGH);
      }
      else {
        digitalWrite(SpinMotor, LOW);
        digitalWrite(ShootMotor, LOW);
      }

      // Hand LEDs
      if (R2Mask & payload.sreg) {
        if (lcount == 16) {
          count += 1;
          lcount = 0;
        } else {
          lcount += 1;
        }
        if (count > 3) {
          count = 1;
        }
        //  tone (SpeakerPin, LaserTone);
        //  LaserTone += 150;
        //  if (LaserTone > 2000) {
        //   LaserTone = 60;
        //}
        if (count == 1) {
          digitalWrite(HandLED1, LOW);
        }
        else {
          digitalWrite(HandLED1, HIGH);
        }
        if (count == 2) {
          digitalWrite(HandLED2, LOW);
        }
        else {
          digitalWrite(HandLED2, HIGH);
        }
        if (count == 3) {
          digitalWrite(HandLED3, LOW);
        }
        else {
          digitalWrite(HandLED3, HIGH);
        }
        delay (2);
        //  noTone (SpeakerPin);
      }
      else {
        digitalWrite(HandLED1, LOW);
        digitalWrite(HandLED2, LOW);
        digitalWrite(HandLED3, LOW);
        // LaserTone = 60;
      }
      
      // Play sound tracks from the sound card
      
      
      


      /*
      soundfile = random(79);
      if (LDMask & payload.sreg) {
        wtv020sd16p.stopVoice();
        wtv020sd16p.asyncPlayVoice(soundfile);
        Serial.print("Now playing sound file #: ");
        Serial.println(soundfile);
      }
      else {
        if (B1Mask & payload.sreg) {
          tone (SpeakerPin, 523);
          delay (10);
          noTone (SpeakerPin);
        }
        else {
          noTone (SpeakerPin) ;
        }
        if (B2Mask & payload.sreg) {
          tone (SpeakerPin, 587);
          delay (10);
          noTone (SpeakerPin);
        }
        else {
          noTone (SpeakerPin) ;
        }
        if (B3Mask & payload.sreg) {
          tone (SpeakerPin, 659);
          delay (10);
          noTone (SpeakerPin);
        }
        else {
          noTone (SpeakerPin) ;
        }
        if (B4Mask & payload.sreg) {
          tone (SpeakerPin, 698);
          delay (10);
          noTone (SpeakerPin);
        }
        else {
          noTone (SpeakerPin) ;
        }
        noTone (SpeakerPin);
      }
      */
    }



  }
  else
  {
    AllStop();
    // Serial.println("No radio available");
    //delay (1000);
  }
  //delay (100);
}

// All Stop - loss of signal -- turn off everything that moves
void AllStop () {
  digitalWrite(RmotorPinF, LOW);
  digitalWrite(RmotorPinR, LOW);
  digitalWrite(LmotorPinF, LOW);
  digitalWrite(LmotorPinF, LOW);

  //digitalWrite(SpinMotor,LOW);
  //digitalWrite(ShootMotor,LOW);
  //digitalWrite(LeftEyeLed,LOW);
  //digitalWrite(RightEyeLed,LOW);
  //digitalWrite(SpotPin,LOW);
  //servo1.write(0);
  //servo2.write(90);
  //ArmAngle = 90;
  //SpotButton = false;
  //SpotOn = false;
  //digitalWrite(RotPin,LOW);
  //RotButton = false;
  //RotOn = false;
  //EyesOn  = false;
  digitalWrite(HandLED1, LOW);
  digitalWrite(HandLED2, LOW);
  digitalWrite(HandLED3, LOW);
  // make a beep for loss of signal
  //delay (100);

}

