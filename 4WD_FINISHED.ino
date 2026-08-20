// ============================================================
//  ROBOT — Bluetooth Control + Line Following + Obstacle Avoidance
//  Komandat Bluetooth:
//    F/U = Para      B/D = Mbrapa
//    L = Majtas      R = Djathtas
//    S = Stop
//    T = Line Following mode  (del me 'S')
//    O = Obstacle Avoidance mode (del me 'S')
//
//  Gjatë kontrollit manual:
//    - F/B: CMD_TIMEOUT_MOVE (600ms)
//    - L/R: CMD_TIMEOUT_TURN (200ms)
//    - Nëse ka objekt përpara ose IR anësor → avoid automatik
// ============================================================

#include <Servo.h>

#define Lpwm_pin  5
#define Rpwm_pin  6
int pinLB = 2;
int pinLF = 4;
int pinRB = 7;
int pinRF = 8;

#define SensorLeft    9
#define SensorMiddle  10
#define SensorRight   11
unsigned char SL, SM, SR;

#define ECHO_PIN  A0
#define TRIG_PIN  A1

Servo myservo;
#define SERVO_PIN A2

#define IR_LEFT_PIN   A3
#define IR_RIGHT_PIN  A4
#define IR_MIN        350
#define IR_MAX        900
#define IR_DEBOUNCE   100
#define IR_COOLDOWN  1500

volatile int DL, DM, DR;

bool blockedLeft  = false;
bool blockedRight = false;
unsigned long lastObstacleTime = 0;
#define BLOCK_FORGET_MS  4000
unsigned long irCooldownUntil = 0;

char bluetooth_data;
unsigned long lastCmdTime = 0;
#define CMD_TIMEOUT_MOVE  600
#define CMD_TIMEOUT_TURN  200

char currentMove = 'S';
unsigned int currentTimeout = CMD_TIMEOUT_MOVE;

void Set_Speed(unsigned char pwm) {
  analogWrite(Lpwm_pin, pwm);
  analogWrite(Rpwm_pin, pwm);
}

void advance() {
  digitalWrite(pinRB, LOW);  digitalWrite(pinRF, HIGH);
  digitalWrite(pinLB, LOW);  digitalWrite(pinLF, HIGH);
}

void turnR() {
  digitalWrite(pinRB, LOW);  digitalWrite(pinRF, HIGH);
  digitalWrite(pinLB, HIGH); digitalWrite(pinLF, LOW);
}

void turnL() {
  digitalWrite(pinRB, HIGH); digitalWrite(pinRF, LOW);
  digitalWrite(pinLB, LOW);  digitalWrite(pinLF, HIGH);
}

void stopp() {
  digitalWrite(pinRB, LOW); digitalWrite(pinRF, LOW);
  digitalWrite(pinLB, LOW); digitalWrite(pinLF, LOW);
  analogWrite(Lpwm_pin, 0);
  analogWrite(Rpwm_pin, 0);
}

void back() {
  digitalWrite(pinRB, HIGH); digitalWrite(pinRF, LOW);
  digitalWrite(pinLB, HIGH); digitalWrite(pinLF, LOW);
}

void Sensor_Scan() {
  SL = digitalRead(SensorLeft);
  SM = digitalRead(SensorMiddle);
  SR = digitalRead(SensorRight);
}

float checkdistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  float distance = pulseIn(ECHO_PIN, HIGH) / 58.00;
  delay(10);
  return distance;
}

void Detect_obstacle_distance() {
  myservo.write(160);
  for (int i = 0; i < 3; i++) { DL = checkdistance(); delay(100); }
  myservo.write(20);
  for (int i = 0; i < 3; i++) { DR = checkdistance(); delay(100); }
  myservo.write(90);
  delay(200);
}

bool readIR(int pin) {
  int val = analogRead(pin);
  if (val < IR_MIN || val > IR_MAX) return false;
  unsigned long t = millis();
  while (millis() - t < IR_DEBOUNCE) {
    val = analogRead(pin);
    if (val < IR_MIN || val > IR_MAX) return false;
    delay(5);
  }
  return true;
}

void resetMemory() {
  blockedLeft  = false;
  blockedRight = false;
}

void doTurnLeft() {
  myservo.write(90);
  turnL(); Set_Speed(200); delay(300);
  advance(); Set_Speed(130);
}

void doTurnRight() {
  myservo.write(90);
  turnR(); Set_Speed(200); delay(300);
  advance(); Set_Speed(130);
}

void manualAutoAvoid() {
  // Kontrollo IR anësor gjithmonë
  if (millis() > irCooldownUntil) {
    bool irL = readIR(IR_LEFT_PIN);
    bool irR = readIR(IR_RIGHT_PIN);

    if (irL || irR) {
      stopp(); Set_Speed(0); delay(150);
      lastObstacleTime = millis();

      if (irL && !irR) {
        back(); Set_Speed(130); delay(200);
        stopp(); Set_Speed(0); delay(100);
        doTurnRight();
      } else if (irR && !irL) {
        back(); Set_Speed(130); delay(200);
        stopp(); Set_Speed(0); delay(100);
        doTurnLeft();
      } else {
        back(); Set_Speed(130); delay(400);
        stopp(); Set_Speed(0); delay(100);
        doTurnRight();
      }

      irCooldownUntil = millis() + IR_COOLDOWN;
      if (currentMove == 'F') { advance(); Set_Speed(180); }
      lastCmdTime = millis();
      return;
    }
  }

  // Kontrollo ultrasonic vetëm kur po ecën para
  if (currentMove == 'F') {
    DM = checkdistance();
    if (DM < 25) {
      stopp(); Set_Speed(0); delay(500);
      lastObstacleTime = millis();
      Detect_obstacle_distance();

      if (DL >= DR) doTurnLeft();
      else          doTurnRight();

      advance(); Set_Speed(180);
      lastCmdTime = millis();
    }
  }
}

void setup() {
  pinMode(pinLB, OUTPUT); pinMode(pinLF, OUTPUT);
  pinMode(pinRB, OUTPUT); pinMode(pinRF, OUTPUT);
  pinMode(Lpwm_pin, OUTPUT); pinMode(Rpwm_pin, OUTPUT);

  pinMode(SensorLeft, INPUT);
  pinMode(SensorMiddle, INPUT);
  pinMode(SensorRight, INPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);

  myservo.attach(SERVO_PIN);
  myservo.write(90);

  DL = 0; DM = 0; DR = 0;
  stopp();

  Serial.begin(9600);
  delay(1000);
}

void loop() {
  if (Serial.available()) {
    bluetooth_data = Serial.read();
    Serial.println(bluetooth_data);

    switch (bluetooth_data) {
      case 'F':
      case 'U':
        currentMove = 'F';
        currentTimeout = CMD_TIMEOUT_MOVE;
        advance(); Set_Speed(180);
        lastCmdTime = millis();
        break;
      case 'B':
      case 'D':
        currentMove = 'B';
        currentTimeout = CMD_TIMEOUT_MOVE;
        back(); Set_Speed(180);
        lastCmdTime = millis();
        break;
      case 'L':
        currentMove = 'L';
        currentTimeout = CMD_TIMEOUT_TURN;
        turnL(); Set_Speed(180);
        lastCmdTime = millis();
        break;
      case 'R':
        currentMove = 'R';
        currentTimeout = CMD_TIMEOUT_TURN;
        turnR(); Set_Speed(180);
        lastCmdTime = millis();
        break;
      case 'S':
        currentMove = 'S';
        stopp();
        lastCmdTime = 0;
        break;
      case 'T':
        currentMove = 'S';
        stopp();
        lastCmdTime = 0;
        Line_Tracking();
        break;
      case 'O':
        currentMove = 'S';
        stopp();
        lastCmdTime = 0;
        Obstacle_Avoidance();
        break;
      default:
        break;
    }
  }

  if (lastCmdTime != 0 && millis() - lastCmdTime > currentTimeout) {
    stopp();
    currentMove = 'S';
    lastCmdTime = 0;
  }

  if (currentMove != 'S' && lastCmdTime != 0) {
    manualAutoAvoid();
  }
}

void Line_Tracking() {
  int flag = 0;
  while (flag == 0) {

    Sensor_Scan();

    if (SM == HIGH) {
      if (SL == HIGH && SR == LOW) {
        turnL(); Set_Speed(120);
      } else if (SR == HIGH && SL == LOW) {
        turnR(); Set_Speed(120);
      } else {
        advance(); Set_Speed(120);
      }
    } else {
      if (SL == HIGH && SR == LOW) {
        turnL(); Set_Speed(120);
      } else if (SR == HIGH && SL == LOW) {
        turnR(); Set_Speed(120);
      } else {
        back(); Set_Speed(100);
        delay(100);
        stopp();
      }
    }

    if (Serial.available()) {
      bluetooth_data = Serial.read();
      if (bluetooth_data == 'S') {
        flag = 1;
        stopp();
      }
    }
  }
}

void Obstacle_Avoidance() {
  int flag = 0;
  blockedLeft  = false;
  blockedRight = false;
  irCooldownUntil = 0;

  while (flag == 0) {

    if (millis() - lastObstacleTime > BLOCK_FORGET_MS) {
      blockedLeft  = false;
      blockedRight = false;
    }

    if (millis() > irCooldownUntil) {
      bool irL = readIR(IR_LEFT_PIN);
      bool irR = readIR(IR_RIGHT_PIN);

      if (irL || irR) {
        stopp(); Set_Speed(0); delay(150);
        lastObstacleTime = millis();

        if (irL && !irR) {
          blockedLeft = true;
          back(); Set_Speed(130); delay(200);
          stopp(); Set_Speed(0); delay(100);
          doTurnRight();
        } else if (irR && !irL) {
          blockedRight = true;
          back(); Set_Speed(130); delay(200);
          stopp(); Set_Speed(0); delay(100);
          doTurnLeft();
        } else {
          blockedLeft = true; blockedRight = true;
          back(); Set_Speed(130); delay(400);
          stopp(); Set_Speed(0); delay(100);
          doTurnRight();
        }

        irCooldownUntil = millis() + IR_COOLDOWN;
        advance(); Set_Speed(130);

        if (Serial.available()) {
          bluetooth_data = Serial.read();
          if (bluetooth_data == 'S') { flag = 1; stopp(); }
        }
        continue;
      }
    }

    DM = checkdistance();

    if (DM < 30) {
      stopp(); Set_Speed(0); delay(1000);
      lastObstacleTime = millis();
      Detect_obstacle_distance();

      if (DL < 50) blockedLeft  = true;
      if (DR < 50) blockedRight = true;

      bool leftClear  = (DL >= 50) && !blockedLeft;
      bool rightClear = (DR >= 50) && !blockedRight;

      if (leftClear && !rightClear) {
        doTurnLeft();
      } else if (rightClear && !leftClear) {
        doTurnRight();
      } else if (leftClear && rightClear) {
        if (DL >= DR) doTurnLeft();
        else          doTurnRight();
      } else {
        back(); Set_Speed(130); delay(500);
        stopp(); Set_Speed(0); delay(100);
        turnR(); Set_Speed(200); delay(300);
        stopp(); Set_Speed(0); delay(80);
        turnR(); Set_Speed(200); delay(300);
        stopp(); Set_Speed(0); delay(80);
        resetMemory();
        advance(); Set_Speed(130);
      }

    } else {
      advance(); Set_Speed(130);
    }

    if (Serial.available()) {
      bluetooth_data = Serial.read();
      if (bluetooth_data == 'S') {
        flag = 1;
        stopp();
      }
    }
  }
}