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
#include <pthread.h>
#include <time.h>

#include "lwip/err.h"
#include "lwip/sys.h"

//#define WIFI_SSID "iPhone"
//#define WIFI_PASSWORD "yu7a2vvuw45fca"
//#define WIFI_SSID "devolo-30d32d583a87"
//#define WIFI_PASSWORD "LNHIFWKWNBEWPHXF"
#define WIFI_SSID "AIFarmingMiddleware"
#define WIFI_PASSWORD "aifarmingmiddleware"

#define ESP32_SSID "AIFarming00"
#define ESP32_PASSWORD "aifarming00"

static EventGroupHandle_t s_event_group;

static const char *TAG = "AI Farming Access Point";

static char *slave_basic_sensors_ip = NULL;
static char *slave_basic_sensors_mac = NULL;
static char *slave_ph_controller_ip = NULL;
static char *slave_ph_controller_mac = NULL;

static char *buffer = NULL;
static char *mac = NULL;

static cJSON *json_host_data = NULL;
static cJSON *json_host_type = NULL;
static cJSON *json_host_status = NULL;

static clock_t start_t = 0;

static nvs_handle nvs;
static esp_err_t err;

struct system_properties {
    double light_sensor;
    double air_temperature;
    double air_humidity;
    double soil_humidity;
    double rain_sensor;
    double water_ph;
    double objective_ph;
	double ph_umbral;
	int32_t last_water;
};
static struct system_properties properties;

static pthread_t task_t;
static int thread_loop = 0;
void *monitoring_task(void *args)
{
	struct sockaddr_in slave_ph_controller;
	int slave_ph_controller_fd;

	slave_ph_controller.sin_family = AF_INET;
	slave_ph_controller.sin_port = htons(40502);
	inet_aton(slave_ph_controller_ip, &slave_ph_controller.sin_addr);

	char *data = (char*)malloc(sizeof(char) * 1024);
    
	int failures = 0;

	thread_loop = 1;
	while (thread_loop)
	{
		if (properties.last_water != -1)
		{
			clock_t end_t = clock();
			clock_t total_t = (double)(end_t - start_t) / CLOCKS_PER_SEC;
			properties.last_water = properties.last_water + total_t;

			err = nvs_open("storage", NVS_READWRITE, &nvs);
			if (err == ESP_OK)
			{
				nvs_set_i32(nvs, "last_water", properties.last_water);
				nvs_commit(nvs);
			}
			nvs_close(nvs);

			start_t = clock();

			if (properties.last_water < 18 * 60 * 60)
			{
				sleep(18 * 60 * 60);
				
				properties.last_water = properties.last_water + (18 * 60 * 60);

				err = nvs_open("storage", NVS_READWRITE, &nvs);
				if (err == ESP_OK)
				{
					nvs_set_i32(nvs, "last_water", properties.last_water);
					nvs_commit(nvs);
				}
				nvs_close(nvs);

				start_t = clock();
				continue;
			}
		}
		if ((properties.rain_sensor != -1) && (properties.soil_humidity != -1))
		{
			if (properties.rain_sensor < 700 && properties.soil_humidity < 800)
			{
				sleep(10 * 60 * 60);
				
				properties.last_water = properties.last_water + (10 * 60 * 60);

				err = nvs_open("storage", NVS_READWRITE, &nvs);
				if (err == ESP_OK)
				{
					nvs_set_i32(nvs, "last_water", properties.last_water);
					nvs_commit(nvs);
				}
				nvs_close(nvs);
				
				start_t = clock();

				continue;
			}
			else if (properties.soil_humidity < 600)
			{
				sleep(12 * 60 * 60);
				
				properties.last_water = properties.last_water + (12 * 60 * 60);

				err = nvs_open("storage", NVS_READWRITE, &nvs);
				if (err == ESP_OK)
				{
					nvs_set_i32(nvs, "last_water", properties.last_water);
					nvs_commit(nvs);
				}
				nvs_close(nvs);
				
				start_t = clock();

				continue;
			}
		}
		else
		{
			sleep(10);
			continue;
		}
		if (properties.water_ph != -1)
		{
			if (properties.water_ph != properties.objective_ph)
			{
				int positive_umbral = properties.objective_ph + properties.ph_umbral;
				int negative_umbral = properties.objective_ph - properties.ph_umbral;
				if ((properties.water_ph > properties.objective_ph) && (properties.water_ph < positive_umbral))
				{
					data[0] = '\0';
					strcpy(data, "{\"method\":\"make-action\",\"code\":");
					strcat(data, "2}");
					data[strlen(data)] = '\0';
				}
				else if ((properties.water_ph < properties.objective_ph) && (properties.water_ph > negative_umbral))
				{
					data[0] = '\0';
					strcpy(data, "{\"method\":\"make-action\",\"code\":");
					strcat(data, "2}");
					data[strlen(data)] = '\0';
				}
				else 
				{
					data[0] = '\0';
					strcpy(data, "{\"method\":\"make-action\",\"code\":");
					strcat(data, "0}");
					data[strlen(data)] = '\0';
				}
			}
			else 
			{
				data[0] = '\0';
				strcpy(data, "{\"method\":\"make-action\",\"code\":");
				strcat(data, "2}");
				data[strlen(data)] = '\0';
			}
		}
		else
		{
			sleep(10);
			continue;
		}

		fprintf(stdout, "MASTER DATA: %s\n", data);

		if ((slave_ph_controller_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		{
			fprintf(stderr, "error: the slave socket failed\n");
			inet_aton(slave_ph_controller_ip, &slave_ph_controller.sin_addr);
			failures += 1;
			if (failures > 100)
			{
				clock_t end_t = clock();
				clock_t total_t = (double)(end_t - start_t) / CLOCKS_PER_SEC;
				properties.last_water = properties.last_water + total_t;

				err = nvs_open("storage", NVS_READWRITE, &nvs);
				if (err == ESP_OK)
				{
					nvs_set_i32(nvs, "last_water", properties.last_water);
					nvs_commit(nvs);
				}
				nvs_close(nvs);
				
				start_t = clock();

				abort();
			}
			continue;
		}

		if (connect(slave_ph_controller_fd, (struct sockaddr *)&slave_ph_controller, sizeof(slave_ph_controller)) == -1)
		{
			fprintf(stderr, "error: the connection call on the slave failed\n");
			sleep(5);
			continue;
		}

		if (send(slave_ph_controller_fd, data, strlen(data) + 1, 0) < 0)
		{
			fprintf(stderr, "error: the send call on the slave failed\n");
			sleep(5);
			continue;
		}
		
		close(slave_ph_controller_fd);
		
		sleep(54 * 60 * 60);

		properties.last_water = properties.last_water + (54 * 60 * 60);

		err = nvs_open("storage", NVS_READWRITE, &nvs);
		if (err == ESP_OK)
		{
			nvs_set_i32(nvs, "last_water", properties.last_water);
			nvs_commit(nvs);
		}
		nvs_close(nvs);
				
		start_t = clock();
	}
	
	free(data);

    return NULL;
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
		printf("Connected to remote access point\n");
        esp_wifi_connect();
        break;

    case SYSTEM_EVENT_STA_GOT_IP:
		printf("Got ip from remote access point\n");
        break;

    case SYSTEM_EVENT_STA_DISCONNECTED:
		printf("Disconnected from remote access point\n");

		clock_t end_t = clock();
		clock_t total_t = (double)(end_t - start_t) / CLOCKS_PER_SEC;
		properties.last_water = properties.last_water + total_t;

		err = nvs_open("storage", NVS_READWRITE, &nvs);
		if (err == ESP_OK)
		{
			nvs_set_i32(nvs, "last_water", properties.last_water);
			nvs_commit(nvs);
		}
		nvs_close(nvs);
				
		start_t = clock();

		abort();
		break;
    case SYSTEM_EVENT_AP_STACONNECTED:
		sleep(3);
	
		sprintf(mac, MACSTR, MAC2STR(event->event_info.sta_connected.mac));

        printf("Station connected to access point. MAC: %s\n", mac);

  	    wifi_sta_list_t wifi_sta_list;
	    tcpip_adapter_sta_list_t adapter_sta_list;

	    memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
	    memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));

        ESP_ERROR_CHECK(esp_wifi_ap_get_sta_list(&wifi_sta_list));	
	    ESP_ERROR_CHECK(tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list));

		char *mac_temp = (char*)malloc(sizeof(char)*32);
		
		fprintf(stdout, "CONECTADOS: %d\n", adapter_sta_list.num);

        for(int i = 0; i < adapter_sta_list.num; i++) {

 		    tcpip_adapter_sta_info_t station = adapter_sta_list.sta[i];

			sprintf(mac_temp, MACSTR, MAC2STR(station.mac));

			if (strcmp(mac_temp, mac) == 0)
			{
				struct sockaddr_in slave;
				int slave_fd;

				slave.sin_family = AF_INET;
				slave.sin_port = htons(40502);

				char *ip = (char*)malloc(sizeof(char) * 32);
				sprintf(ip, "%s", ip4addr_ntoa(&(station.ip)));
				inet_aton(ip, &slave.sin_addr);

				fprintf(stdout, "SLAVE HOST: %s\n", inet_ntoa(slave.sin_addr));

				if ((slave_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
				{
					fprintf(stderr, "error: a slave socket failed\n");
					break;
				}

				if(connect(slave_fd, (struct sockaddr *)&slave, sizeof(slave)) < 0)
				{
					fprintf(stderr, "error: the connection on a possible slave failed\n");
					close(slave_fd);
					break;
				}

				buffer[0] = '\0';
				strcpy(buffer, "{\"method\":\"get-type\"}");
				buffer[strlen(buffer)] = '\0';
				
				if (send(slave_fd, buffer, strlen(buffer) + 1, 0) < 0)
				{
					fprintf(stderr, "error: the send call on the slave failed\n");
					close(slave_fd);
					break;
				}

				buffer[0] = '\0';
			    if (recv(slave_fd, buffer, 1024, 0) < 0)
				{
					fprintf(stderr, "error: the recv call on the slave failed\n");
					close(slave_fd);
					break;
				}

				fprintf(stdout, "RESPONSE: %s\n", buffer);
				
				json_host_data = cJSON_Parse(buffer);
				if(cJSON_IsInvalid(json_host_data))
				{
					fprintf(stderr, "error: the data is not in a json format\n");
					close(slave_fd);
					break;
				}
				
				if(!cJSON_IsObject(json_host_data))
				{
					fprintf(stderr, "error: the json data is not of the type object\n");
					close(slave_fd);
					break;
				}
					
				json_host_status = cJSON_GetObjectItemCaseSensitive(json_host_data, "status");
				if (cJSON_IsInvalid(json_host_status))
				{
					fprintf(stderr, "error: the json does not have a \"status\" property\n");
					close(slave_fd);
					break;
				}
					
				if (!cJSON_IsNumber(json_host_status))
				{
					fprintf(stderr, "error: the json property \"status\" is not of type number\n");
					close(slave_fd);
					break;
				}
					
				if (json_host_status->valueint == 200)
				{
					json_host_type = cJSON_GetObjectItemCaseSensitive(json_host_data, "type");
					if (cJSON_IsInvalid(json_host_type))
					{
						fprintf(stderr, "error: the json does not have a \"type\" property\n");
						close(slave_fd);
						break;
					}

					if (!cJSON_IsString(json_host_type))
					{
						fprintf(stderr, "error: the json property \"type\" is not of type string\n");
						close(slave_fd);
						break;
					}
					
					if (strcmp(json_host_type->valuestring, "basic_sensors") == 0)
					{
						strcpy(slave_basic_sensors_ip, inet_ntoa(slave.sin_addr));
						strcpy(slave_basic_sensors_mac, mac);
						fprintf(stdout, "SLAVE BASIC SENSORS CONF: ip -> %s; mac -> %s\n", slave_basic_sensors_ip, slave_basic_sensors_mac);
					}					
					else if (strcmp(json_host_type->valuestring, "ph_controller") == 0)
					{
						strcpy(slave_ph_controller_ip, inet_ntoa(slave.sin_addr));
						strcpy(slave_ph_controller_mac, mac);
						fprintf(stdout, "SLAVE PH CONTROLLER CONF: ip -> %s; mac -> %s\n", slave_ph_controller_ip, slave_ph_controller_mac);

						char objective_ph_buffer[32];
						char ph_umbral_buffer[32];

						sprintf(ph_umbral_buffer, "%.2f", properties.ph_umbral);
						sprintf(objective_ph_buffer, "%.2f", properties.objective_ph);

						buffer[0] = '\0';
						strcpy(buffer, "{\"properties\":{\"objective_ph\":");
						strcat(buffer, objective_ph_buffer);
						strcat(buffer, ",\"ph_umbral\":");
						strcat(buffer, ph_umbral_buffer);
						strcat(buffer, "}}");
						buffer[strlen(buffer)] = '\0';
						
						if (send(slave_fd, buffer, strlen(buffer) + 1, 0) < 0)
						{
							fprintf(stderr, "error: the send call on the slave failed\n");
							close(slave_fd);
							break;
						}
					}
			
					fprintf(stdout, "STATIONS: %s;%s;%s;%s\n", slave_basic_sensors_ip, slave_basic_sensors_mac, slave_ph_controller_ip, slave_ph_controller_mac);
					
					if ((strcmp(slave_basic_sensors_ip, "") != 0) && (strcmp(slave_ph_controller_ip, "") != 0))
					{
						if (thread_loop == 0)
						{
							fprintf(stdout, "Thread creado\n");
							if (pthread_create(&task_t, NULL, &monitoring_task, NULL) != 0)
							{
								fprintf(stderr, "error: the thread creation failed\n");
							}
						}
					}

					close(slave_fd);
				}
				break;
			}
      	}
		free(mac_temp);
        break;

    case SYSTEM_EVENT_AP_STADISCONNECTED:

        printf("Station disconnected from access point. MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", MAC2STR(event->event_info.sta_disconnected.mac));

		char *dis_mac_temp = (char*)malloc(sizeof(char) * 32);
		
		sprintf(dis_mac_temp, MACSTR, MAC2STR(event->event_info.sta_disconnected.mac));

		if (strcmp(slave_basic_sensors_mac, dis_mac_temp) == 0)
		{
			printf("Station deleted\n");
			slave_basic_sensors_ip[0] = '\0';
			slave_basic_sensors_mac[0] = '\0';
			if (thread_loop == 1)
			{
				thread_loop = 0;
			}
		}
		else if (strcmp(slave_ph_controller_mac, dis_mac_temp) == 0)
		{
			printf("Station deleted\n");
			slave_ph_controller_ip[0] = '\0';
			slave_ph_controller_mac[0] = '\0';
			if (thread_loop == 1)
			{
				thread_loop = 0;
			}
		}
		else 
		{
			printf("Station not found\n");
		}

		free(dis_mac_temp);
		
        break;

    default:
        break;
    }

    return ESP_OK;
}

static void start_dhcp_server()
{
  	// initialize the tcp stack
    tcpip_adapter_init();
    // stop DHCP server
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
    // assign a static IP to the network interface
    tcpip_adapter_ip_info_t info;
    memset(&info, 0, sizeof(info));
    IP4_ADDR(&info.ip, 192, 168, 1, 1);
    IP4_ADDR(&info.gw, 192, 168, 1, 1);//ESP acts as router, so gw addr will be its own addr
    IP4_ADDR(&info.netmask, 255, 255, 255, 0);
	
    ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));
    // start the DHCP server   
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));

    printf("DHCP server started \n");
}

void wifi_init()
{
    s_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t sta_wifi_config = {
        .sta = {
            .ssid = WIFI_SSID//,
            //.password = WIFI_PASSWORD
        }
    };

    wifi_config_t ap_wifi_config = {
        .ap = {
            .ssid = ESP32_SSID,
            .ssid_len = strlen(ESP32_SSID),
            .password = ESP32_PASSWORD,
            .max_connection = 15,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
	if (strlen(ESP32_PASSWORD) == 0) {
        ap_wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_wifi_config) );
	ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished.SSID:%s password:%s", ESP32_SSID, ESP32_PASSWORD);
}

void app_main()
{
	err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
	}
	ESP_ERROR_CHECK(nvs_flash_init());

	err = nvs_open("storage", NVS_READWRITE, &nvs);
	if (err != ESP_OK) 
	{
        fprintf(stderr,"error: the nvs open call failed\n");
		return;
	}
	else {
		size_t required_size;
		err = nvs_get_str(nvs, "air_temperature", NULL, &required_size);
		char *value = (char*)malloc(required_size);
		if (err == ESP_OK)
		{
			nvs_get_str(nvs, "air_temperature", value, &required_size);
			properties.air_temperature = atof(value);
		}
		else
		{
			properties.air_temperature = -1.0;
		}

		err = nvs_get_str(nvs, "air_humidity", NULL, &required_size);
		if (err == ESP_OK)
		{
			value[0] = '\0';
			nvs_get_str(nvs, "air_humidity", value, &required_size);
			properties.air_humidity = atof(value);
		}
		else 
		{
			properties.air_humidity = -1.0;
		}

		err = nvs_get_str(nvs, "soil_humidity", NULL, &required_size);
		if (err == ESP_OK)
		{
			value[0] = '\0';
			nvs_get_str(nvs, "soil_humidity", value, &required_size);
			properties.soil_humidity = atof(value);
		}
		else 
		{
			properties.soil_humidity = -1.0;
		}

		err = nvs_get_str(nvs, "rain_sensor", NULL, &required_size);
		if (err == ESP_OK)
		{
			value[0] = '\0';
			nvs_get_str(nvs, "rain_sensor", value, &required_size);
			properties.rain_sensor = atof(value);
		}
		else 
		{
			properties.rain_sensor = -1.0;
		}
		
		err = nvs_get_str(nvs, "water_ph", NULL, &required_size);
		if (err == ESP_OK)
		{
			value[0] = '\0';
			nvs_get_str(nvs, "water_ph", value, &required_size);
			properties.water_ph = atof(value);
		}
		else 
		{
			properties.water_ph = -1.0;
		}
		
		err = nvs_get_str(nvs, "objective_ph", NULL, &required_size);
		if (err == ESP_OK)
		{
			value[0] = '\0';
			nvs_get_str(nvs, "objective_ph", value, &required_size);
			properties.objective_ph = atof(value);
		}
		else 
		{
			properties.objective_ph = 7.0;
		}
		
		err = nvs_get_str(nvs, "ph_umbral", NULL, &required_size);
		if (err == ESP_OK)
		{
			value[0] = '\0';
			nvs_get_str(nvs, "ph_umbral", value, &required_size);
			properties.ph_umbral = atof(value);
		}
		else 
		{
			properties.ph_umbral = 0.2;
		}
		
		err = nvs_get_str(nvs, "light_sensor", NULL, &required_size);
		if (err == ESP_OK)
		{
			value[0] = '\0';
			nvs_get_str(nvs, "light_sensor", value, &required_size);
			properties.light_sensor = atof(value);
		}
		else 
		{
			properties.light_sensor = -1.0;
		}

		err = nvs_get_i32(nvs, "last_water", &properties.last_water);
		if (err != ESP_OK)
		{
			properties.last_water = -1;
		}

		free(value);
	}
	nvs_close(nvs);

	start_t = clock();

	buffer = (char*)malloc(sizeof(char) * 1024);
	mac = (char*)malloc(sizeof(char) * 32);

    slave_basic_sensors_mac = (char*)malloc(sizeof(char) * 32);
    slave_basic_sensors_ip = (char*)malloc(sizeof(char) * 32);
    slave_ph_controller_mac = (char*)malloc(sizeof(char) * 32);
    slave_ph_controller_ip = (char*)malloc(sizeof(char) * 32);
	
	slave_basic_sensors_mac[0] = '\0';
	slave_basic_sensors_ip[0] = '\0';
	slave_ph_controller_mac[0] = '\0';
	slave_ph_controller_ip[0] = '\0';

	start_dhcp_server();
	wifi_init();

	int server_fd, client_fd;
    struct sockaddr_in server, client;

    server_fd = socket(AF_INET , SOCK_STREAM , 0);
    if (server_fd == -1)
    {
        fprintf(stderr,"error: the server socket failed\n");
		return;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(40501);

    if(bind(server_fd, (struct sockaddr *)&server, sizeof(server)) == -1)
    {
        fprintf(stderr,"error: the bind call with the server file descriptor failed\n");
        return;
    }

    if(listen(server_fd, 10))
    {
	    fprintf(stderr,"error: the listen call with the server file descriptor failed\n");
	    return;
    }

    socklen_t client_length = sizeof(client);
	
	char *data = (char*)malloc(sizeof(char) * 1024);

	cJSON *json_data = NULL;
	cJSON *json_method = NULL;
	
	cJSON *json_slave_data = NULL;
	cJSON *json_slave_type = NULL;

	while (1)
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
			fprintf(stderr, "error: the json does not have a \"method\" property\n");
			close(client_fd);
			continue;
		}

		if(!cJSON_IsString(json_method))
		{
			fprintf(stderr, "error: the json property \"method\" is not of type string\n");
			close(client_fd);
			continue;
		}

		//UPDATE-SYSTEM
		if (strcmp(json_method->valuestring, "update-system") == 0)
		{
			json_slave_type = cJSON_GetObjectItemCaseSensitive(json_data, "type");
			if (cJSON_IsInvalid(json_slave_type))
			{
				fprintf(stderr, "error: the json does not have a \"type\" property\n");
				close(client_fd);
				continue;
			}

			if(!cJSON_IsString(json_slave_type))
			{
				fprintf(stderr, "error: the json property \"type\" is not of type string\n");
				close(client_fd);
				continue;
			}
				
			json_slave_data = cJSON_GetObjectItemCaseSensitive(json_data, "properties");
			if (cJSON_IsInvalid(json_slave_data))
			{
				fprintf(stderr, "error: the json does not have a \"properties\" property\n");
				close(client_fd);
				continue;
			}

			if(!cJSON_IsObject(json_slave_data))
			{
				fprintf(stderr, "error: the json data is not of type object\n");
				close(client_fd);
				continue;
			}

			if (strcmp(json_slave_type->valuestring, "basic_sensors") == 0)
			{
				err = nvs_open("storage", NVS_READWRITE, &nvs);
				
				json_slave_type = cJSON_GetObjectItemCaseSensitive(json_slave_data, "air_temperature");
				if (cJSON_IsInvalid(json_slave_type))
				{
					fprintf(stderr, "error: the json does not have a \"air_temperature\" property\n");
				}
				else
				{
					if(!cJSON_IsString(json_slave_type))
					{
						fprintf(stderr, "error: the json property \"air_temperature\" is not of type number\n");
					}
					else 
					{
						properties.air_temperature = atof(json_slave_type -> valuestring);
						if (err == ESP_OK)
						{
							char c[32];
							sprintf(c, "%f", properties.air_temperature);
							nvs_set_str(nvs, "air_temperature", c);
						}
					}
				}

				json_slave_type = cJSON_GetObjectItemCaseSensitive(json_slave_data, "air_humidity");
				if (cJSON_IsInvalid(json_slave_type))
				{
					fprintf(stderr, "error: the json does not have a \"air_humidity\" property\n");
				}
				else
				{
					if(!cJSON_IsString(json_slave_type))
					{
						fprintf(stderr, "error: the json property \"air_humidity\" is not of type number\n");
					}
					else 
					{
						properties.air_humidity = atof(json_slave_type -> valuestring);
						if (err == ESP_OK)
						{
							char c[32];
							sprintf(c, "%f", properties.air_humidity);
							nvs_set_str(nvs, "air_humidity", c);
						}
					}
				}

				json_slave_type = cJSON_GetObjectItemCaseSensitive(json_slave_data, "rain_sensor");
				if (cJSON_IsInvalid(json_slave_type))
				{
					fprintf(stderr, "error: the json does not have a \"rain_sensor\" property\n");
				}
				else
				{
					if(!cJSON_IsString(json_slave_type))
					{
						fprintf(stderr, "error: the json property \"rain_sensor\" is not of type number\n");
					}
					else 
					{
						properties.rain_sensor = atof(json_slave_type -> valuestring);
						if (err == ESP_OK)
						{
							char c[32];
							sprintf(c, "%f", properties.rain_sensor);
							nvs_set_str(nvs, "rain_sensor", c);
						}

					}
				}

				json_slave_type = cJSON_GetObjectItemCaseSensitive(json_slave_data, "soil_humidity");
				if (cJSON_IsInvalid(json_slave_type))
				{
					fprintf(stderr, "error: the json does not have a \"soil_humidity\" property\n");
				}
				else
				{
					if(!cJSON_IsString(json_slave_type))
					{
						fprintf(stderr, "error: the json property \"soil_humidity\" is not of type number\n");
					}
					else 
					{
						properties.soil_humidity = atof(json_slave_type -> valuestring);
						if (err == ESP_OK)
						{
							char c[32];
							sprintf(c, "%f", properties.soil_humidity);
							nvs_set_str(nvs, "soil_humidity", c);
						}
					}
				}

				json_slave_type = cJSON_GetObjectItemCaseSensitive(json_slave_data, "light_sensor");
				if (cJSON_IsInvalid(json_slave_type))
				{
					fprintf(stderr, "error: the json does not have a \"light_sensor\" property\n");
				}
				else
				{
					if(!cJSON_IsString(json_slave_type))
					{
						fprintf(stderr, "error: the json property \"light_sensor\" is not of type number\n");
					}
					else 
					{
						properties.light_sensor = atof(json_slave_type -> valuestring);
						if (err == ESP_OK)
						{
							char c[32];
							sprintf(c, "%f", properties.light_sensor);
							nvs_set_str(nvs, "light_sensor", c);
						}
					}
				}

				nvs_commit(nvs);
				nvs_close(nvs);
			}
			else if (strcmp(json_slave_type->valuestring, "ph_controller") == 0)
			{
				err = nvs_open("storage", NVS_READWRITE, &nvs);

				json_slave_type = cJSON_GetObjectItemCaseSensitive(json_slave_data, "water_ph");
				if (cJSON_IsInvalid(json_slave_type))
				{
					fprintf(stderr, "error: the json does not have a \"water_ph\" property\n");
				}
				else
				{
					if(!cJSON_IsString(json_slave_type))
					{
						fprintf(stderr, "error: the json property \"water_ph\" is not of type number\n");
					}
					else 
					{
						properties.water_ph = atof(json_slave_type -> valuestring);
						if (err == ESP_OK)
						{
							char c[32];
							sprintf(c, "%f", properties.water_ph);
							nvs_set_str(nvs, "water_ph", c);
						}
					}
				}

				json_slave_type = cJSON_GetObjectItemCaseSensitive(json_slave_data, "last_water");
				if (cJSON_IsInvalid(json_slave_type))
				{
					fprintf(stderr, "error: the json does not have a \"last_water\" property\n");
				}
				else
				{
					if(!cJSON_IsTrue(json_slave_type))
					{
						fprintf(stderr, "error: the json property \"last_water\" is not of type boolean\n");
					}
					else
					{
						nvs_set_i32(nvs, "last_water", 0);
						properties.last_water = 0;
						
						start_t = clock();
					}
				}

				nvs_commit(nvs);
				nvs_close(nvs);
			}
		}
		
		else if (strcmp(json_method->valuestring, "get-system") == 0) 
		{
			printf("METHOD: get-system\n");

			if (properties.last_water != -1)
			{
				clock_t end = clock();
				double time = (double)(end - start_t)/CLOCKS_PER_SEC;
			
				properties.last_water = properties.last_water + time;

				err = nvs_open("storage", NVS_READWRITE, &nvs);
				if (err == ESP_OK)
				{
					nvs_set_i32(nvs, "last_water", properties.last_water);
					nvs_commit(nvs);
				}
				nvs_close(nvs);
				
				start_t = clock();
				
				data[0] = '\0';
				sprintf(data, "{\"status\":200,\"data\":{\"air_temperature\":\"%.1f\",\"air_humidity\":\"%.1f\",\"soil_humidity\":\"%.1f\",\"rain_sensor\":\"%.1f\",\"water_ph\":\"%.1f\",\"light_sensor\":\"%.1f\",\"last_water\":\"%d\"}}", properties.air_temperature, properties.air_humidity, properties.soil_humidity, properties.rain_sensor, properties.water_ph, properties.light_sensor, properties.last_water);
				data[strlen(data)] = '\0';
			}
			else
			{
				clock_t end = clock();
				double time = (double)(end - start_t)/CLOCKS_PER_SEC;

				data[0] = '\0';
				sprintf(data, "{\"status\":200,\"data\":{\"air_temperature\":\"%.1f\",\"air_humidity\":\"%.1f\",\"soil_humidity\":\"%.1f\",\"rain_sensor\":\"%.1f\",\"water_ph\":\"%.1f\",\"light_sensor\":\"%.1f\",\"last_water\":\"%.1f\"}}", properties.air_temperature, properties.air_humidity, properties.soil_humidity, properties.rain_sensor, properties.water_ph, properties.light_sensor, time);
				data[strlen(data)] = '\0';
			}

			if (write(client_fd, data, strlen(data) + 1) == -1)
			{
				fprintf(stderr, "error: the write call on the client failed\n");
			}
		}
		
		else if (strcmp(json_method->valuestring, "edit-system") == 0) 
		{
			printf("METHOD: edit-system\n");

			json_slave_data = cJSON_GetObjectItemCaseSensitive(json_data, "properties");
			if (cJSON_IsInvalid(json_slave_data))
			{
				fprintf(stderr, "error: the json does not have a \"properties\" property\n");
				close(client_fd);
				continue;
			}

			if(!cJSON_IsObject(json_slave_data))
			{
				fprintf(stderr, "error: the json data is not of type object\n");
				close(client_fd);
				continue;
			}
			else
			{

				err = nvs_open("storage", NVS_READWRITE, &nvs);
				
				json_slave_type = cJSON_GetObjectItemCaseSensitive(json_slave_data, "objective_ph");
				if (cJSON_IsInvalid(json_slave_type))
				{
					fprintf(stderr, "error: the json does not have a \"objective_ph\" property\n");
				}
				else
				{
					if(!cJSON_IsString(json_slave_type))
					{
						fprintf(stderr, "error: the json property \"objective_ph\" is not of type string\n");
					}
					else 
					{
						properties.objective_ph = atof(json_slave_type -> valuestring);

						char c[32];
						sprintf(c, "%.2f", properties.objective_ph);
						nvs_set_str(nvs, "objective_ph", c);
					}
				}
				
				json_slave_type = cJSON_GetObjectItemCaseSensitive(json_slave_data, "ph_umbral");
				if (cJSON_IsInvalid(json_slave_type))
				{
					fprintf(stderr, "error: the json does not have a \"ph_umbral\" property\n");
				}
				else
				{
					if(!cJSON_IsString(json_slave_type))
					{
						fprintf(stderr, "error: the json property \"ph_umbral\" is not of type string\n");
					}
					else 
					{
						properties.ph_umbral = atof(json_slave_type -> valuestring);

						char c[32];
						sprintf(c, "%.2f", properties.ph_umbral);
						nvs_set_str(nvs, "ph_umbral", c);
					}
				}

				data[0] = '\0';
				sprintf(data, "{\"method\":\"update-ph\",\"properties\":{\"objective_ph\":%.2f,\"ph_umbral\":%.2f}}", properties.objective_ph, properties.ph_umbral);
				data[strlen(data)] = '\0';
				
				struct sockaddr_in slave;
				int slave_fd;

				slave.sin_family = AF_INET;
				slave.sin_port = htons(40502);
				inet_aton(slave_ph_controller_ip, &slave.sin_addr);

				fprintf(stdout, "SLAVE HOST: %s\n", inet_ntoa(slave.sin_addr));

				if ((slave_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
				{
					fprintf(stderr, "error: a slave socket failed\n");
					close(client_fd);
					continue;
				}

				if(connect(slave_fd, (struct sockaddr *)&slave, sizeof(slave)) < 0)
				{
					fprintf(stderr, "error: the connection on a possible slave failed\n");
					close(slave_fd);
					close(client_fd);
					continue;
				}

				
				if (send(slave_fd, data, strlen(data) + 1, 0) < 0)
				{
					fprintf(stderr, "error: the send call on the slave failed\n");
					close(slave_fd);
					close(client_fd);
					continue;
				}

				data[0] = '\0';
			    if (recv(slave_fd, data, 1024, 0) < 0)
				{
					fprintf(stderr, "error: the recv call on the slave failed\n");
					close(slave_fd);
					close(client_fd);
					continue;
				}
				
				close(slave_fd);

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

				json_method = cJSON_GetObjectItemCaseSensitive(json_data, "status");
				if (cJSON_IsInvalid(json_method))
				{
					fprintf(stderr, "error: the json does not have a \"status\" property\n");
					close(client_fd);
					continue;
				}

				if(!cJSON_IsNumber(json_method))
				{
					fprintf(stderr, "error: the json property \"status\" is not of type number\n");
					close(client_fd);
					continue;
				}

				int status = json_method->valueint;
				if (status == 200)
				{
					nvs_commit(nvs);
					nvs_close(nvs);
					
					data[0] = '\0';
					strcpy(data, "{\"status\":200}");
					data[strlen(data)]= '\0';
					
					if (write(client_fd, data, strlen(data) + 1) == -1)
					{
						fprintf(stderr, "error: the write call on the client failed\n");
					}
				}
				else
				{
					nvs_close(nvs);
				}
			}
		}

		//SEND-ACTION
		else if (strcmp(json_method->valuestring, "send-action") == 0)
		{
			printf("METHOD: send-action\n");

			json_slave_data = cJSON_GetObjectItemCaseSensitive(json_data, "code");
			if (cJSON_IsInvalid(json_slave_data))
			{
				fprintf(stderr, "error: the json does not have a \"code\" property\n");
				close(client_fd);
				continue;
			}

			if(!cJSON_IsNumber(json_slave_data))
			{
				fprintf(stderr, "error: the json property \"code\" is not of type number\n");
				close(client_fd);
				continue;
			}
			
			int code = json_slave_data->valueint;
			
			if ((code != 1) && (code != 2))
			{
				fprintf(stderr, "error: the json property \"code\" is not a valid number\n");
				close(client_fd);
				continue;
			}
							
			struct sockaddr_in slave;
			int slave_fd;

			slave.sin_family = AF_INET;
			slave.sin_port = htons(40502);
			inet_aton(slave_ph_controller_ip, &slave.sin_addr);

			fprintf(stdout, "SLAVE HOST: %s\n", inet_ntoa(slave.sin_addr));

			if ((slave_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
			{
				fprintf(stderr, "error: a slave socket failed\n");
				close(client_fd);
				continue;
			}

			if(connect(slave_fd, (struct sockaddr *)&slave, sizeof(slave)) < 0)
			{
				fprintf(stderr, "error: the connection on a possible slave failed\n");
				close(slave_fd);
				close(client_fd);
				continue;
			}

			data[0] = '\0';
			sprintf(data, "{\"method\":\"make-action\",\"code\":%d}", code);
			data[strlen(data)] = '\0';
			
			if (send(slave_fd, data, strlen(data) + 1, 0) < 0)
			{
				fprintf(stderr, "error: the send call on the slave failed\n");
				close(slave_fd);
				close(client_fd);
				continue;
			}

			data[0] = '\0';
			if (recv(slave_fd, data, 1024, 0) < 0)
			{
				fprintf(stderr, "error: the recv call on the slave failed\n");
				close(slave_fd);
				close(client_fd);
				continue;
			}
				
			close(slave_fd);

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

			json_method = cJSON_GetObjectItemCaseSensitive(json_data, "status");
			if (cJSON_IsInvalid(json_method))
			{
				fprintf(stderr, "error: the json does not have a \"status\" property\n");
				close(client_fd);
				continue;
			}

			if(!cJSON_IsNumber(json_method))
			{
				fprintf(stderr, "error: the json property \"status\" is not of type number\n");
				close(client_fd);
				continue;
			}

			int status = json_method->valueint;
			if (status == 200)
			{
				data[0] = '\0';
				strcpy(data, "{\"status\":200}");
				data[strlen(data)]= '\0';
					
				if (write(client_fd, data, strlen(data) + 1) == -1)
				{
					fprintf(stderr, "error: the write call on the client failed\n");
				}
			}
		}

		close(client_fd);
	}
    return;
}
