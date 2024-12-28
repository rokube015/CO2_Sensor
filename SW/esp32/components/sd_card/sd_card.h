#pragma once
#include "esp_err.h"

class SD_CARD{
  private:
    constexpr static char* SD_CARD_TAG = "sd_card";
    
    constexpr static char MOUNT_POINT[] = "/sdcard";
    constexpr static int SPI_MISO_PIN  = GPIO_NUM_19;
    constexpr static int SPI_MOSI_PIN  = GPIO_NUM_23;
    constexpr static int SPI_CLK_PIN   = GPIO_NUM_18;
    constexpr static gpio_num_t SPI_CS_PIN    = GPIO_NUM_5;
   
    constexpr static int MAX_SDCARD_LINE_CHAR_SIZE = 512;
  public:
    SD_CARD();

    esp_err_t init();

    esp_err_t write_data(const char* pfile_path, size_t file_path_size, char* data, char mode);

    esp_err_t read_data(const char* path);
};