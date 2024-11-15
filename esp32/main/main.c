#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>

#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "i2c_base.h"
#include "scd40.h"
#include "bme280.h"
#include "sd_card.h"
#include "wifi.h"

static const char* MAIN_TAG = "app_main";

void app_main(void){
  esp_err_t r = ESP_OK;
  esp_log_level_set(MAIN_TAG, ESP_LOG_DEBUG);
 
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  // Is time set? If not, tm_year will be (1970 - 1900).
  if (timeinfo.tm_year < (2024 - 1900)) {
    ESP_LOGI(MAIN_TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
    obtain_time();
    // update 'now' variable with current time
    time(&now);
  }

  char time_buf[64];

  // Set timezone to JST Standard Time
  setenv("TZ", "JST-9", 1);
  tzset();
  localtime_r(&now, &timeinfo);
  strftime(time_buf, sizeof(time_buf), "%Y/%m/%d %H:%M:%S", &timeinfo);
  ESP_LOGI(MAIN_TAG, "The current date/time in Japan is: %s", time_buf);
  
  const char* pScd40_data_filepath = MOUNT_POINT"/scd40.txt";
  char sdcard_write_data[MAX_SDCARD_LINE_CHAR_SIZE] = "\0";
  
  scd40_value_t scd40_value = {
    .co2 = 0,
    .temperature = 0.0, 
    .relative_humidity = 0.0
  };
  
  bme280_value_t bme280_value = {
    .temperature = 0.0,
    .pressure = 0.0,
    .humidity = 0.0
  };

  float temperature_offset = 0.0;

  if(r == ESP_OK){
    ESP_LOGI(MAIN_TAG, "initilize SD Card setup.");
    r =  init_sd_card();
  }
  
  if(r == ESP_OK){
    snprintf(sdcard_write_data, sizeof(sdcard_write_data), "YYYY/MM/DD HH/MM/SS,CO2[rpm],Temperature[degree],Humidity[%%RH],Pressure[hPa]\n");
    r = write_sd_card_file(pScd40_data_filepath, sdcard_write_data, 'w');
  }

  if(r == ESP_OK){
    r = i2c_port_init();
  }
  if(r == ESP_OK){
    r = init_scd40();
  }
  if(r != ESP_OK){
    ESP_LOGE(MAIN_TAG, "faild to set up scd40.");
  }

  if(r == ESP_OK){
    r = check_scd40_serial_number();
    if(r != ESP_OK){
      vTaskDelay(500/ portTICK_PERIOD_MS);
      r = stop_scd40_periodic_measurement();
      if(r == ESP_OK){
        vTaskDelay(500/ portTICK_PERIOD_MS);
        r = check_scd40_serial_number();
      }
    }
  }
  
  if(r == ESP_OK){
    r = get_scd40_temperature_offset(&temperature_offset);
    if(r == ESP_OK){
      ESP_LOGI(MAIN_TAG, "default temperature_offset is %f", temperature_offset);
      vTaskDelay(1 / portTICK_PERIOD_MS);
    }
  }

  if(r == ESP_OK){
    temperature_offset = 0.0;
    ESP_LOGI(MAIN_TAG, "set temperature offset: %f", temperature_offset);
    r = set_scd40_temperature_offset(temperature_offset);
    vTaskDelay(5/ portTICK_PERIOD_MS);
  }

  if(r == ESP_OK){
    r = init_bme280();
    if(r != ESP_OK){
      ESP_LOGE(MAIN_TAG, "fail to initialize bme280 setup.");
    }
  }

 
  while(1){
    if(r == ESP_OK){
      time(&now);
      localtime_r(&now, &timeinfo);
      strftime(time_buf, sizeof(time_buf), "%Y/%m/%d %H:%M:%S", &timeinfo);
    }
    if(r == ESP_OK){
      r = start_scd40_periodic_measurement();
    }
    if(r == ESP_OK){
      vTaskDelay(5000/ portTICK_PERIOD_MS);
      r = get_scd40_sensor_data(&scd40_value);
      if(r == ESP_OK){
        ESP_LOGI(MAIN_TAG, "%s co2:%d, temperature:%f, humidity:%f",
            time_buf, scd40_value.co2, scd40_value.temperature, scd40_value.relative_humidity);
      }
    }
    if(r == ESP_OK){
      vTaskDelay(5/ portTICK_PERIOD_MS);
      r = stop_scd40_periodic_measurement();
    }
    if(r == ESP_OK){
      r = get_bme280_data(&bme280_value);
      ESP_LOGI(MAIN_TAG, "temperature:%lf, pressure:%lf, humidity:%lf", 
          bme280_value.temperature,
          bme280_value.pressure,
          bme280_value.humidity);
      if(r != ESP_OK){
        ESP_LOGE(MAIN_TAG, "fail to read bme280 data");
      }
    }
    if(r == ESP_OK){
      snprintf(sdcard_write_data, sizeof(sdcard_write_data),
          "%s,%d,%.2lf,%.2lf,%.2lf\n",time_buf, scd40_value.co2, bme280_value.temperature, bme280_value.humidity, bme280_value.pressure);
      r = write_sd_card_file(pScd40_data_filepath, sdcard_write_data, 'a');
      ESP_LOGI(MAIN_TAG, "write scd40 data to sd card.");
    }
  }
}
