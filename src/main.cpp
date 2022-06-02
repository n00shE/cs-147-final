#include <Arduino.h>
#include <SparkFunLSM6DSO.h>
#include "driver/i2s.h"
#include <algorithm>
#include <WiFi.h>
#include <HttpClient.h>

using std::string;

//https://github.com/atomic14/esp32_audio

#define BCKL 25
#define LRCL 27
#define DOUT 26

char ssid[] = "SETUP-E0D3";    // your network SSID (name) 
char pass[] = "artist8968fever"; // your network password (use for WPA, or use as key for WEP)


const i2s_port_t I2S_PORT = I2S_NUM_0;
const int BLOCK_SIZE = 1024;
const int SAMPLE_SIZE = 16384 / 4;
const int NOISE_THRESH = -22136;
const int MAX_NOISE = 32768;
float base;
const int do_http = 0;


LSM6DSO myIMU;

int16_t *samples = (int16_t *)malloc(sizeof(uint16_t) * SAMPLE_SIZE);

// Name of the server we want to connect to
const char kHostname[] = "52.90.95.83";
// Path to download (this is the bit after the hostname in the URL
// that you want to download
const char kPath[] = "/?var=";

// Number of milliseconds to wait without receiving any data before we give up
const int kNetworkTimeout = 30*1000;
// Number of milliseconds to wait if no data is available before trying again
const int kNetworkDelay = 1000;

WiFiClient c;
HttpClient http(c);

void printall() {
  Serial.print("\nAccelerometer:\n");
  Serial.print(" X = ");
  Serial.println(myIMU.readFloatAccelX(), 3);
  Serial.print(" Y = ");
  Serial.println(myIMU.readFloatAccelY(), 3);
  Serial.print(" Z = ");
  Serial.println(myIMU.readFloatAccelZ(), 3);

  Serial.print("\nGyroscope:\n");
  Serial.print(" X = ");
  Serial.println(myIMU.readFloatGyroX(), 3);
  Serial.print(" Y = ");
  Serial.println(myIMU.readFloatGyroY(), 3);
  Serial.print(" Z = ");
  Serial.println(myIMU.readFloatGyroZ(), 3);
}

void printAccel() {
    Serial.print(myIMU.readFloatAccelZ(), 4);
    Serial.print(",");
    Serial.print(myIMU.readFloatAccelX(), 4);
    Serial.print(",");
    Serial.print(myIMU.readFloatAccelY(), 4);
    Serial.println();
}


void setup() {
  Serial.begin(115200);
  Wire.begin();
  esp_err_t err;
  Serial.println("Configuring I2S...");
  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX), // Receive, not transfer
    .sample_rate = 16000,                         // 16KHz
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // could only get it to work with 32bits
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // although the SEL config should be left, it seems to transmit on right
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,     // Interrupt level 1
    .dma_buf_count = 4,                           // number of buffers
    .dma_buf_len = BLOCK_SIZE,// samples per buffer
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0                  
  };
  const i2s_pin_config_t pin_config = {
    .bck_io_num = BCKL,   // BCKL
    .ws_io_num = LRCL,    // LRCL
    .data_out_num = -1, // not used (only for speakers)
    .data_in_num = DOUT   // DOUT
  };
  err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed installing driver: %d\n", err);
    while (true);
  }
  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Failed setting pin: %d\n", err);
    while (true);
  }
  Serial.println("I2S driver installed.");

  if( myIMU.begin() )
    Serial.println("IMU Ready.");
  else { 
    Serial.println("Could not connect to IMU.");
    while(1);
  }

  if( myIMU.initialize(BASIC_SETTINGS) ) //PEDOMETER_SETTINGS
    Serial.println("Loaded IMU Settings.");


  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("MAC address: ");
  Serial.println(WiFi.macAddress());
  Serial.println();
  base = myIMU.readFloatAccelZ();
}

void loop() {
    int32_t raw_samples[256];
    //int sample_index = 0;
    int count = SAMPLE_SIZE;
    int16_t highest = 0;
    //int16_t lowest = 0;
    while (count > 0)
    {
      //int mean = 0;
        size_t bytes_read = 0;
        i2s_read(I2S_PORT, (void **)raw_samples, sizeof(int32_t) * ((count < 256) ? count : 256), &bytes_read, portMAX_DELAY);
        int samples_read = bytes_read / sizeof(int32_t);
        for (int i = 0; i < samples_read; i++)
        {
            //samples[sample_index] = (raw_samples[i] & 0xFFFFFFF0) >> 11;
            //sample_index++;

            //mean += samples[sample_index];

            int16_t current = (raw_samples[i] & 0xFFFFFFF0) >> 11;
            count--;
            if (current > highest) {
                highest = current;
            }
            //if (current < lowest) {
            //    lowest = current;
            //}
            //Serial.println(samples[sample_index]);
        }
    }
    //printall();
    float movement = myIMU.readFloatAccelY() * 10;
    //base = movement;
    float audio = (float)highest / (float)MAX_NOISE;
    Serial.print(audio, 4);
    Serial.print(",");
    Serial.print(movement, 4);
    Serial.println();
    //delay(500);

    int err = 0;

    if (do_http == 1) {
      char temp[40];
      //dtostrf(audio, 2, 3, &temp[0]);
      //Serial.println(temp);
      String temps = "/?var=";
      //temps.concat(std::to_string(audio));
      temps.concat(audio);
      temps.concat(",");
      temps.concat(movement);
      temps.toCharArray(temp, 40);
      err = http.get(kHostname, temp);
      if (err == 0)
      {
        Serial.println("startedRequest ok");

        err = http.responseStatusCode();
        if (err >= 0)
        {
          Serial.print("Got status code: ");
          Serial.println(err);
        }
      }
      http.stop();
    }
    delay(500);
}