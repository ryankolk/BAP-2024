#include "Accelerometer.h"

Adafruit_MPU6050 mpu;

// Accelerometer pin connections
#define SDA_PIN 21 // SDA pin
#define SCL_PIN 22 // SCL pin

// Variables for continuous accelerometer updates
float roll = 0, pitch = 0, yaw = 0;

unsigned long lastTime = 0;
// set up the accelerometer
void setupAccelerometer()
{
  Wire.begin(SDA_PIN, SCL_PIN); // Initialize I2C with custom SDA and SCL pins
  if (!mpu.begin())
  {
    Serial.println("Failed to find MPU6050 chip");
    while (1)
    {
      delay(10);
    }
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
}

// Calibration offsets

// Variables to store the accelerometer offsets
float accelOffsetX = 0.0;
float accelOffsetY = 0.0;
float accelOffsetZ = 0.0;

// Variables to store the gyroscope offsets
float gyroOffsetX = 0.0;
float gyroOffsetY = 0.0;
float gyroOffsetZ = 0.0;

// Constants for gravitational acceleration
const float GRAVITY = 9.81; // m/s^2

void calibrateAccelerometer()
{
  float gyroX = 0, gyroY = 0, gyroZ = 0;
  float accelX = 0, accelY = 0, accelZ = 0;
  int sampleCount = 100; // Number of samples for averaging

  Serial.println("Calibrating sensor, keep the sensor still...");

  // Collect sampleCount readings of sensor data
  for (int i = 0; i < sampleCount; i++)
  {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    // Sum up accelerometer readings
    accelX += a.acceleration.x;
    accelY += a.acceleration.y;
    accelZ += a.acceleration.z;

    // Sum up gyroscope readings
    gyroX += g.gyro.x;
    gyroY += g.gyro.y;
    gyroZ += g.gyro.z;

    delay(50); // Small delay between readings
  }

  // Calculate the average offset for accelerometer and gyroscope
  accelOffsetX = accelX / sampleCount;
  accelOffsetY = (accelY / sampleCount);
  accelOffsetZ = (accelZ / sampleCount) - GRAVITY;

  // Gyroscope calibration is already covered in the previous example
  gyroOffsetX = gyroX / sampleCount;
  gyroOffsetY = gyroY / sampleCount;
  gyroOffsetZ = gyroZ / sampleCount;

  Serial.println("Calibration complete!");
  Serial.printf("Accelerometer offsets: X: %.2f, Y: %.2f, Z: %.2f\n", accelOffsetX, accelOffsetY, accelOffsetZ);
  Serial.printf("Gyroscope offsets: X: %.2f, Y: %.2f, Z: %.2f\n", gyroOffsetX, gyroOffsetY, gyroOffsetZ);
  lastTime = millis();
}

// Task for continuous accelerometer updates
void accelerometerTask(void *parameter)
{
  static float previousYaw = 0;

  while (true)
  {
    // Serial.println("running task");
    sensors_event_t a,
        g, temp;
    if (xSemaphoreTake(xSemaphore, (TickType_t)10) == pdTRUE)
    {
      // Critical section
      mpu.getEvent(&a, &g, &temp);

      // Give the semaphore back
      xSemaphoreGive(xSemaphore);

      // Serial.printf("Accelerometer offsets: X: %.2f, Y: %.2f, Z: %.2f\n", a.acceleration.x - accelOffsetX, a.acceleration.y - accelOffsetY, a.acceleration.z - accelOffsetZ);
      // Serial.printf("Gyroscope offsets: X: %.2f, Y: %.2f, Z: %.2f\n", g.gyro.x - gyroOffsetX, g.gyro.y - gyroOffsetY, g.gyro.z - gyroOffsetZ);

      // Calculate roll and pitch
      roll = atan2((a.acceleration.y - accelOffsetY), (a.acceleration.z - accelOffsetZ)) * 180 / PI;
      pitch = atan2(-(a.acceleration.x - accelOffsetX), sqrt((a.acceleration.y - accelOffsetY) * (a.acceleration.y - accelOffsetY) + (a.acceleration.z - accelOffsetZ) * (a.acceleration.z - accelOffsetZ))) * 180 / PI;

      //@TODO: no safe for overflow
      unsigned long diff = millis() - lastTime;
      lastTime = millis();
      // Integrate yaw from gyroscope data
      float newYaw = previousYaw + ((g.gyro.z - gyroOffsetZ) * (diff / 1000)) * 180 / PI; // Assuming 10ms interval
      // previousYaw = yaw;

      // Only update yaw if change is greater than 0.5 degrees
      if (fabs(newYaw - previousYaw) >= 0)
      {
        yaw = newYaw; // Update yaw

        if (yaw > 180)
        {
          yaw -= 360; // Wrap around if greater than 180
        }
        else if (yaw < -180)
        {
          yaw += 360; // Wrap around if less than -180
        }

        previousYaw = yaw; // Update previousYaw to the new yaw value
      }

      delay(10);
    }
    else
    {
      // Failed to take the semaphore
      // Serial.println("Failed to take semaphore");
      delay(10);
    }
  }
}

// Function to fetch the latest accelerometer data
void getAccelerometerData(float &currentRoll, float &currentPitch, float &currentYaw)
{
  currentRoll = roll;
  currentPitch = pitch;
  currentYaw = yaw;
}

// This function is called in the setup to create the task
void startAccelerometerTask()
{
  xTaskCreatePinnedToCore(
      accelerometerTask,    // Task function
      "Accelerometer Task", // Task name
      4096,                 // Stack size (in bytes)
      NULL,                 // Task parameters
      10,                   // Priority
      NULL,                 // Task handle
      1                     // Core to run the task on
  );
}
