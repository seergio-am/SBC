#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include <esp_http_server.h>
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/api.h>
#include <lwip/netdb.h>
#include "lwip/err.h"
#include "lwip/sys.h"
#include <ultrasonic.h>
#include <ultrasonic2.h>
#include <esp_err.h>

//pines Ultrasonidos atrás
#define TRIGGER_GPIO1 5
#define ECHO_GPIO1 18
//pines Ultrasonidos alante
#define TRIGGER_GPIO2 32
#define ECHO_GPIO2 35


#define MAX_DISTANCE_CM 500 // 5m max
//pines motores
//enable
#define ENABLE 33
//alante izquierda
#define AI0 15
#define AI1 2
//alante derecha 
#define AD0 16
#define AD1 4
//atras izquierda
#define BI0 22
#define BI1 23
//atras derecha
#define BD0 19
#define BD1 17


#define EXAMPLE_ESP_WIFI_SSID      "AntenaAngustias"
#define EXAMPLE_ESP_WIFI_PASS      "angustias123"
#define EXAMPLE_ESP_WIFI_CHANNEL   1
#define EXAMPLE_MAX_STA_CONN       4
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

// Página web con botones de dirección
char html_page[] = "<!DOCTYPE HTML><html>\n"
                   "<head>\n"
                   "  <title>Control de Movimiento</title>\n"
                   "  <style>\n"
                   "    body {\n"
                   "      display: flex;\n"
                   "      flex-direction: column;\n"
                   "      align-items: center;\n"
                   "      justify-content: center;\n"
                   "      height: 100vh;\n"
                   "      margin: 0;\n"
                   "    }\n"
                   "    .button-container {\n"
                   "      display: flex;\n"
                   "      flex-direction: row;\n"
                   "      justify-content: space-between;\n"
                   "      width: 100%;\n"
                   "    }\n"
                   "    button {\n"
                   "      margin: 10px;\n"
                   "      width: 120px;\n"
                   "      height: 60px;\n"
                   "      font-size: 18px;\n"
                   "    }\n"
                   "  </style>\n"
                   "</head>\n"
                   "<body>\n"
                   "  <h1>Control de Movimiento</h1>\n"
                   "  <div class=\"button-container\">\n"
                   "    <button onclick=\"sendCommand('front_left')\">Adelante Izq</button>\n"
                   "    <button onclick=\"sendCommand('forward')\">Adelante</button>\n"
                   "    <button onclick=\"sendCommand('front_right')\">Adelante Der</button>\n"
                   "  </div>\n"
                   "  <div class=\"button-container\">\n"
                   "    <button onclick=\"sendCommand('left')\">Izquierda</button>\n"
                   "    <button onclick=\"sendCommand('stop')\">Detener</button>\n"
                   "    <button onclick=\"sendCommand('right')\">Derecha</button>\n"
                   "  </div>\n"
                   "  <div class=\"button-container\">\n"
                   "    <button onclick=\"sendCommand('back_left')\">Atras Izq</button>\n"
                   "    <button onclick=\"sendCommand('back')\">Atras</button>\n"
                   "    <button onclick=\"sendCommand('back_right')\">Atras Der</button>\n"
                   "  </div>\n"
                   "  <script>\n"
                   "    function sendCommand(command) {\n"
                   "      fetch('/control', { method: 'POST', body: `${command}`, headers: { 'Content-Type': 'application/x-www-form-urlencoded' } })\n"
                   "        .then(response => response.text())\n"
                   "        .then(data => console.log(data))\n"
                   "        .catch(error => console.error('Error:', error));\n"
                   "    }\n"
                   "  </script>\n"
                   "</body>\n"
                   "</html>";



static const char *TAG = "wifi softAP";

void forward();
void back();
void left();
void right();
void stop();
void forward_left();
void forward_right();
void back_left();
void back_right();

void ultrasonic_task1(void *pvParameters){//sensor atrás

    ultrasonic_sensor_t sensor = {
        .trigger_pin = TRIGGER_GPIO1,
        .echo_pin = ECHO_GPIO1
    };

    ultrasonic_init(&sensor);

    while (true)
    {
        float distance;
        char buf[100];
        esp_err_t res = ultrasonic_measure(&sensor, MAX_DISTANCE_CM, &distance);

        if (res == ESP_OK)
        {
            printf("Distancia atrás: %0.02f cm\n", distance * 100);
            char dist[10];
            sprintf(dist, "%0.02f cm", distance * 100);

            if(distance<4 && strcmp(buf, "back") == 0)
            {
                stop();
            }
            else
            {
                back();
            }
        }
        else
        {
            printf("Error alante %d: %s\n", res, esp_err_to_name(res));
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void ultrasonic_task2(void *pvParameters){//sensor alante

    ultrasonic_sensor_t sensor = {
        .trigger_pin = TRIGGER_GPIO2,
        .echo_pin = ECHO_GPIO2
    };

    ultrasonic_init(&sensor);

    while (true)
    {
        float distance;
        char buf[100];
        esp_err_t res = ultrasonic_measure (&sensor, MAX_DISTANCE_CM, &distance);

        if (res == ESP_OK)
        {
            printf("Distancia atrás: %0.02f cm\n", distance * 100);
            char dist[10];
            sprintf(dist, "%0.02f cm", distance * 100);
            if(distance<4 && strcmp(buf, "forward") == 0)
            {
                stop();
            }
            else
            {
                forward();
            }
        }
        else
        {
            printf("Error atrás %d: %s\n", res, esp_err_to_name(res));
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}


void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}

esp_err_t send_web_page(httpd_req_t *req)
{
    int response;
    char response_data[sizeof(html_page)];
    memset(response_data, 0, sizeof(response_data));

    // Configura el encabezado Cache-Control para evitar el almacenamiento en caché
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");

    // Llena la respuesta con el contenido HTML
    sprintf(response_data, html_page);

    // Envía la respuesta al cliente
    response = httpd_resp_send(req, response_data, HTTPD_RESP_USE_STRLEN);
    
    return response;
}

esp_err_t post_req_handler(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;

    while (remaining > 0)
    {
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                httpd_resp_send_408(req);
            }
            return ESP_FAIL;
        }
        remaining -= ret;
    }
    buf[req->content_len] = '\0';  // Null-terminate the buffer

    // Imprime el valor del comando en la consola
    ESP_LOGI(TAG, "Received command: %s", buf);

    // Responde con un mensaje simple
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);

    // Realiza acciones según el comando recibido
    if (strcmp(buf, "forward") == 0)
    {
        forward();
    }

    else  if (strcmp(buf, "back") == 0)
    {
        back();
    }

    else if (strcmp(buf, "left") == 0)
    {
        left();
    }

    else if (strcmp(buf, "right") == 0)
    {
        right();
    }

    else if (strcmp(buf, "stop") == 0)
    {
        stop();
    }

    else if (strcmp(buf, "front_left") == 0)
    {
        forward_left();
    }

    else if (strcmp(buf, "front_right") == 0)
    {
        forward_right();
    }

    else if (strcmp(buf, "back_left") == 0)
    {
        back_left();
    }

    else if (strcmp(buf, "back_right") == 0)
    {
        back_right();
    }

    return ESP_OK;
}


httpd_uri_t uri_post = {
    .uri = "/control",
    .method = HTTP_POST,
    .handler = post_req_handler,
    .user_ctx = NULL
};

esp_err_t get_req_handler(httpd_req_t *req)
{
    return send_web_page(req);
}

httpd_uri_t uri_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = get_req_handler,
    .user_ctx = NULL
};

httpd_handle_t setup_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &uri_post);
    }

    return server;
}

//funciones de movimiento básico
void forward()
{
  //primero encender motores si estaban apagados
  gpio_set_level(ENABLE, 1);
  //motores a 1 alante
  gpio_set_level(AI0, 0);
  gpio_set_level(AI1, 1);
  gpio_set_level(AD0, 1);
  gpio_set_level(AD1, 0);
  gpio_set_level(BI0, 0);
  gpio_set_level(BI1, 1);
  gpio_set_level(BD0, 1);
  gpio_set_level(BD1, 0);
}

void back()
{
  //primero encender motores si estaban apagados
  gpio_set_level(ENABLE, 1);
  //motores a 1 atrás
  gpio_set_level(AI0, 1);
  gpio_set_level(AI1, 0);
  gpio_set_level(AD0, 0);
  gpio_set_level(AD1, 1);
  gpio_set_level(BI0, 1);
  gpio_set_level(BI1, 0);
  gpio_set_level(BD0, 0);
  gpio_set_level(BD1, 1);
}

void left()
{
  //primero encender motores si estaban apagados
  gpio_set_level(ENABLE, 18);
  //motores a 1 izquierda (rotación)
  gpio_set_level(AI0, 0);
  gpio_set_level(AI1, 1);
  gpio_set_level(AD0, 0);
  gpio_set_level(AD1, 1);
  gpio_set_level(BI0, 0);
  gpio_set_level(BI1, 1);
  gpio_set_level(BD0, 0);
  gpio_set_level(BD1, 1);
}

void right()
{
  //primero encender motores si estaban apagados
  gpio_set_level(ENABLE, 1);
  //motores a 1 derecha (rotación)
  gpio_set_level(AI0, 1);
  gpio_set_level(AI1, 0);
  gpio_set_level(AD0, 1);
  gpio_set_level(AD1, 0);
  gpio_set_level(BI0, 1);
  gpio_set_level(BI1, 0);
  gpio_set_level(BD0, 1);
  gpio_set_level(BD1, 0);
}

void forward_right()
{
  //primero encender motores si estaban apagados
  gpio_set_level(ENABLE, 1);
  //motores a 1 alante izquiera (pivote)
  gpio_set_level(AI0, 0);
  gpio_set_level(AI1, 0);
  gpio_set_level(AD0, 1);
  gpio_set_level(AD1, 0);
  gpio_set_level(BI0, 0);
  gpio_set_level(BI1, 1);
  gpio_set_level(BD0, 1);
  gpio_set_level(BD1, 0);
}

void back_right()
{
  //primero encender motores si estaban apagados
  gpio_set_level(ENABLE, 1);
  //motores a 1 atrás izquierda (pivote)
  gpio_set_level(AI0, 0);
  gpio_set_level(AI1, 0);
  gpio_set_level(AD0, 0);
  gpio_set_level(AD1, 1);
  gpio_set_level(BI0, 1);
  gpio_set_level(BI1, 0);
  gpio_set_level(BD0, 0);
  gpio_set_level(BD1, 1);
  
}

void forward_left()
{
  //primero encender motores si estaban apagados
  gpio_set_level(ENABLE, 1);
  //motores a 1 alante derecha (pivote)
  gpio_set_level(AI0, 0);
  gpio_set_level(AI1, 1);
  gpio_set_level(AD0, 0);
  gpio_set_level(AD1, 0);
  gpio_set_level(BI0, 0);
  gpio_set_level(BI1, 1);
  gpio_set_level(BD0, 1);
  gpio_set_level(BD1, 0);
}

void back_left()
{
  //primero encender motores si estaban apagados
  gpio_set_level(ENABLE, 1);
  //motores a 1 atrás
  gpio_set_level(AI0, 1);
  gpio_set_level(AI1, 0);
  gpio_set_level(AD0, 0);
  gpio_set_level(AD1, 0);
  gpio_set_level(BI0, 1);
  gpio_set_level(BI1, 0);
  gpio_set_level(BD0, 0);
  gpio_set_level(BD1, 1);
}

void stop()
{
  //apagar los motores sirve
  gpio_set_level(ENABLE, 0);
}

void startup()
{
  stop();
  vTaskDelay(5000/portTICK_PERIOD_MS);
}

// Punto de entrada principal
void app_main(void)
{
    //config pines motores
  esp_rom_gpio_pad_select_gpio(ENABLE);
  esp_rom_gpio_pad_select_gpio(AI0);
  esp_rom_gpio_pad_select_gpio(AI1);
  esp_rom_gpio_pad_select_gpio(AD0);
  esp_rom_gpio_pad_select_gpio(AD1);
  esp_rom_gpio_pad_select_gpio(BI0);
  esp_rom_gpio_pad_select_gpio(BI1);
  esp_rom_gpio_pad_select_gpio(BD0);
  esp_rom_gpio_pad_select_gpio(BD1);
  gpio_set_direction(ENABLE, GPIO_MODE_OUTPUT);
  gpio_set_direction(AI0, GPIO_MODE_OUTPUT);
  gpio_set_direction(AI1, GPIO_MODE_OUTPUT);
  gpio_set_direction(AD0, GPIO_MODE_OUTPUT);
  gpio_set_direction(AD1, GPIO_MODE_OUTPUT);
  gpio_set_direction(BI0, GPIO_MODE_OUTPUT);
  gpio_set_direction(BI1, GPIO_MODE_OUTPUT);
  gpio_set_direction(BD0, GPIO_MODE_OUTPUT);
  gpio_set_direction(BD1, GPIO_MODE_OUTPUT);

  //arranque
  startup();

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());

        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();
    ESP_LOGI(TAG, "Control Web Server is running ... ...\n");
    xTaskCreate(ultrasonic_task1, "ultrasonic_task", configMINIMAL_STACK_SIZE * 6, NULL, 5, NULL);
    xTaskCreate(ultrasonic_task2, "ultrasonic_task", configMINIMAL_STACK_SIZE * 6, NULL, 5, NULL);
    setup_server();

}