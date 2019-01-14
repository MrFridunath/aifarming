#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "cJSON.h"
#include <sys/types.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <arpa/inet.h>
#include "signal.h"
#include "sys/signal.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <pthread.h>
#include "driver/adc.h"
#include "esp_adc_cal.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "rom/ets_sys.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#include "DHT22.h"

#define ESP32_SSID "AIFarming00"
#define ESP32_PASSWORD "aifarming00"

static EventGroupHandle_t s_event_group;

static char *type = "basic_sensors";

static char *master_ip = NULL;

static pthread_t task_t;

static int thread_loop = 0;
static int adc_samples = 10;

//THREAD MONITORIZACION
void *monitoring_task(void *args)
{
	struct sockaddr_in master;
	int master_fd, ret;
	
	uint32_t rain_sensor_reading = 0;
    uint32_t soil_humidity_reading = 0;
    uint32_t light_sensor_reading = 0;

	master.sin_family = AF_INET;
	master.sin_port = htons(40501);
	inet_aton(master_ip, &master.sin_addr);

	char *data = (char*)malloc(sizeof(char) * 1024);

	char *air_temperature = (char*)malloc(sizeof(char) * 32);
	char *air_humidity = (char*)malloc(sizeof(char) * 32);
	char *rain_sensor = (char*)malloc(sizeof(char) * 32);
	char *soil_humidity = (char*)malloc(sizeof(char) * 32);
	char *light_sensor = (char*)malloc(sizeof(char) * 32);

	setDHTgpio( 16 );
	
	adc1_config_width(ADC_WIDTH_BIT_10);
    adc1_config_channel_atten(ADC_CHANNEL_0, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC_CHANNEL_3, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC_CHANNEL_6, ADC_ATTEN_DB_11);

	thread_loop = 1;
	while (thread_loop)
	{
		ret = readDHT();

		if (ret < 0) 
		{
			fprintf(stderr, "error: the dht22 read failed\n");
			sleep(5);
			continue;
		}
			
		else 
		{
			air_temperature[0] = '\0';
			sprintf(air_temperature, "%.1f", getTemperature());
			air_humidity[0] = '\0';
			sprintf(air_humidity, "%.1f", getHumidity());
			fprintf(stdout, "SENSOR H.: %s\n", air_humidity);
			fprintf(stdout, "SENSOR T.: %s\n", air_temperature);
		}
		
		rain_sensor_reading = 0;
        soil_humidity_reading = 0;
        for (int i = 0; i < adc_samples; i++) {
            rain_sensor_reading += adc1_get_raw((adc1_channel_t)ADC_CHANNEL_3);
            soil_humidity_reading += adc1_get_raw((adc1_channel_t)ADC_CHANNEL_0);
            light_sensor_reading += adc1_get_raw((adc1_channel_t)ADC_CHANNEL_6);
        }

		rain_sensor_reading /= adc_samples;
		soil_humidity_reading /= adc_samples;
		light_sensor_reading /= adc_samples;
		
		sprintf(rain_sensor, "%d", rain_sensor_reading);
		sprintf(soil_humidity, "%d", soil_humidity_reading);
		sprintf(light_sensor, "%d", light_sensor_reading);

		if ((master_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		{
			fprintf(stderr, "error: the master socket failed\n");
			abort();
			continue;
		}

		if (connect(master_fd, (struct sockaddr *)&master, sizeof(master)) == -1)
		{
			fprintf(stderr, "error: the connection call on the master failed\n");
			sleep(5);
			continue;
		}
		
		data[0] = '\0';
		strcpy(data, "{\"method\":\"update-system\",\"type\":\"");
		strcat(data, type);
		strcat(data, "\",\"properties\":{");
		strcat(data, "\"air_temperature\":\"");
		strcat(data, air_temperature);
		strcat(data, "\",\"air_humidity\":\"");
		strcat(data, air_humidity);
		strcat(data, "\",\"rain_sensor\":\"");
		strcat(data, rain_sensor);
		strcat(data, "\",\"soil_humidity\":\"");
		strcat(data, soil_humidity);		
		strcat(data, "\",\"light_sensor\":\"");
		strcat(data, light_sensor);
		strcat(data, "\"}}");
		data[strlen(data)] = '\0';
		
		fprintf(stdout, "SLAVE DATA: %s\n", data);

		if (send(master_fd, data, strlen(data) + 1, 0) < 0)
		{
			fprintf(stderr, "error: the send call on the master failed\n");
			sleep(5);
			continue;
		}
		
		close(master_fd);

		sleep(5);
	}
	
	free(data);

	free(air_humidity);
	free(air_temperature);
	free(soil_humidity);
	free(rain_sensor);
	free(light_sensor);

    return NULL;
}

//WIFI EVENTS
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
	
	//INICIO WIFI
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;

	//TENGO IP
    case SYSTEM_EVENT_STA_GOT_IP:
		strcpy(master_ip, ip4addr_ntoa(&event->event_info.got_ip.ip_info.gw));
		if (thread_loop == 0)
		{
			if (pthread_create(&task_t, NULL, &monitoring_task, NULL) != 0) 
			{
				fprintf(stderr, "error: the thread creation failed\n");
			}
		}
		break;

	// DESCONEXION DE RED LOCAL
    case SYSTEM_EVENT_STA_DISCONNECTED:
        {
			if (thread_loop == 1)
			{
				thread_loop = 0;
			}
            esp_wifi_connect();
            break;
        }

    default:
        break;
    }

    return ESP_OK;
}

//INICIO WIFI
void wifi_init()
{
    s_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP32_SSID,
            .password = ESP32_PASSWORD
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
	ESP_ERROR_CHECK(esp_wifi_start() );
}

//MAIN
void app_main() {

	esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);
	
	master_ip = (char*) malloc (sizeof(char) * 32);
	
	wifi_init();

    int server_fd, client_fd;
    struct sockaddr_in server , client;

    server_fd = socket(AF_INET , SOCK_STREAM , 0);
    if (server_fd == -1)
    {
        fprintf(stderr,"error: the server socket failed\n");
		return;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(40502);

    if(bind(server_fd, (struct sockaddr *)&server, sizeof(server)) == -1)
    {
        fprintf(stderr,"error: the bind call with the server file descriptor failed\n");
        return;
    }

    if(listen(server_fd, 3))
    {
	    fprintf(stderr,"error: the listen call with the server file descriptor failed\n");
	    return;
    }

    socklen_t client_length = sizeof(client);
	
    char *data = (char*)malloc(sizeof(char) * 1024);
	
	cJSON *json_data = NULL;
	cJSON *json_method = NULL;

	// SERVER API
    while(1)
    {
		client_fd = accept(server_fd, (struct sockaddr *)&client, &client_length);
		if(client_fd == -1)
		{
			fprintf(stderr, "error: the accept call on the client failed\n");
			close(client_fd);
			continue;
		}
		
		data[0] = '\0';
		if(read(client_fd, data, 1024) == -1)
		{
			fprintf(stderr, "error: the read call on the client failed\n");
			close(client_fd);
			continue;
		}
		
		fprintf(stdout, "DATA: %s\n", data);
		
		json_data = cJSON_Parse(data);		
		if(cJSON_IsInvalid(json_data))
		{
			fprintf(stderr, "error: the data is not in a json format\n");
			close(client_fd);
			continue;
		}

		if(!cJSON_IsObject(json_data))
		{
			fprintf(stderr, "error: the json data is not of type object\n");
			close(client_fd);
			continue;
		}

		json_method = cJSON_GetObjectItemCaseSensitive(json_data, "method");
		if (cJSON_IsInvalid(json_method))
		{
			fprintf(stderr, "error: the json does not have a \"method\" member\n");
			close(client_fd);
			continue;
		}

		if(!cJSON_IsString(json_method))
		{
			fprintf(stderr, "error: the json member \"method\" is not of type string\n");
			close(client_fd);
			continue;
		}

		//INICIALIZAR
		if (strcmp(json_method->valuestring, "get-type") == 0)
		{
			data[0] = '\0';
			strcpy(data, "{\"status\":200,\"type\":\"");
			strcat(data, type);
			strcat(data, "\"}");
			data[strlen(data)] = '\0';

			fprintf(stdout, "RESPONSE: %s\n", data);
				
			if (write(client_fd, data, strlen(data) + 1) == -1)
			{
				fprintf(stderr, "error: the write call on the client failed\n");
			}
		}
		close(client_fd);
    }
    return;
}
