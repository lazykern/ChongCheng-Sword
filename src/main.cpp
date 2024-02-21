#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"
#include "esp_now.h"
#include "WiFi.h"

#define BUTTON_PIN 19
#define BUZZER_PIN 34

MPU6050 mpu;
int16_t ax, ay, az;
int16_t gx, gy, gz;

const uint SLASH_COOLDOWN = 500;
const int16_t DIRECTION_CHANGE_THRESHOLD = 50;
const int DEBOUNCE_THRESHOLD = 25;

int slash_count = 0;

ulong last_slash = 0;
ulong last_change_direction = 0;
bool blocking = false;
char currentOrientation = 'v';
char lastOrientation = 'v';

// 3C:61:05:03:68:74
uint8_t serverAddress[] = {0x3C, 0x61, 0x05, 0x03, 0x68, 0x74};

esp_now_peer_info_t peerInfo;

// Define a data structure
typedef struct struct_message
{
  uint8_t swordNumber;
  char orientation;
  uint8_t action;
} struct_message;

typedef struct struct_game {
  uint8_t gameStage;      // 0: waiting, 1: playing, 2: wined
  uint8_t playerNumber;   // 1: player1, 2: player2, 0: default
  uint8_t action;         // 1: block, 2: hit(lost blood), 0: default
  int player1health;
  int player2health;
} struct_game;

// Create a structured object
struct_message swordData;
struct_game gameData;

enum SwordStates
{
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

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&gameData, incomingData, sizeof(gameData));
  Serial.print("Bytes received: ");
  Serial.println(len);
  Serial.print("Game Stage: ");
  Serial.println(gameData.gameStage);
  Serial.print("Player Number: ");
  Serial.println(gameData.playerNumber);
  Serial.print("Action: ");
  Serial.println(gameData.action);
  Serial.print("Player 1 Health: ");
  Serial.println(gameData.player1health);
  Serial.print("Player 2 Health: ");
  Serial.println(gameData.player2health);

  if (gameData.playerNumber == 0) {
    return;
  }

  if (gameData.playerNumber != swordData.swordNumber) {
    // TODO
  } else {

  }
}


void debounce(int now)
{
  if (now == direction_last)
  {
    direction_debounce++;
  }
  else
  {
    direction_debounce = 1;
    direction_last = now;
  }

  if (direction_debounce == DEBOUNCE_THRESHOLD)
  {
    direction = now;
  }
}

void updateDirection()
{
  if (az > DIRECTION_CHANGE_THRESHOLD)
  {
    debounce(HAND_UP);
  }
  else if (az < -DIRECTION_CHANGE_THRESHOLD)
  {
    debounce(HAND_DOWN);
  }
  else if (ay > DIRECTION_CHANGE_THRESHOLD)
  {
    debounce(TIP_UP);
  }
  else if (ay < -DIRECTION_CHANGE_THRESHOLD)
  {
    debounce(TIP_DOWN);
  }
  else if (ax > DIRECTION_CHANGE_THRESHOLD)
  {
    debounce(TIP_F_NORMAL);
  }
  else if (ax < -DIRECTION_CHANGE_THRESHOLD)
  {
    debounce(TIP_F_TWIST);
  }
}

void slash(char orientation)
{
  Serial.printf("%c %d\n", orientation, slash_count++);
  last_slash = millis();

  swordData.action = 1;
  swordData.orientation = orientation;

  esp_err_t result = esp_now_send(serverAddress, (uint8_t *)&swordData, sizeof(swordData));
}

void updateOrientation()
{
  if (direction == TIP_UP)
  {
    currentOrientation = 'v';
  }
  else if (direction == TIP_DOWN)
  {
    currentOrientation = 'v';
  }
  else if (direction == HAND_UP)
  {
    currentOrientation = 'h';
  }
  else if (direction == HAND_DOWN)
  {
    currentOrientation = 'h';
  }
  else if (direction == TIP_F_NORMAL)
  {
    currentOrientation = 'v';
  }
  else if (direction == TIP_F_TWIST)
  {
    currentOrientation = 'v';
  }
}

void checkSlash()
{
  if ((millis() - last_slash > SLASH_COOLDOWN) && gz == 127)
  {
    slash(currentOrientation);
  }
}

void block()
{
  blocking = true;
  Serial.printf("Blocking %c\n", currentOrientation);
  swordData.action = 2;
  swordData.orientation = currentOrientation;
  esp_err_t result = esp_now_send(serverAddress, (uint8_t *)&swordData, sizeof(swordData));
}

void unblock()
{
  blocking = false;
  Serial.printf("Unblocking %c\n", currentOrientation);
  swordData.action = 0;
  swordData.orientation = currentOrientation;
  esp_err_t result = esp_now_send(serverAddress, (uint8_t *)&swordData, sizeof(swordData));
}

void setup()
{

  Serial.println(WiFi.macAddress());

  if (WiFi.macAddress().equalsIgnoreCase("E8:68:E7:22:B6:B8"))
  {
    swordData.swordNumber = 1;
  }
  else if (WiFi.macAddress().equalsIgnoreCase("24:6F:28:D1:F2:34"))
  {
    swordData.swordNumber = 2;
  }
  else
  {
    swordData.swordNumber = UINT8_MAX;
    return;
  }

  Serial.begin(9600);
  Wire.begin();
  mpu.initialize();

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);

  esp_now_register_send_cb(OnDataSent);

  memcpy(peerInfo.peer_addr, serverAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    Serial.println("Failed to add peer");
    return;
  }

}

void getMotion()
{
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

void loop()
{

  if (swordData.swordNumber == UINT8_MAX) {
    Serial.println("Invalid sword number");
    return;
  }

  getMotion();

  updateDirection();

  lastOrientation = currentOrientation;

  updateOrientation();

  if (digitalRead(BUTTON_PIN) == LOW)
  {

    // digitalWrite(BUZZER_PIN, HIGH);

    if (!blocking || (blocking && lastOrientation != currentOrientation))
    {
      block();
    }
  
  }
  else
  {

    // digitalWrite(BUZZER_PIN, LOW);

    if (blocking)
    {
      unblock();
    }

    checkSlash();
  }
}
