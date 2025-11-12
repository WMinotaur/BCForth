#include <iostream>
#include <stdio.h>
#include <fcntl.h>
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_spiffs.h"

namespace ESP32
{
    static const char* TAG = "Forth ESP32 implementation";

    void configure()
    {
        if (uart_driver_install(UART_NUM_0, 2*1024, 0, 0, NULL, 0) != ESP_OK) {
            ESP_LOGE(TAG, "Driver installation failed");
        }

        uart_config_t uart_config = {
            .baud_rate = 115200,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
            .rx_flow_ctrl_thresh = 122
        };

        uart_param_config(UART_NUM_0, &uart_config);

        esp_vfs_dev_uart_use_driver(UART_NUM_0);
    }

    void register_spiffs(void)
    {
    esp_vfs_spiffs_conf_t config = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true,
    };
    esp_vfs_spiffs_register(&config);
    }

    void unregister_spiffs(void)
    {
        esp_vfs_spiffs_unregister(NULL);
    }
}