#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"

MPU6050 mpu;
int16_t ax, ay, az;
int16_t gx, gy, gz;

const uint SLASH_COOLDOWN = 500;
const int16_t DIRECTION_CHANGE_THRESHOLD = 50;
const int DEBOUNCE_THRESHOLD = 50;
int slash_count = 0;

ulong last_slash = 0;
ulong last_change_direction = 0;

enum SwordStates {
  TIP_UP,
  TIP_DOWN,
  HAND_UP,
  HAND_DOWN,
  TIP_F_NORMAL,
  TIP_F_TWIST,
};

const String SWORD_STATES[] = {
    "TIP_UP",
    "TIP_DOWN",
    "HAND_UP",
    "HAND_DOWN",
    "TIP_F_NORMAL",
    "TIP_F_TWIST",
};

int direction = -1;
int direction_last = -1;
int direction_debounce = 0;

void setup() {
  Serial.begin(9600);
  Wire.begin();
  mpu.initialize();
}

void debounce(int now) {
  if (now == direction_last) {
    direction_debounce++;
  } else {
    direction_debounce = 1;
    direction_last = now;
  }

  if (direction_debounce == DEBOUNCE_THRESHOLD) {
    direction = now;
  }
}

void checkSlashDirection() {
  String slashType;

  if (direction == TIP_UP) {
    slashType = "vertical";
  } else if (direction == TIP_DOWN) {
    slashType = "vertical";
  } else if (direction == HAND_UP) {
    slashType = "horizontal";
  } else if (direction == HAND_DOWN) {
    slashType = "horizontal";
  } else if (direction == TIP_F_NORMAL) {
    slashType = "vertical";
  } else if (direction == TIP_F_TWIST) {
    slashType = "vertical";
  }

  ulong now = millis();

  if ((now - last_slash > SLASH_COOLDOWN) && gz == 127) {
    Serial.printf("%s %d\n", slashType, slash_count++);
    last_slash = now;
  }
}

void loop() {
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  ax = constrain(ax, -17000, 17000);
  ay = constrain(ay, -17000, 17000);
  az = constrain(az, -17000, 17000);

  ax = map(ax, -17000, 17000, -127, 127);
  ay = map(ay, -17000, 17000, -127, 127);
  az = map(az, -17000, 17000, -127, 127);

  gx = map(gx, -32768, 32767, -127, 127);
  gy = map(gy, -32768, 32767, -127, 127);
  gz = map(gz, -32768, 32767, -127, 127);

  if (az > DIRECTION_CHANGE_THRESHOLD) {
    debounce(HAND_UP);
  } else if (az < -DIRECTION_CHANGE_THRESHOLD) {
    debounce(HAND_DOWN);
  } else if (ay > DIRECTION_CHANGE_THRESHOLD) {
    debounce(TIP_UP);
  } else if (ay < -DIRECTION_CHANGE_THRESHOLD) {
    debounce(TIP_DOWN);

  } else if (ax > DIRECTION_CHANGE_THRESHOLD) {
    debounce(TIP_F_NORMAL);
  } else if (ax < -DIRECTION_CHANGE_THRESHOLD) {
    debounce(TIP_F_TWIST);
  }

  checkSlashDirection();
}
