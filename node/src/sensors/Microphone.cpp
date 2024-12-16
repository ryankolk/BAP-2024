#include "Microphone.h"

// Pins for I2S microphone
#define I2S_NUM I2S_NUM_0
#define I2S_WS 25  // Word select (L/R clock) pin
#define I2S_SD 33  // Data input pin
#define I2S_SCK 26 // Bit clock pin

// FFT parameters
const uint16_t samples = 2048; // Number of samples for FFT (must be a power of 2)
double vReal[samples];
double vImag[samples];
const double SAMPLE_RATE = 16000.0;

ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, samples, SAMPLE_RATE);

// Variables for storing results
double averageDb = 0.0;
double peakFrequency = 0.0; // To store peak frequency
int zeroCrossings = 0;      // To store the count of zero-crossings
bool dataReady = false;     // Flag to signal new data is available

// Mutex to protect shared data
portMUX_TYPE dataMutex = portMUX_INITIALIZER_UNLOCKED;

// Task handle
TaskHandle_t microphoneTaskHandle;

// setup microphone
void setupMicrophone()
{
  // Configure I2S
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 1024,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0};

  i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_SCK,
      .ws_io_num = I2S_WS,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = I2S_SD};

  i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM, &pin_config);
  i2s_start(I2S_NUM);

  // Start microphone processing task
  xTaskCreatePinnedToCore(
      microphoneTask,        // Task function
      "MicrophoneTask",      // Task name
      8192,                  // Stack size
      NULL,                  // Parameter
      5,                     // Priority
      &microphoneTaskHandle, // Task handle
      0                      // Core
  );
}

// Microphone processing task
void microphoneTask(void *param)
{
  while (true)
  {
    // Serial.println("Running microphone task");
    int16_t sampleBuffer[samples];
    size_t bytesRead;

    // Read data from I2S
    i2s_read(I2S_NUM, sampleBuffer, samples * sizeof(int16_t), &bytesRead, portMAX_DELAY);

    // Prepare data for FFT
    for (uint16_t i = 0; i < samples; i++)
    {
      vReal[i] = (double)sampleBuffer[i]; // Copy real part
      vImag[i] = 0.0;                     // Imaginary part set to 0
    }

    // Perform FFT
    FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.compute(FFT_FORWARD);
    FFT.complexToMagnitude();

    // Find the peak frequency in the range of interest (20Hz to 4000Hz)
    double peakFrequencyLocal = 0.0;
    double maxMagnitude = 0.0;

    for (uint16_t i = 1; i < (samples / 2); i++)
    { // Only positive frequencies
      double frequency = (i * SAMPLE_RATE) / samples;
      double magnitude = vReal[i];

      // Filter: Only consider frequencies between 20 Hz and 4000 Hz
      if (frequency >= 20 && frequency <= 4000)
      {
        if (magnitude > maxMagnitude)
        {
          maxMagnitude = magnitude;
          peakFrequencyLocal = frequency;
        }
      }
    }

    // Zero-crossing count
    zeroCrossings = 0;
    for (uint16_t i = 1; i < samples; i++)
    {
      if ((vReal[i - 1] > 0 && vReal[i] < 0) || (vReal[i - 1] < 0 && vReal[i] > 0))
      {
        zeroCrossings++;
      }
    }

    // Calculate average dB
    double totalVolume = 0.0;
    int volumeCount = 0;
    for (uint16_t i = 1; i < (samples / 2); i++)
    {
      totalVolume += vReal[i];
      volumeCount++;
    }
    double averageLinearVolume = totalVolume / volumeCount;
    double localAverageDb = 20.0 * log10(averageLinearVolume);

    // Update shared data with mutex
    portENTER_CRITICAL(&dataMutex);
    averageDb = localAverageDb;
    peakFrequency = peakFrequencyLocal; // Store the peak frequency
    dataReady = true;                   // Signal new data is ready
    portEXIT_CRITICAL(&dataMutex);

    vTaskDelay(200 / portTICK_PERIOD_MS); // Process every second
  }
}

// Function to get microphone data
bool getMicrophoneData(double &avgDb, double &peakestFrequency, int &zeroCrossingsCount)
{
  bool ready = false;

  // Access shared data with mutex
  portENTER_CRITICAL(&dataMutex);
  if (dataReady)
  {
    avgDb = averageDb;

    // The peak frequency should already be calculated in your microphone task.
    peakestFrequency = peakFrequency; // `peakFrequency` holds the peak frequency

    zeroCrossingsCount = zeroCrossings; // Get the zero-crossing count

    dataReady = false; // Reset flag
    ready = true;
  }
  portEXIT_CRITICAL(&dataMutex);

  return ready;
}
