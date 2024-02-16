
#include "Wire.h"
#include "I2Cdev.h"
#include "MPU6050.h"

MPU6050 mpu;
int16_t ax, ay, az;
int16_t gx, gy, gz;

uint8_t sword_state = 0; // 0 = Sword Up, 1 = Slashed
uint slash_count = 0;

const uint SLASH_COOLDOWN = 500;
const uint PRINT_INTERVAL = 500;

ulong last_print = 0;
ulong last_slash = 0;

enum Direction
{
    UP,
    DOWN,
    LEFT,
    RIGHT,
};

const String DIRECTIONS[] = {
    "UP",
    "DOWN",
    "LEFT",
    "RIGHT",
};

Direction next_slash_direction = UP;

void setup()
{
    Serial.begin(9600);
    Wire.begin();
    mpu.initialize();
}

void loop()
{
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

    ax = min(17000, max(-17000, (int)ax));
    ay = min(17000, max(-17000, (int)ay));
    az = min(17000, max(-17000, (int)az));

    ax = map(ax, -17000, 17000, -127, 127);
    ay = map(ay, -17000, 17000, -127, 127);
    az = map(az, -17000, 17000, -127, 127);

    gx = map(gx, -32768, 32767, -127, 127);
    gy = map(gy, -32768, 32767, -127, 127);
    gz = map(gz, -32768, 32767, -127, 127);

    if (gz == 127)
    {
        if (sword_state == 1)
            return;

        Serial.printf("%s Slash %d\n", DIRECTIONS[next_slash_direction], slash_count);

        Serial.printf("ax: %d, ay: %d, az: %d\n", ax, ay, az);
        Serial.printf("gx: %d, gy: %d, gz: %d\n", gx, gy, gz);

        last_slash = millis();
        slash_count += 1;
        sword_state = 1;
    }
    else
    {
        sword_state = 0;

        if (abs(ay) > abs(az))
        {
            if (ay > 60)
                next_slash_direction = DOWN;
            else if (ay < -60)
                next_slash_direction = UP;
        }
        else
        {
            if (az > 60)
                next_slash_direction = RIGHT;
            else if (az < -60)
                next_slash_direction = LEFT;
        }

        Serial.printf("%s ax: %d, ay: %d, az: %d, gx: %d, gy: %d, gz: %d\n", DIRECTIONS[next_slash_direction], ax, ay, az, gx, gy, gz);
    }
}