#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"
#include "esp_now.h"
#include "WiFi.h"


MPU6050 mpu;
int16_t ax, ay, az;
int16_t gx, gy, gz;

const uint SLASH_COOLDOWN = 500;
const int16_t DIRECTION_CHANGE_THRESHOLD = 50;
const int DEBOUNCE_THRESHOLD = 25;

int slash_count = 0;

ulong last_slash = 0;
ulong last_change_direction = 0;

// 3C:61:05:03:68:74
uint8_t serverAddress[] = {0x3C, 0x61, 0x05, 0x03, 0x68, 0x74};

esp_now_peer_info_t peerInfo;

// Define a data structure
typedef struct struct_message {
  uint8_t swordNumber;
  char direction;
  uint8_t action;
} struct_message;

// Create a structured object
struct_message myData;


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

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
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

void slash(char direction) {
    Serial.printf("%c %d\n", direction, slash_count++);
    last_slash = millis();

    myData.action = 1;
    myData.direction = direction;

    esp_err_t result = esp_now_send(serverAddress, (uint8_t *) &myData, sizeof(myData));
}

void checkSlash() {

  char slashDirection;

  if (direction == TIP_UP) {
    slashDirection = 'v';
  } else if (direction == TIP_DOWN) {
    slashDirection = 'v';
  } else if (direction == HAND_UP) {
    slashDirection = 'h';
  } else if (direction == HAND_DOWN) {
    slashDirection = 'h';
  } else if (direction == TIP_F_NORMAL) {
    slashDirection = 'v';
  } else if (direction == TIP_F_TWIST) {
    slashDirection = 'v';
  }

  if ((millis() - last_slash > SLASH_COOLDOWN) && gz == 127) {
    slash(slashDirection);
  }
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  mpu.initialize();

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(OnDataSent);
  
  memcpy(peerInfo.peer_addr, serverAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

  if (WiFi.macAddress().equalsIgnoreCase("E8:68:E7:22:B6:B8")) {
    myData.swordNumber = 1;
  } else if (WiFi.macAddress().equalsIgnoreCase("")) {
    myData.swordNumber = 2;
  } else {
    myData.swordNumber = UINT8_MAX;
  }
  
}

void getMotion() {
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
}

void loop() {

  getMotion();

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

  checkSlash();
}
