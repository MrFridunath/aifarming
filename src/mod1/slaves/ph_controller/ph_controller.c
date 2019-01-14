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
#include <time.h>

#include "lwip/err.h"
#include "lwip/sys.h"

#include "rom/ets_sys.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#define ESP32_SSID "AIFarming00"
#define ESP32_PASSWORD "aifarming00"

static EventGroupHandle_t s_event_group;

static char *type = "ph_controller";

static char *master_ip = NULL;

static double objective_ph = -1.0;
static double ph_umbral = -1.0;
static double water_ph = -1.0;

static pthread_t task_t;

static int thread_loop = 0;

void *monitoring_task(void *args)
{
	struct sockaddr_in master;
	int master_fd;

	master.sin_family = AF_INET;
	master.sin_port = htons(40501);
	inet_aton(master_ip, &master.sin_addr);

	char *data = (char*)malloc(sizeof(char) * 1024);

	char *water_ph_buffer = (char*)malloc(sizeof(char) * 32);

	thread_loop = 1;
	while (thread_loop)
	{
		//LEER PH
        water_ph_buffer = "7";
        water_ph = 7.0;
		
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
		strcat(data, "\"water_ph\":\"");
		strcat(data, water_ph_buffer);
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

	free(water_ph_buffer);

    return NULL;
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;

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

static int pulse_count = 0;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    
	if (gpio_num == 4)
	{
		fprintf(stdout, "notice: interrupt from GPIO 4\n");
		pulse_count += 1;
	}
}

void app_main() {

	esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);
	
	master_ip = (char*) malloc (sizeof(char) * 32);
	
	wifi_init();

    gpio_config_t io_conf;
	io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = ((1ULL<<16) | (1ULL<<17) | (1ULL<<18));
	io_conf.pull_down_en = 0;
	io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
	
    io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
    io_conf.pin_bit_mask = (1ULL<<4);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
	
	gpio_install_isr_service(0);
	
	gpio_set_level(16, 0);
	gpio_set_level(17, 0);
	gpio_set_level(18, 0);

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
			
			data[0] = '\0';
			if (recv(client_fd, data, 1024, 0) < 0)
			{
				fprintf(stderr, "error: the recv call on the master failed\n");
				close(client_fd);
				continue;
			}

			fprintf(stdout, "RESPONSE: %s\n", data);
					
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

			json_method = cJSON_GetObjectItemCaseSensitive(json_data, "properties");
			if (cJSON_IsInvalid(json_method))
			{
				fprintf(stderr, "error: the json does not have a \"properties\" property\n");
				close(client_fd);
				continue;
			}

			if(!cJSON_IsObject(json_method))
			{
				fprintf(stderr, "error: the json data is not of type object\n");
				close(client_fd);
				continue;
			}
			
			json_data = cJSON_GetObjectItemCaseSensitive(json_method, "objective_ph");
			if (cJSON_IsInvalid(json_data))
			{
				fprintf(stderr, "error: the json does not have a \"objective_ph\" property\n");
			}
			else
			{
				if(!cJSON_IsNumber(json_data))
				{
					fprintf(stderr, "error: the json property \"objective_ph\" is not of type number\n");
				}
				else
				{
					objective_ph = json_data->valuedouble;
				}
			}
			
			json_data = cJSON_GetObjectItemCaseSensitive(json_method, "ph_umbral");
			if (cJSON_IsInvalid(json_data))
			{
				fprintf(stderr, "error: the json does not have a \"ph_umbral\" property\n");
			}
			else
			{
				if(!cJSON_IsNumber(json_data))
				{
					fprintf(stderr, "error: the json property \"ph_umbral\" is not of type number\n");
				}
				else
				{
					ph_umbral = json_data->valuedouble;
				}
			}

		}
		else if (strcmp(json_method->valuestring, "make-action") == 0)
		{
			json_method = cJSON_GetObjectItemCaseSensitive(json_data, "code");
			if (cJSON_IsInvalid(json_method))
			{
				fprintf(stderr, "error: the json does not have a \"code\" property\n");
				close(client_fd);
				continue;
			}

			if(!cJSON_IsNumber(json_method))
			{
				fprintf(stderr, "error: the json property \"code\" is not of type number\n");
				close(client_fd);
				continue;
			}
			
			data[0] = '\0';
			sprintf(data, "{\"status\":200}");
			data[strlen(data)] = '\0';

			fprintf(stdout, "RESPONSE: %s\n", data);
				
			if (write(client_fd, data, strlen(data) + 1) == -1)
			{
				fprintf(stderr, "error: the write call on the client failed\n");
			}
			
			close(client_fd);

			//NEUTRALIZAR
			if (json_method->valueint == 1 || json_method->valueint == 2)
			{
				
			}

			if (json_method->valueint == 0 || json_method->valueint == 2)
			{
				// REGAR
				double flow_rate = 0;
				double flow_millilitres = 0;
				double millilitres = 0;
				
				pulse_count = 0;
				gpio_isr_handler_add(4, gpio_isr_handler, (void*) 4);

				clock_t time = clock();
				gpio_set_level(17, 1);
				while (1)
				{
					if (((clock() - time)/CLOCKS_PER_SEC) > 1.0)
					{
						gpio_isr_handler_remove(4);
						
						flow_rate = (((clock() - time)/CLOCKS_PER_SEC) * pulse_count) / 4.5;

						time = clock();

						flow_millilitres = (flow_rate / 60) * 1000;
							
						millilitres += flow_millilitres;
							
						fprintf(stdout, "notice: %.2f millilitres\n", millilitres);
							
						if (millilitres > 200.0)
						{
							break;
						}
						
						//BORRAR
						else//
						{//
							break;//
						}//
							
						pulse_count = 0;

						gpio_isr_handler_add(4, gpio_isr_handler, (void*) 4);
					}

				}
				gpio_set_level(17, 0);

				struct sockaddr_in master;
				int master_fd;

				master.sin_family = AF_INET;
				master.sin_port = htons(40501);
				inet_aton(master_ip, &master.sin_addr);

				if ((master_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
				{
					fprintf(stderr, "error: the master socket failed\n");
					continue;
				}

				if (connect(master_fd, (struct sockaddr *)&master, sizeof(master)) == -1)
				{
					fprintf(stderr, "error: the connection call on the master failed\n");
					continue;
				}
			
				data[0] = '\0';
				strcpy(data, "{\"method\":\"update-system\",\"type\":\"");
				strcat(data, type);
				strcat(data, "\",\"properties\":{");
				strcat(data, "\"last_water\":true}}");
				data[strlen(data)] = '\0';

				fprintf(stdout, "SLAVE DATA: %s\n", data);

				if (write(master_fd, data, strlen(data) + 1) < 0)
				{
					fprintf(stderr, "error: the send call on the master failed\n");
				}
				close(master_fd);
			}
			continue;
		}
		
		//UPDATE-PH
		else if (strcmp(json_method->valuestring, "update-ph") == 0)
		{
			json_method = cJSON_GetObjectItemCaseSensitive(json_data, "properties");
			if (cJSON_IsInvalid(json_method))
			{
				fprintf(stderr, "error: the json does not have a \"properties\" property\n");
				close(client_fd);
				continue;
			}

			if(!cJSON_IsObject(json_method))
			{
				fprintf(stderr, "error: the json data is not of type object\n");
				close(client_fd);
				continue;
			}
			
			json_data = cJSON_GetObjectItemCaseSensitive(json_method, "objective_ph");
			if (cJSON_IsInvalid(json_data))
			{
				fprintf(stderr, "error: the json does not have a \"objective_ph\" property\n");
			}
			else
			{
				if(!cJSON_IsNumber(json_data))
				{
					fprintf(stderr, "error: the json property \"objective_ph\" is not of type number\n");
				}
				else
				{
					objective_ph = json_data->valuedouble;
				}
			}
			
			json_data = cJSON_GetObjectItemCaseSensitive(json_method, "ph_umbral");
			if (cJSON_IsInvalid(json_data))
			{
				fprintf(stderr, "error: the json does not have a \"ph_umbral\" property\n");
			}
			else
			{
				if(!cJSON_IsNumber(json_data))
				{
					fprintf(stderr, "error: the json property \"ph_umbral\" is not of type number\n");
				}
				else
				{
					ph_umbral = json_data->valuedouble;
				}
			}

			data[0] = '\0';
			sprintf(data, "{\"status\":200}");
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
