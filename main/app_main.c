#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "driver/gpio.h"

static const char *TAG = "MAIN";

/********************* PINOS *************************/
#define LED_PIN  2
#define BTN_PIN  4

/********************* WIFI *************************/

#define WIFI_SSID      "DOUGLAS_VLINK"
#define WIFI_PASSWORD  "06191005c"

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi desconectado, tentando reconectar...");
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "WiFi conectado! IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void wifi_init(void)
{
    esp_netif_init();
    esp_event_loop_create_default();

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
}


static esp_mqtt_client_handle_t mqtt_client;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event->event_id) {

        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT conectado!");

            esp_mqtt_client_subscribe(event->client, "led/acao", 0);

            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT DATA: [%.*s]", event->data_len, event->data);

            if (strncmp(event->topic, "led/acao", event->topic_len) == 0) {

                if (event->data[0] == '1') {
                    gpio_set_level(LED_PIN, 1);
                    ESP_LOGI(TAG, "LED LIGADO via MQTT");
                }
                else if (event->data[0] == '0') {
                    gpio_set_level(LED_PIN, 0);
                    ESP_LOGI(TAG, "LED DESLIGADO via MQTT");
                }
            }
            break;

        default:
            break;
    }
}

void mqtt_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://192.168.0.158:1883",
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, mqtt_client);
    esp_mqtt_client_start(mqtt_client);
}

/********************* LEITURA DO BOTÃO *************************/

void button_task(void *pv)
{
    int last_state = 1; // pull-up, então começa em 1 (solto)

    while (1) {

        int current_state = gpio_get_level(BTN_PIN);

        if (current_state != last_state) {
            vTaskDelay(50 / portTICK_PERIOD_MS);
            current_state = gpio_get_level(BTN_PIN);

            if (current_state != last_state) {
                last_state = current_state;

                if (current_state == 0) {
                    ESP_LOGI(TAG, "Botão PRESSIONADO");
                    esp_mqtt_client_publish(mqtt_client, "bnt/estado", "1", 0, 0, 0);
                } else {
                    ESP_LOGI(TAG, "Botão SOLTO");
                    esp_mqtt_client_publish(mqtt_client, "bnt/estado", "0", 0, 0, 0);
                }
            }
        }

        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

/********************* APP MAIN *************************/

void app_main(void)
{
    nvs_flash_init();
    wifi_init();

    vTaskDelay(3000 / portTICK_PERIOD_MS);

    mqtt_start();

    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    gpio_reset_pin(BTN_PIN);
    gpio_set_direction(BTN_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BTN_PIN, GPIO_PULLUP_ONLY);

    xTaskCreate(button_task, "button_task", 2048, NULL, 5, NULL);
}
