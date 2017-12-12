# iot_mqtt_lora

ESP32 (wifi_station or/and wifi_ap, mqtt_client, sntp_client, tls_server, ota, websocket_server, web_server, ...)  +  any sensors + lora module + ws2812 + cdcard

hardware:
- esp32
- ssd1306
- lora module HM-TRLR-S-868 (HopeRF)
- i2c sensors (bmp280 + bh1750 + apds9960 + si7021)
- gpio sensor RCWL-0516
- ws2812
- cdcard


# Файлы пакета:

* sdkconfing	- файл конфигурации проекта

* Makefile	- make файл (файл сценария компиляции проекта)

* version	- файл версии ПО

* README.md	- файл справки

* main/		- папка исходников


Требуемые компоненты:
```
- Cross compiler xtensa-esp32-elf (http://esp-idf-fork.readthedocs.io/en/stable/linux-setup.html#step-0-prerequisites)
- SDK esp-idf (https://github.com/espressif/esp-idf)
- Python2 (https://www.python.org/)
```

# Компиляция и загрузка

make menuconfig - конфигурация проекта

make app	- компиляция проекта

make flash	- запись бинарного кода проекта в dataflash
