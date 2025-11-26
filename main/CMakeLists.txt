# idf_component_register(
#     SRCS "app_main.c"
#     PRIV_REQUIRES mqtt nvs_flash esp_netif esp_wifi esp_event protocol_examples_common esp_driver_gpio
#     INCLUDE_DIRS "."
# )

idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_wifi esp_event mqtt nvs_flash esp_driver_gpio
)