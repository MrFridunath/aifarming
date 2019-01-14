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

#include "lwip/err.h"
#include "lwip/sys.h"

#define ESP32_SSID "AIFarmingMiddleware"
#define ESP32_PASSWORD "aifarmingmiddleware"

static EventGroupHandle_t s_event_group;

static const char *TAG = "AI Farming Middleware Access Point";

// WIFI EVENTS
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_AP_STACONNECTED:
		sleep(3);
        printf("Station connected to access point. MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", MAC2STR(event->event_info.sta_disconnected.mac));
        break;

    case SYSTEM_EVENT_AP_STADISCONNECTED:
        printf("Station disconnected from access point. MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", MAC2STR(event->event_info.sta_disconnected.mac));
        break;

    default:
        break;
    }

    return ESP_OK;
}

//SERVER DHCP
static void start_dhcp_server()
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
    tcpip_adapter_ip_info_t info;
    memset(&info, 0, sizeof(info));
    IP4_ADDR(&info.ip, 192, 168, 1, 1);
    IP4_ADDR(&info.gw, 192, 168, 1, 1);
    IP4_ADDR(&info.netmask, 255, 255, 255, 0);
	
    ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));

    printf("DHCP server started \n");
}

//INICIO WIFI
void wifi_init()
{
    s_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t ap_wifi_config = {
        .ap = {
            .ssid = ESP32_SSID,
            .ssid_len = strlen(ESP32_SSID),
            .password = ESP32_PASSWORD,
            .max_connection = 8,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

	if (strlen(ESP32_PASSWORD) == 0) {
        ap_wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ap_wifi_config.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_wifi_config) );
	ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished.SSID:%s password:%s", ESP32_SSID, ESP32_PASSWORD);
}

//MAIN
void app_main()
{
	esp_err_t err;
	err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
	}
	ESP_ERROR_CHECK(nvs_flash_init());
	
	int32_t stations_added = 0;

    char **masters_mac = (char**)malloc(sizeof(char*) * 8);

	for (int i = 0; i < 8; i++)
	{
		*(masters_mac+i) = (char*)malloc(sizeof(char) * 32);
		strcpy(masters_mac[i], "");
	}

	// RECUPERAR NVS
	nvs_handle nvs;
	err = nvs_open("storage", NVS_READWRITE, &nvs);
	if (err != ESP_OK) 
	{
        fprintf(stderr,"error: the nvs open call failed\n");
		return;
	}
	else {	
		nvs_get_i32(nvs, "stations_added", &stations_added);
		size_t required_size;
		char *station_nvs_mac = (char*)malloc(sizeof(char)*32);
		for (int i = 0; i < stations_added; i++)
		{
			sprintf(station_nvs_mac, "%s%d", "station_mac_", i);
			fprintf(stdout, "STATION NVS: %s\n", station_nvs_mac);
			err = nvs_get_str(nvs, station_nvs_mac, NULL, &required_size);
			if (err == ESP_OK)
			{
				nvs_get_str(nvs, station_nvs_mac, masters_mac[i], &required_size);
			}
		}
		free(station_nvs_mac);
	}
	nvs_close(nvs);

	start_dhcp_server();
	sleep(5);
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
    server.sin_port = htons(40500);

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
	char *temp = (char*)malloc(sizeof(char) * 32);
	char *temp2 = (char*)malloc(sizeof(char) * 32);

	cJSON *json_data = NULL;
	cJSON *json_method = NULL;
	cJSON *json_temp1 = NULL;
	cJSON *json_temp2 = NULL;
		
	wifi_sta_list_t wifi_sta_list;
	tcpip_adapter_sta_list_t adapter_sta_list;

	//API SERVER
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

		//REGISTRAR CULTIVO
		if (strcmp(json_method->valuestring, "add-system") == 0)
		{
			fprintf(stdout, "METHOD: add-system\n");

			int status = 409;
			if(stations_added < 8)
			{
				status = 404;

				memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
				memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));

				ESP_ERROR_CHECK(esp_wifi_ap_get_sta_list(&wifi_sta_list));	
				ESP_ERROR_CHECK(tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list));
				
				char *mac_temp = (char*)malloc(sizeof(char)*32);
				for(int i = 0; i < adapter_sta_list.num; i++)
				{
					tcpip_adapter_sta_info_t station = adapter_sta_list.sta[i];
					sprintf(mac_temp, MACSTR, MAC2STR(station.mac));

					fprintf(stdout, "MAC STATION: %s\n", mac_temp);

					int added = 0;
					for (int j = 0; j < 8; j++)
					{
						if (strcmp(mac_temp, masters_mac[j]) == 0)
						{
							added = 1;
							break;
						}
					}
					if (added)
					{
						continue;
					}
					else 
					{
						struct sockaddr_in master;
						int master_fd;
	
						master.sin_family = AF_INET;
						master.sin_port = htons(40501);
						
						sprintf(temp2, "%s", ip4addr_ntoa(&(station.ip)));
						inet_aton(temp2, &master.sin_addr);
						
						data[0] = '\0';
						strcpy(data, "{\"method\":\"get-system\"}");
						data[strlen(data)] = '\0';

						fprintf(stdout, "SEND TO MASTER: %s\n", data);

						if ((master_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
						{
							fprintf(stderr, "error: the master socket failed\n");
							continue;
						}

						if (connect(master_fd, (struct sockaddr *)&master, sizeof(master)) == -1)
						{
							fprintf(stderr, "error: the connection call on the master failed\n");
							close(master_fd);
							continue;
						}
						
						if (send(master_fd, data, strlen(data) + 1, 0) < 0)
						{
							fprintf(stderr, "error: the send call on the master failed\n");
							close(master_fd);
							continue;
						}
						
						
						data[0] = '\0';
						if (recv(master_fd, data, 1024, 0) < 0)
						{
							fprintf(stderr, "error: the recv call on the slave failed\n");
							close(master_fd);
							continue;
						}

						fprintf(stdout, "RESPONSE FROM MASTER: %s\n", data);
						close(master_fd);
						
						json_temp1= cJSON_Parse(data);
						if(cJSON_IsInvalid(json_temp1))
						{
							fprintf(stderr, "error: the data is not in a json format\n");
							continue;
						}
				
						if(!cJSON_IsObject(json_temp1))
						{
							fprintf(stderr, "error: the json data is not of the type object\n");
							continue;
						}
					
						json_temp2 = cJSON_GetObjectItemCaseSensitive(json_temp1, "status");
						if (cJSON_IsInvalid(json_temp2))
						{
							fprintf(stderr, "error: the json does not have a \"status\" property\n");
							continue;
						}
					
						if (!cJSON_IsNumber(json_temp2))
						{
							fprintf(stderr, "error: the json property \"status\" is not of type number\n");
							continue;
						}
					
						if (json_temp2->valueint == 200)
						{
							int custom_settings = 1;
							json_temp1 = cJSON_GetObjectItemCaseSensitive(json_data, "properties");
							if (cJSON_IsInvalid(json_temp1))
							{
								fprintf(stderr, "error: the json does not have a \"properties\" property\n");
								custom_settings = 0;
							}

							if(!cJSON_IsObject(json_temp1))
							{
								fprintf(stderr, "error: the json property \"properties\" is not of type object\n");
								custom_settings = 0;
							}
							
							if (custom_settings)
							{
								data[0] = '\0';
								strcpy(data, "{\"method\":\"update-system\",\"properties\":{");

								json_temp2 = cJSON_GetObjectItemCaseSensitive(json_temp1, "objective_ph");
								if (cJSON_IsInvalid(json_temp2))
								{
									fprintf(stderr, "error: the json does not have a \"objective_ph\" property\n");
								}
								else
								{
									if(!cJSON_IsString(json_temp2))
									{
										fprintf(stderr, "error: the json property \"objective_ph\" is not of type string\n");
									}
									else 
									{
										strcat(data, "\"objective_ph\":\"");
										strcat(data, json_temp2->valuestring);
										strcat(data, "\",");
									}
								}
								
								json_temp2 = cJSON_GetObjectItemCaseSensitive(json_temp1, "ph_umbral");
								if (cJSON_IsInvalid(json_temp2))
								{
									fprintf(stderr, "error: the json does not have a \"ph_umbral\" property\n");
								}
								else
								{
									if(!cJSON_IsString(json_temp2))
									{
										fprintf(stderr, "error: the json property \"ph_umbral\" is not of type string\n");
									}
									else 
									{
										strcat(data, "\"ph_umbral\":\"");
										strcat(data, json_temp2->valuestring);
										strcat(data, "\"");
									}
								}

								strcat(data, "}}");
								data[strlen(data)] = '\0';
	
								master.sin_family = AF_INET;
								master.sin_port = htons(40501);
								
								sprintf(temp2, "%s", ip4addr_ntoa(&(station.ip)));
								inet_aton(temp2, &master.sin_addr);			
								
								if ((master_fd = socket(AF_INET, SOCK_STREAM, 0)) != -1)
								{
									if (connect(master_fd, (struct sockaddr *)&master, sizeof(master)) != -1)
									{
										if (send(master_fd, data, strlen(data) + 1, 0) < 0)
										{
											fprintf(stderr, "error: the send call on the master failed\n");
										}
									}
								}
								close(master_fd);
							}

							err = nvs_open("storage", NVS_READWRITE, &nvs);
							if (err == ESP_OK)
							{
								sprintf(temp, "%s%d", "station_mac_", stations_added);
						
								sprintf(temp2, MACSTR, MAC2STR(station.mac));
								strcpy(masters_mac[stations_added], temp2);
								
								nvs_set_str(nvs, temp, temp2);
								stations_added += 1;
								nvs_set_i32(nvs, "stations_added", stations_added);
								
								fprintf(stdout, "STATION WRITE IN NVS: %s\n", temp2);
								
								status = 200;
							}
							nvs_commit(nvs);
							nvs_close(nvs);
							break;
						}
					}
				}
				free(mac_temp);
			}
			
			data[0] = '\0';
			sprintf(data, "{\"status\":%d}", status);
			data[strlen(data)]= '\0';
			
			if (write(client_fd, data, strlen(data)) == -1)
			{
				fprintf(stderr, "error: the write call on the client failed\n");
			}
		}

		//ELIMINAR CULTIVO
		else if (strcmp(json_method->valuestring, "delete-system") == 0)
		{
			fprintf(stdout, "METHOD: delete-system\n");
			
			int status = 409;
			if (stations_added > 0)
			{
				while(1)
				{
					json_temp1 = cJSON_GetObjectItemCaseSensitive(json_data, "id");
					if (cJSON_IsInvalid(json_temp1))
					{
						fprintf(stderr, "error: the json does not have a \"id\" property\n");
						status = 400;
						break;
					}
					
					if (!cJSON_IsNumber(json_temp1))
					{
						fprintf(stderr, "error: the json property \"id\" is not of type number\n");
						status = 400;
						break;
					}
				
					int id = json_temp1->valueint;
				
					if (strcmp(masters_mac[id], "") == 0)
					{
						status = 404;
						break;
					}
					
					err = nvs_open("storage", NVS_READWRITE, &nvs);
					size_t required_size;

					sprintf(temp, "%s%d", "station_mac_", id);
					err = nvs_get_str(nvs, temp, NULL, &required_size);

					if (err == ESP_OK)
					{
						err = nvs_erase_key(nvs, temp);
						if (err == ESP_OK)
						{
							fprintf(stdout, "STATION DELETED FROM NVS: %s\n", masters_mac[id]);

							strcpy(masters_mac[id], "");
							stations_added -= 1;
							nvs_set_i32(nvs, "stations_added", stations_added);							
							
							status = 200;
						}
					}
					
					nvs_commit(nvs);
					nvs_close(nvs);
					
					break;
				}
			}
			data[0] = '\0';
			sprintf(data, "{\"status\":%d}", status);
			data[strlen(data)]= '\0';
			
			if (write(client_fd, data, strlen(data)) == -1)
			{
				fprintf(stderr, "error: the write call on the client failed\n");
			}
		}
		
		//EDITAR PROPIEDADES DE PH DE CULTIVO REGISTRADO
		else if (strcmp(json_method->valuestring, "edit-system") == 0)
		{
			fprintf(stdout, "METHOD: edit-system\n");
			
			int status = 409;
			if (stations_added > 0)
			{	
				while(1)
				{
					json_temp1 = cJSON_GetObjectItemCaseSensitive(json_data, "id");
					if (cJSON_IsInvalid(json_temp1))
					{
						fprintf(stderr, "error: the json does not have a \"id\" property\n");
						status = 400;
						break;
					}
					
					if (!cJSON_IsNumber(json_temp1))
					{
						fprintf(stderr, "error: the json property \"id\" is not of type number\n");
						status = 400;
						break;
					}
				
					err = nvs_open("storage", NVS_READWRITE, &nvs);
					
					if (err != ESP_OK)
					{
						status = 500;
						break;
					}

					int id = json_temp1->valueint;
					sprintf(temp, "%s%d", "station_mac_", id);
					
					size_t required_size;					
					err = nvs_get_str(nvs, temp, NULL, &required_size);

					nvs_close(nvs);

					status = 404;
					if (err == ESP_OK)
					{
						memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
						memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));

						ESP_ERROR_CHECK(esp_wifi_ap_get_sta_list(&wifi_sta_list));
						ESP_ERROR_CHECK(tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list));
										
						for(int i = 0; i < adapter_sta_list.num; i++)
						{
							tcpip_adapter_sta_info_t station = adapter_sta_list.sta[i];
							sprintf(temp2, MACSTR, MAC2STR(station.mac));
							
							fprintf(stdout, "MAC STATION: %s %s\n",temp2, masters_mac[id]);
							
							if (strcmp(temp2, masters_mac[id]) == 0)
							{
								status = 408;

								json_temp1 = cJSON_GetObjectItemCaseSensitive(json_data, "properties");
								if (cJSON_IsInvalid(json_temp1))
								{
									fprintf(stderr, "error: the json does not have a \"properties\" property\n");
									status = 400;
									break;
								}

								if(!cJSON_IsObject(json_temp1))
								{
									fprintf(stderr, "error: the json property \"properties\" is not of type object\n");
									status = 400;
									break;
								}
								
								data[0] = '\0';
								strcpy(data, "{\"method\":\"edit-system\",\"properties\":{");

								json_temp2 = cJSON_GetObjectItemCaseSensitive(json_temp1, "objective_ph");
								if (cJSON_IsInvalid(json_temp2))
								{
									fprintf(stderr, "error: the json does not have a \"objective_ph\" property\n");
								}
								else
								{
									if(!cJSON_IsString(json_temp2))
									{
										fprintf(stderr, "error: the json property \"objective_ph\" is not of type string\n");
									}
									else 
									{
										strcat(data, "\"objective_ph\":\"");
										strcat(data, json_temp2->valuestring);
										strcat(data, "\",");
									}
								}
								
								json_temp2 = cJSON_GetObjectItemCaseSensitive(json_temp1, "ph_umbral");
								if (cJSON_IsInvalid(json_temp2))
								{
									fprintf(stderr, "error: the json does not have a \"ph_umbral\" property\n");
								}
								else
								{
									if(!cJSON_IsString(json_temp2))
									{
										fprintf(stderr, "error: the json property \"ph_umbral\" is not of type string\n");
									}
									else 
									{
										strcat(data, "\"ph_umbral\":\"");
										strcat(data, json_temp2->valuestring);
										strcat(data, "\"");
									}
								}

								strcat(data, "}}");
								data[strlen(data)] = '\0';
	
								struct sockaddr_in master;
								int master_fd;
	
								master.sin_family = AF_INET;
								master.sin_port = htons(40501);
		
								sprintf(temp2, "%s", ip4addr_ntoa(&(station.ip)));
								inet_aton(temp2, &master.sin_addr);							
																
								if ((master_fd = socket(AF_INET, SOCK_STREAM, 0)) != -1)
								{
									if (connect(master_fd, (struct sockaddr *)&master, sizeof(master)) != -1)
									{
										if (send(master_fd, data, strlen(data) + 1, 0) < 0)
										{
											fprintf(stderr, "error: the send call on the master failed\n");
										}
									}
								}
						
								data[0] = '\0';
								if (recv(master_fd, data, 1024, 0) < 0)
								{
									fprintf(stderr, "error: the recv call on the slave failed\n");
									close(master_fd);
									break;
								}								
								
								fprintf(stdout, "RESPONSE: %s\n", data);
								close(master_fd);
						
								json_temp1= cJSON_Parse(data);
								if(cJSON_IsInvalid(json_temp1))
								{
									fprintf(stderr, "error: the data is not in a json format\n");
									break;
								}
				
								if(!cJSON_IsObject(json_temp1))
								{
									fprintf(stderr, "error: the json data is not of the type object\n");
									break;
								}
					
								json_temp2 = cJSON_GetObjectItemCaseSensitive(json_temp1, "status");
								if (cJSON_IsInvalid(json_temp2))
								{
									fprintf(stderr, "error: the json does not have a \"status\" property\n");
									break;
								}
					
								if (!cJSON_IsNumber(json_temp2))
								{
									fprintf(stderr, "error: the json property \"status\" is not of type number\n");
									break;
								}
					
								if (json_temp2->valueint == 200)
								{
									status = 200;
								}
								
								break;
							}
						}
					}
					break;
				}
			}
			data[0] = '\0';
			sprintf(data, "{\"status\":%d}", status);
			data[strlen(data)]= '\0';
			
			if (write(client_fd, data, strlen(data)) == -1)
			{
				fprintf(stderr, "error: the write call on the client failed\n");
			}
		}

		//OBTENER DATOS DE CULTIVO
		else if (strcmp(json_method->valuestring, "get-system") == 0)
		{
			int status = 409;
			if (stations_added > 0)
			{				
				while(1)
				{
					json_temp1 = cJSON_GetObjectItemCaseSensitive(json_data, "id");
					if (cJSON_IsInvalid(json_temp1))
					{
						fprintf(stderr, "error: the json does not have a \"id\" property\n");
						status = 400;
						break;
					}
					
					if (!cJSON_IsNumber(json_temp1))
					{
						fprintf(stderr, "error: the json property \"id\" is not of type number\n");
						status = 400;
						break;
					}
				
					err = nvs_open("storage", NVS_READWRITE, &nvs);
					
					if (err != ESP_OK)
					{
						status = 500;
						break;
					}

					int id = json_temp1->valueint;
					sprintf(temp, "%s%d", "station_mac_", id);
					
					size_t required_size;
					err = nvs_get_str(nvs, temp, NULL, &required_size);

					nvs_close(nvs);

					status = 404;
					if (err == ESP_OK)
					{
						memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
						memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));

						ESP_ERROR_CHECK(esp_wifi_ap_get_sta_list(&wifi_sta_list));
						ESP_ERROR_CHECK(tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list));
						
				
						for(int i = 0; i < adapter_sta_list.num; i++)
						{
							tcpip_adapter_sta_info_t station = adapter_sta_list.sta[i];
							sprintf(temp2, MACSTR, MAC2STR(station.mac));
							if (strcmp(temp2, masters_mac[id]) == 0)
							{
								status = 408;

								struct sockaddr_in master;
								int master_fd;
	
								master.sin_family = AF_INET;
								master.sin_port = htons(40501);
								
								char *ip = (char*)malloc(sizeof(char) * 32);
								sprintf(ip, "%s", ip4addr_ntoa(&(station.ip)));
								inet_aton(ip, &master.sin_addr);
								
								free(ip);
								
								data[0] = '\0';
								strcpy(data, "{\"method\":\"get-system\"}");
								data[strlen(data)] = '\0';
	
								if ((master_fd = socket(AF_INET, SOCK_STREAM, 0)) != -1)
								{
									if (connect(master_fd, (struct sockaddr *)&master, sizeof(master)) != -1)
									{
										if (send(master_fd, data, strlen(data) + 1, 0) < 0)
										{
											fprintf(stderr, "error: the send call on the master failed\n");
										}
									}
								}
						
								data[0] = '\0';
								if (recv(master_fd, data, 1024, 0) < 0)
								{
									fprintf(stderr, "error: the recv call on the slave failed\n");
									close(master_fd);
									break;
								}
																
								fprintf(stdout, "RESPONSE: %s\n", data);
								close(master_fd);
						
								json_temp1= cJSON_Parse(data);
								if(cJSON_IsInvalid(json_temp1))
								{
									fprintf(stderr, "error: the data is not in a json format\n");
									break;
								}
				
								if(!cJSON_IsObject(json_temp1))
								{
									fprintf(stderr, "error: the json data is not of the type object\n");
									break;
								}
					
								json_temp2 = cJSON_GetObjectItemCaseSensitive(json_temp1, "status");
								if (cJSON_IsInvalid(json_temp2))
								{
									fprintf(stderr, "error: the json does not have a \"status\" property\n");
									break;
								}
					
								if (!cJSON_IsNumber(json_temp2))
								{
									fprintf(stderr, "error: the json property \"status\" is not of type number\n");
									break;
								}
					
								if (json_temp2->valueint == 200)
								{
									status = 200;
								}

								break;
							}
						}
					}
					break;
				}
			}

			if (status == 200)
			{
				data[0] = '\0';
				sprintf(data, "{\"status\":%d,\"data\":{", status);
				
				json_temp2 = cJSON_GetObjectItemCaseSensitive(json_temp1, "data");
				if (cJSON_IsInvalid(json_temp2))
				{
					fprintf(stderr, "error: the json does not have a \"data\" property\n");
				}
				else
				{
					if (!cJSON_IsObject(json_temp2))
					{
						fprintf(stderr, "error: the json property \"data\" is not of type object\n");
					}
					else
					{
						json_temp1 = cJSON_GetObjectItemCaseSensitive(json_temp2, "air_temperature");
						if (cJSON_IsInvalid(json_temp1))
						{
							fprintf(stderr, "error: the json does not have a \"air_temperature\" property\n");
						}
						else
						{
							if(!cJSON_IsString(json_temp1))
							{
								fprintf(stderr, "error: the json property \"air_temperature\" is not of type string\n");
							}
							else 
							{
								strcat(data, "\"air_temperature\":\"");
								strcat(data, json_temp1->valuestring);
								strcat(data, "\",");
							}
						}
						
						json_temp1 = cJSON_GetObjectItemCaseSensitive(json_temp2, "air_humidity");
						if (cJSON_IsInvalid(json_temp1))
						{
							fprintf(stderr, "error: the json does not have a \"air_humidity\" property\n");
						}
						else
						{
							if(!cJSON_IsString(json_temp1))
							{
								fprintf(stderr, "error: the json property \"air_humidity\" is not of type string\n");
							}
							else 
							{
								strcat(data, "\"air_humidity\":\"");
								strcat(data, json_temp1->valuestring);
								strcat(data, "\",");
							}
						}

						json_temp1 = cJSON_GetObjectItemCaseSensitive(json_temp2, "soil_humidity");
						if (cJSON_IsInvalid(json_temp1))
						{
							fprintf(stderr, "error: the json does not have a \"soil_humidity\" property\n");
						}
						else
						{
							if(!cJSON_IsString(json_temp1))
							{
								fprintf(stderr, "error: the json property \"soil_humidity\" is not of type string\n");
							}
							else 
							{
								strcat(data, "\"soil_humidity\":\"");
								strcat(data, json_temp1->valuestring);
								strcat(data, "\",");
							}
						}

						json_temp1 = cJSON_GetObjectItemCaseSensitive(json_temp2, "rain_sensor");
						if (cJSON_IsInvalid(json_temp1))
						{
							fprintf(stderr, "error: the json does not have a \"rain_sensor\" property\n");
						}
						else
						{
							if(!cJSON_IsString(json_temp1))
							{
								fprintf(stderr, "error: the json property \"rain_sensor\" is not of type string\n");
							}
							else 
							{
								strcat(data, "\"rain_sensor\":\"");
								strcat(data, json_temp1->valuestring);
								strcat(data, "\",");
							}
						}

						json_temp1 = cJSON_GetObjectItemCaseSensitive(json_temp2, "light_sensor");
						if (cJSON_IsInvalid(json_temp1))
						{
							fprintf(stderr, "error: the json does not have a \"light_sensor\" property\n");
						}
						else
						{
							if(!cJSON_IsString(json_temp1))
							{
								fprintf(stderr, "error: the json property \"light_sensor\" is not of type string\n");
							}
							else 
							{
								strcat(data, "\"light_sensor\":\"");
								strcat(data, json_temp1->valuestring);
								strcat(data, "\",");
							}
						}

						json_temp1 = cJSON_GetObjectItemCaseSensitive(json_temp2, "water_ph");
						if (cJSON_IsInvalid(json_temp1))
						{
							fprintf(stderr, "error: the json does not have a \"water_ph\" property\n");
						}
						else
						{
							if(!cJSON_IsString(json_temp1))
							{
								fprintf(stderr, "error: the json property \"water_ph\" is not of type string\n");
							}
							else 
							{
								strcat(data, "\"water_ph\":\"");
								strcat(data, json_temp1->valuestring);
								strcat(data, "\",");
							}
						}

						json_temp1 = cJSON_GetObjectItemCaseSensitive(json_temp2, "last_water");
						if (cJSON_IsInvalid(json_temp1))
						{
							fprintf(stderr, "error: the json does not have a \"last_water\" property\n");
						}
						else
						{
							if(!cJSON_IsString(json_temp1))
							{
								fprintf(stderr, "error: the json property \"last_water\" is not of type string\n");
							}
							else 
							{
								strcat(data, "\"last_water\":\"");
								strcat(data, json_temp1->valuestring);
								strcat(data, "\"");
							}
						}
					}
				}
				strcat(data, "}}");
				data[strlen(data)] = '\0';
			}
			else
			{
				data[0] = '\0';
				sprintf(data, "{\"status\":%d}", status);
				data[strlen(data)]= '\0';
			}
			
			if (write(client_fd, data, strlen(data)) == -1)
			{
				fprintf(stderr, "error: the write call on the client failed\n");
			}
		}
		
		//ENVIAR ACCION
		else if (strcmp(json_method->valuestring, "send-action") == 0)
		{
			fprintf(stdout, "METHOD: send-action\n");

			int status = 409;
			if (stations_added > 0)
			{			
				while(1)
				{
					json_temp1 = cJSON_GetObjectItemCaseSensitive(json_data, "code");
					if (cJSON_IsInvalid(json_temp1))
					{
						fprintf(stderr, "error: the json does not have a \"code\" property\n");
						status = 400;
						break;
					}
					
					if (!cJSON_IsNumber(json_temp1))
					{
						fprintf(stderr, "error: the json property \"code\" is not of type number\n");
						status = 400;
						break;
					}

					int code = json_temp1->valueint;
					
					if ((code != 1) && (code != 2))
					{
						fprintf(stderr, "error: the json property \"code\" is not a valid number\n");
						status = 400;
						break;
					}

					json_temp1 = cJSON_GetObjectItemCaseSensitive(json_data, "id");
					if (cJSON_IsInvalid(json_temp1))
					{
						fprintf(stderr, "error: the json does not have a \"id\" property\n");
						status = 400;
						break;
					}
					
					if (!cJSON_IsNumber(json_temp1))
					{
						fprintf(stderr, "error: the json property \"id\" is not of type number\n");
						status = 400;
						break;
					}
				
					err = nvs_open("storage", NVS_READWRITE, &nvs);
					
					if (err != ESP_OK)
					{
						status = 500;
						break;
					}

					int id = json_temp1->valueint;
					sprintf(temp, "%s%d", "station_mac_", id);
					
					size_t required_size;
					err = nvs_get_str(nvs, temp, NULL, &required_size);

					nvs_close(nvs);

					status = 404;
					if (err == ESP_OK)
					{

						memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
						memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));

						ESP_ERROR_CHECK(esp_wifi_ap_get_sta_list(&wifi_sta_list));
						ESP_ERROR_CHECK(tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list));
				
						for(int i = 0; i < adapter_sta_list.num; i++)
						{
							tcpip_adapter_sta_info_t station = adapter_sta_list.sta[i];
							sprintf(temp2, MACSTR, MAC2STR(station.mac));
							
							fprintf(stdout, "MAC STATION: %s %s\n",temp2, masters_mac[id]);
							
							if (strcmp(temp2, masters_mac[id]) == 0)
							{
								status = 408;
								
								struct sockaddr_in master;
								int master_fd;
	
								master.sin_family = AF_INET;
								master.sin_port = htons(40501);
								
								char *ip = (char*)malloc(sizeof(char) * 32);
								sprintf(ip, "%s", ip4addr_ntoa(&(station.ip)));
								inet_aton(ip, &master.sin_addr);
								
								free(ip);
								
								data[0] = '\0';
								sprintf(data, "{\"method\":\"send-action\",\"code\":%d}", code);
								data[strlen(data)] = '\0';
	
								if ((master_fd = socket(AF_INET, SOCK_STREAM, 0)) != -1)
								{
									if (connect(master_fd, (struct sockaddr *)&master, sizeof(master)) != -1)
									{
										if (send(master_fd, data, strlen(data) + 1, 0) < 0)
										{
											fprintf(stderr, "error: the send call on the master failed\n");
										}
									}
								}
						
								data[0] = '\0';
								if (recv(master_fd, data, 1024, 0) < 0)
								{
									fprintf(stderr, "error: the recv call on the slave failed\n");
									close(master_fd);
									break;
								}
																
								fprintf(stdout, "RESPONSE: %s\n", data);
								close(master_fd);
						
								json_temp1= cJSON_Parse(data);
								if(cJSON_IsInvalid(json_temp1))
								{
									fprintf(stderr, "error: the data is not in a json format\n");
									break;
								}
				
								if(!cJSON_IsObject(json_temp1))
								{
									fprintf(stderr, "error: the json data is not of the type object\n");
									break;
								}
					
								json_temp2 = cJSON_GetObjectItemCaseSensitive(json_temp1, "status");
								if (cJSON_IsInvalid(json_temp2))
								{
									fprintf(stderr, "error: the json does not have a \"status\" property\n");
									break;
								}
					
								if (!cJSON_IsNumber(json_temp2))
								{
									fprintf(stderr, "error: the json property \"status\" is not of type number\n");
									break;
								}
					
								if (json_temp2->valueint == 200)
								{
									status = 200;
								}

								break;
							}
						}
					}
					break;
				}
			}

			data[0] = '\0';
			sprintf(data, "{\"status\":%d}", status);
			data[strlen(data)]= '\0';
			
			if (write(client_fd, data, strlen(data)) == -1)
			{
				fprintf(stderr, "error: the write call on the client failed\n");
			}
		}
		
		close(client_fd);
	}
    return;
}
