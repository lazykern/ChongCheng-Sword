#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"
#include "esp_now.h"
#include "WiFi.h"
#include "pitches.h"
#include <ArduinoQueue.h>

#define BUTTON_PIN 19
#define BUZZER_PIN 18
#define LED_RED 32
#define LED_GREEN 33
#define LED_BLUE 25

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

typedef struct struct_game
{
  uint8_t gameStage;    // 0: waiting, 1: playing, 2: wined
  uint8_t playerNumber; // 1: player1, 2: player2, 0: default
  uint8_t action;       // 1: block, 2: hit(lost blood), 0: default
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

int last_buzzer = 0;
int buzzer_duration = 0;

void buzzer(int note, int duration)
{
  last_buzzer = millis();
  buzzer_duration = duration;
  tone(BUZZER_PIN, note, duration);
}

void stopBuzzer()
{
  noTone(BUZZER_PIN);
  buzzer_duration = 0;
  last_buzzer = 0;
}

void checkBuzzer()
{
  if (buzzer_duration > 0)
  {
    if (millis() - last_buzzer > buzzer_duration)
    {
      stopBuzzer();
    }
  }
}

void setRGB(int R, int G, int B)
{
  analogWrite(LED_RED, R);
  analogWrite(LED_GREEN, G);
  analogWrite(LED_BLUE, B);
}

void showRGBFromHealth(int health)
{
  // Use analogWrite to change the brightness of the LED
  int R = 0;
  int G = 0;
  int B = 0;

  if (health > 50)
  {
    G = 255;
    R = map(health, 50, 100, 255, 0);
  }
  else
  {
    G = map(health, 0, 50, 0, 127);
    R = 255;
  }

  setRGB(R, G, B);
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
  memcpy(&gameData, incomingData, sizeof(gameData));

  if (gameData.gameStage != 1)
  {
    setRGB(0, 0, 0);
    return;
  }

  if (swordData.swordNumber == 1)
  {
    showRGBFromHealth(gameData.player1health);
  }
  else
  {
    showRGBFromHealth(gameData.player2health);
  }

  if (gameData.playerNumber == 0)
  {
    return;
  }

  if (gameData.playerNumber != swordData.swordNumber)
  {
    // Another player blocked
    if (gameData.action == 1)
    {
      buzzer(NOTE_D1, 300);
    }
    // Another player got hit
    else if (gameData.action == 2)
    {
      buzzer(NOTE_F6, 300);
    }
  }
  else
  {
    // This player blocked
    if (gameData.action == 1)
    {
      buzzer(NOTE_F6, 300);
    }
    // This player got hit
    else if (gameData.action == 2)
    {
      buzzer(NOTE_D1, 300);
    }
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

  Serial.begin(9600);

  if (WiFi.macAddress().equalsIgnoreCase("E8:68:E7:22:B6:B8"))
  {
    swordData.swordNumber = 1;
  }
  else if (WiFi.macAddress().equalsIgnoreCase("24:0A:C4:9A:FC:98"))
  {
    swordData.swordNumber = 2;
  }
  else
  {
    swordData.swordNumber = UINT8_MAX;
    return;
  }

  Wire.begin();
  mpu.initialize();

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);

  ledcAttachPin(BUZZER_PIN, 0);

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

  if (swordData.swordNumber == UINT8_MAX)
  {
    Serial.println("Invalid sword number");
    Serial.println(WiFi.macAddress());

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
