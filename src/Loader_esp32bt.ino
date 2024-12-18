/**
  ******************************************************************************
    @file    Loader.h
    @author  Waveshare Team
    @version V2.0.0
    @date    10-August-2018
    @brief   The main file.
             This file provides firmware functions:
              + Initialization of Serial Port, SPI pins and server
              + Main loop

  ******************************************************************************
*/

/* Includes ------------------------------------------------------------------*/
#include "srvr.h" // Server functions

/* Sleep functions -----------------------------------------------------------*/
#define uS_TO_S_FACTOR 1000000ULL /* Conversion factor for micro seconds to seconds */
#define TIME_TO_POWERON  1 * 60 * 1000

void sleep_setup() {
  /*
  First we configure the wake up source
  We set our ESP32 to wake up every 5 seconds
  */
  esp_sleep_enable_timer_wakeup(SLEEP_TIME * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(SLEEP_TIME) + " Seconds");

  /*
  Next we decide what all peripherals to shut down/keep on
  By default, ESP32 will automatically power down the peripherals
  not needed by the wakeup source, but if you want to be a poweruser
  this is for you. Read in detail at the API docs
  http://esp-idf.readthedocs.io/en/latest/api-reference/system/deep_sleep.html
  Left the line commented as an example of how to configure peripherals.
  The line below turns off all RTC peripherals in deep sleep.
  */
  //esp_deep_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  //Serial.println("Configured all RTC Peripherals to be powered down in sleep");
  
  timerPowerOn.startUp(TIME_TO_POWERON);
}

void sleep_start () {
  Serial.println("Going to sleep now");
  esp_deep_sleep_start();
}


/* Entry point ----------------------------------------------------------------*/
void setup()
{
  // Serial port initialization
  Serial.begin(115200);
  delay(10);

  // Storage initialization
  storage_begin();
  //MyFs.rename("/006.img", "/004.img");

  // Bluetooth initialization
  Srvr__btSetup();

  // SPI initialization
  EPD_initSPI();

  // Initialization is complete
  Serial.print("\r\nOk!\r\n");

  // power on to change image
  delay(500);
  if (config.countImg > 0) {
    config.idxImg++;
    config.idxImg %= config.countImg;
    config.idxImg %= MAX_IMG_COUNT;
    configSave();

    storage_show();
  }

  // deep sleep
  sleep_setup();
}

/* The main loop -------------------------------------------------------------*/
void loop()
{
  Srvr__loop();
  
  // deep sleep
  if (timerPowerOn.isTimeOut()) {
    sleep_start();
  }
}
