// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// This sample shows how to translate the Device Twin json received from Azure IoT Hub into meaningful data for your application.
// It uses the parson library, a very lightweight json parser.

// There is an analogous sample using the serializer - which is a library provided by this SDK to help parse json - in devicetwin_simplesample.
// Most applications should use this sample, not the serializer.

// WARNING: Check the return of all API calls when developing your solution. Return checks ommited for sample simplification.

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>

#include "azure_c_shared_utility/macro_utils.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/platform.h"
#include "iothub_device_client.h"
#include "iothub_client_options.h"
#include "iothub.h"
#include "iothub_message.h"
#include "parson.h"
#include "curl/curl.h"
#include "mosquitto.h"
#include "base64Decode.h"
//#include "device_provisioning.h"
#include "uuid4.h"

// The protocol you wish to use should be uncommented
//
//#define SAMPLE_MQTT
//#define SAMPLE_MQTT_OVER_WEBSOCKETS
//#define SAMPLE_AMQP
#define SAMPLE_AMQP_OVER_WEBSOCKETS
//#define SAMPLE_HTTP

#ifdef SAMPLE_MQTT
    #include "iothubtransportmqtt.h"
#endif // SAMPLE_MQTT
#ifdef SAMPLE_MQTT_OVER_WEBSOCKETS
    #include "iothubtransportmqtt_websockets.h"
#endif // SAMPLE_MQTT_OVER_WEBSOCKETS
#ifdef SAMPLE_AMQP
    #include "iothubtransportamqp.h"
#endif // SAMPLE_AMQP
#ifdef SAMPLE_AMQP_OVER_WEBSOCKETS
    #include "iothubtransportamqp_websockets.h"
#endif // SAMPLE_AMQP_OVER_WEBSOCKETS
#ifdef SAMPLE_HTTP
    #include "iothubtransporthttp.h"
#endif // SAMPLE_HTTP

#ifdef SET_TRUSTED_CERT_IN_SAMPLES
#include "certs.h"
#endif // SET_TRUSTED_CERT_IN_SAMPLES

#define DOWORK_LOOP_NUM     3

bool g_Cancel = false;
static bool messagingEnabled = true; 

typedef struct GATEWAY_TAG
{
    double knownDevices; // reported property
    int WhiteListStatus; // reported property
    int useWhiteListOnly; //desired property
    char* defaultAppeui; //desired property
    char* defaultAppkey; //desired property
    double softwareVersion; //reported app version
} Gateway;

// ****************************************************************************************************************
// Utilities 

const char* LoadConnectionString() {

    JSON_Value *cs_data = json_parse_file("config/ConnectionString.json");
    const char * connectionString = json_object_get_string(json_object(cs_data), "ConnectionString");
    messagingEnabled = (bool) json_object_get_boolean(json_object(cs_data),"MessagingEnabled");
    printf("CS: %s\n", connectionString);
    if(messagingEnabled){
        printf("Messaging Enabled\n");
    }
    else {
        printf("Messaging Disabled\n");
    }
    //json_value_free(cs_data); // frees the connectionString memory
    return connectionString;
}

bool ishyphen(char test)
{
	if (test == '-')
		return true;
	else
		return false;
}

void remove_hyphens(char* str_trimmed, const char* str_untrimmed)
{
	while (*str_untrimmed != '\0')
	{
		if (!ishyphen(*str_untrimmed))
		{
			*str_trimmed = *str_untrimmed;
			str_trimmed++;
		}
		str_untrimmed++;
	}
	*str_trimmed = '\0';
}



// Variable declarations
    IOTHUB_DEVICE_CLIENT_HANDLE iotHubClientHandle;
    struct mosquitto *mosq = NULL;

// ****************************************************************************************************************
// Mosquito functions

THREAD_HANDLE       listenerThread;

void mqtt_message_callback(struct mosquitto *mosq, void *userdata,const struct mosquitto_message *message)
{
    if (iotHubClientHandle == NULL) {
        printf("ERROR: IoT Hub client not initialized\n");
    }
    else {

        JSON_Value *JVpayload = json_parse_string(message->payload);
        JSON_Object *JOpayload = json_value_get_object (JVpayload);

        char newMessage[512];

        // Base64 encoding required for byte array
        // Use %x in the sprintf if required to present hex base64decoded
        const char* dataPayload = json_object_get_string(JOpayload, "data"); // base64
        char clrdst[250] = ""; //base64 decoded payload data. 250 > 242 maximum LoRaWAN payload size
        b64_decode((char *)dataPayload, clrdst);

        // Required for Digital Twins, message properties
        //"DigitalTwins-Telemetry", "1.0"
        //"DigitalTwins-SensorHardwareId", $"{sensor.HardwareId}"
        //"CreationTimeUtc", DateTime.UtcNow.ToString("o")
        //"x-ms-client-request-id", Guid.NewGuid().ToString()

        char uuidBuf[UUID4_LEN];
        uuid4_init();
        uuid4_generate(uuidBuf);

        const char * rawDevEui = json_object_get_string(JOpayload,"deveui");
        char * deveui = (char*)malloc(17);
        remove_hyphens( deveui, rawDevEui );

        sprintf(newMessage,"{\"deveui\":\"%s\",\"appeui\":\"%s\",\"gweui\":\"%s\",\"data\":\"%s\",\"time\":\"%s\",\"datr\":\"%s\",\"rssi\":%.0f}",
            rawDevEui,
            json_object_get_string(JOpayload, "appeui"),
            json_object_get_string(JOpayload,"gweui"),
            json_object_get_string(JOpayload,"data"),
            json_object_get_string(JOpayload,"time"),
            json_object_get_string(JOpayload,"datr"),
            json_object_get_number(JOpayload, "rssi")
            );

        printf("Version: 1.0\n");
        printf("%x, %s\n", clrdst,json_object_get_string(JOpayload,"data") );
        printf("%s\n",message->payload);

        IOTHUB_MESSAGE_HANDLE message_handle = IoTHubMessage_CreateFromString(newMessage);

        if (message_handle == 0) {
            printf("ERROR: unable to create a new IoTHubMessage\n");
        }
        else {

            	IOTHUB_MESSAGE_RESULT message_result1 = IoTHubMessage_SetProperty(message_handle, "DigitalTwins-Telemetry","1.0");
                
				if (message_result1 != IOTHUB_MESSAGE_OK) {
					printf("ERROR: unable to create 1 property on IoTHubMessage\n");
					return;
				}

                IOTHUB_MESSAGE_RESULT message_result2 = IoTHubMessage_SetProperty(message_handle, "DigitalTwins-SensorHardwareId",deveui);
                
				if (message_result2 != IOTHUB_MESSAGE_OK) {
					printf("ERROR: unable to create 2 property on IoTHubMessage\n");
					return;
				}
                IOTHUB_MESSAGE_RESULT message_result3 = IoTHubMessage_SetProperty(message_handle, "CreationTimeUtc", json_object_get_string(JOpayload,"time"));
                
				if (message_result3 != IOTHUB_MESSAGE_OK) {
					printf("ERROR: unable to create 3 property on IoTHubMessage\n");
					return;
				}
                IOTHUB_MESSAGE_RESULT message_result4 = IoTHubMessage_SetProperty(message_handle, "x-ms-client-request-id",uuidBuf);
                
				if (message_result4 != IOTHUB_MESSAGE_OK) {
					printf("ERROR: unable to create 4 property on IoTHubMessage\n");
					return;
				}

            if (IoTHubDeviceClient_SendEventAsync(iotHubClientHandle, message_handle,
                NULL,
                /*&callback_param*/ 0) != IOTHUB_CLIENT_OK) {
                printf("ERROR: failed to hand over the message to IoTHubClient\n");
            }
            else {
                printf("INFO: IoTHubClient accepted the message for delivery\n");
            }
        }

        if (JVpayload != NULL) {
		    json_value_free(JVpayload);
        }

    }
}

void mqtt_connect_callback(struct mosquitto *mosq, void *userdata, int result)
{
	if(!result){
		/* Subscribe to broker information topics on successful connect. */
		mosquitto_subscribe(mosq, NULL, "lora/+/up", 2); // $SYS/# is topic specifier. "lora/+/up" is Conduit lorawan message topic
	}else{
		fprintf(stderr, "Connect failed\n");
        (void)fflush(stdout);
	}
}

void mqtt_subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
{
	int i;

	printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);
	for(i=1; i<qos_count; i++){
		printf(", %d", granted_qos[i]);
        (void)fflush(stdout);
	}
	printf("\n");
    (void)fflush(stdout);
}

void mqtt_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
	/* Pring all log messages regardless of level. */
	printf("%s\n", str);
    (void)fflush(stdout);
}

static int mqtt_worker()
{
    // Code issues:
    // 1. Global mqtt_module_data because mqtt_message_callback can't obtain module_data otherwise
    // 4. Hardcoded keepalive

	char *host = "localhost"; //(char *)mqtt_module_data->portHost; //"localhost"; //tcp://127.0.0.1"; //localhost
	int port = 1883; //mqtt_module_data->port; //1883; // or 8883 for SSL
	int keepalive = 60;
	bool clean_session = true;
	//struct mosquitto *mosq = NULL;

	mosquitto_lib_init();
	mosq = mosquitto_new(NULL, clean_session, NULL);
	if(!mosq){
		fprintf(stderr, "Error: Out of memory.\n");
        (void)fflush(stdout);
		return 1;
	}
	mosquitto_log_callback_set(mosq, mqtt_log_callback);
	mosquitto_connect_callback_set(mosq, mqtt_connect_callback);
	mosquitto_message_callback_set(mosq, mqtt_message_callback);
	mosquitto_subscribe_callback_set(mosq, mqtt_subscribe_callback);

	if(mosquitto_connect(mosq, host, port, keepalive)){
		fprintf(stderr, "Unable to connect.\n");
        (void)fflush(stdout);
		return 1;
	}

	mosquitto_loop_forever(mosq, -1, 1);

	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();
    return 0;
}



// ****************************************************************************************************************
// Direct Method functions
static int PostDownLink(const unsigned char* payload)
{

    //deveui – DevEUI of destination end device
    //data – base64 encoded data to be sent in downlink packet payload.
    //ack – optional, request ACK of receipt of packet by end device. If enabled the end device should send an ACK in its next uplink //packet after receiving the downlink. If the ACK is not seen in the uplink the same downlink packet may be sent again, depending on retry settings. In Class C the ACK will be expected soon after the downlink is transmitted, i.e. five seconds, otherwise a retry will be sent.
    //ack_retries – optional, number of re-transmissions to perform in sending the downlink. If network server is unable to schedule the downlink it will not be counted against the retries.
    //rx_wnd – optional, specify Rx window downlink should be scheduled for values (0, 1 or 2). If provided, and not 0, the network server will wait until the packet can be scheduled for the specified window before sending the packet to the radio for transmission. The packet will remain in the queue until scheduled. Set to 0 for Class C devices.

int result = 200;


    JSON_Value *JVpayload = json_parse_string(payload);
    JSON_Object *JOpayload = json_value_get_object (JVpayload);

    char topic[128];
    const char * deveui = json_object_dotget_string (JOpayload, "deveui");
    
    sprintf(topic,"lora/%s/down", deveui);

    int payloadlen = strlen((char*)payload);

    // Debug info
    printf("\n\n%s\n\n", topic );
    printf("%s\n\n", payload);
    printf("%d\n\n", payloadlen);

    int ret = mosquitto_publish(mosq, NULL, (const char*)topic, payloadlen, payload, 1, false);

    if (ret)
    {
        result = 500;
    }

    return result;
}

static int RestartLoRaWANServer()
{
    printf("RestartLoRaWANServer\n");
        int result = 200;
        CURL *curl;
		CURLcode res;
        curl = curl_easy_init();
        if (curl) {
            struct curl_slist *hs=NULL;
            hs = curl_slist_append(hs, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hs);
            curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1/api/lora/restart");
            /* Now specify the POST data */
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "" );
            
            /* Perform the request, res will get the return code */
            res = curl_easy_perform(curl);

            /* Check for errors */
            if (res != CURLE_OK)
            {
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
                result = 500;
            }
            /* always cleanup */
            curl_easy_cleanup(curl);
        }
        curl_global_cleanup();
        return result;
}

static int SaveLoRaWANChanges()
{
    printf("SaveLoRaWANChanges\n");
    	CURL *curl;
		CURLcode res;
        int result = 200;
        curl = curl_easy_init();
        if (curl) {
            struct curl_slist *hs=NULL;
            hs = curl_slist_append(hs, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hs);
            curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1/api/command/save");
            /* Now specify the POST data */
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "" );
            
            /* Perform the request, res will get the return code */
            res = curl_easy_perform(curl);

            /* Check for errors */
            if (res != CURLE_OK){
                result = -1;
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            }

            /* always cleanup */
            curl_easy_cleanup(curl);
        }
        curl_global_cleanup();
}

// Receives a LoRaWAN device definition from a Direct Method callback_param
static int SetLoRaWANDevice(const unsigned char* payload)
{
    printf("SetLoRaWANDevice\n");
    printf("%s\n", payload);
    CURL *curl;
    CURLcode res;
    int result = 200;
    /* In windows, this will init the winsock stuff */
    curl_global_init(CURL_GLOBAL_ALL);

    /* get a curl handle */
    curl = curl_easy_init();
    if (curl) {
        struct curl_slist *hs=NULL;
        hs = curl_slist_append(hs, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hs);
        curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1/api/loraNetwork/whitelist/devices");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload );
        
        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);

        /* Check for errors */
        if (res != CURLE_OK)
        {
            result = -1;
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        /* always cleanup */
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    return result;
} 

static int BatchLoRaWANDevices(const unsigned char* payload)
{
    CURL *curl;
    CURLcode res;
    int result = 200;
    /* In windows, this will init the winsock stuff */
    curl_global_init(CURL_GLOBAL_ALL);

    /* get a curl handle */
    curl = curl_easy_init();
    if (curl) {
        struct curl_slist *hs=NULL;
        hs = curl_slist_append(hs, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hs);
        curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1/api/loraNetwork/whitelist");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload ); // Should be a PUT?
        
        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);

        /* Check for errors */
        if (res != CURLE_OK)
        {
            result = -1;
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        /* always cleanup */
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    return result;
}

static int ClearWhiteList(const unsigned char* payload)
{
    printf("ClearWhiteList\n");
    int result = 200;
    CURL *curl;
    CURLcode res;
    curl = curl_easy_init();
    if (curl) {
        struct curl_slist *hs=NULL;
        hs = curl_slist_append(hs, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hs);
        curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1/api/loraNetwork/whitelist/devices");
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        
        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);

        /* Check for errors */
        if (res != CURLE_OK){
            result = -1;
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }

        /* always cleanup */
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();

    return result;
}

static int DeleteDeviceSessions()
{
    printf("Shutting down lora:\n");
    system("/etc/init.d/lora-network-server stop");
    printf("Deleting session db:\n");
    system("rm /var/config/lora/lora-network-server.db");
    printf("Starting lora:\n");
    system("/etc/init.d/lora-network-server start");
}


//  Converts the Gateway object into a JSON blob with reported properties that is ready to be sent across the wire as a twin.
static char* serializeToJson(Gateway* gateway)
{
    char* result;

    JSON_Value* root_value = json_value_init_object();
    JSON_Object* root_object = json_value_get_object(root_value);

    // Only reported properties:
    (void)json_object_dotset_number(root_object, "KnownDevices", gateway->knownDevices);
    (void)json_object_dotset_number(root_object, "SoftwareVersion", gateway->softwareVersion);
    (void)json_object_dotset_number(root_object, "WhiteListStatus", gateway->WhiteListStatus);
    (void)json_object_dotset_string(root_object, "defaultAppeui", gateway->defaultAppeui);
    (void)json_object_dotset_string(root_object, "defaultAppkey", gateway->defaultAppkey);
    result = json_serialize_to_string(root_value);

    json_value_free(root_value);

    return result;
}

//  Converts the desired properties of the Device Twin JSON blob received from IoT Hub into a Gateway object.
static Gateway* parseFromJson(const char* json, DEVICE_TWIN_UPDATE_STATE update_state)
{
    Gateway* gateway = malloc(sizeof(Gateway));
    (void)memset(gateway, 0, sizeof(Gateway));

    JSON_Value* root_value = json_parse_string(json);
    JSON_Object* root_object = json_value_get_object(root_value);

    // Only desired properties:
    JSON_Value* useWhiteListOnly;
    JSON_Value* defaultAppeui;
    JSON_Value* defaultAppkey;


    if (update_state == DEVICE_TWIN_UPDATE_COMPLETE)
    {
        useWhiteListOnly = json_object_dotget_value(root_object, "desired.useWhiteListOnly");
        defaultAppeui = json_object_dotget_value(root_object, "desired.defaultAppeui");
        defaultAppkey = json_object_dotget_value(root_object, "desired.defaultAppkey");
    }
    else
    {
        useWhiteListOnly = json_object_dotget_value(root_object, "useWhiteListOnly");
        defaultAppeui = json_object_dotget_value(root_object, "defaultAppeui");
        defaultAppkey = json_object_dotget_value(root_object, "defaultAppkey");
    }

    if (useWhiteListOnly != NULL)
    {
        gateway->useWhiteListOnly = (uint8_t)json_value_get_number(useWhiteListOnly);
    }

    if (defaultAppeui != NULL)
    {
        const char* data = json_value_get_string(defaultAppeui);

        if (data != NULL)
        {
            gateway->defaultAppeui = malloc(strlen(data) + 1);
            (void)strcpy(gateway->defaultAppeui, data);
        }
    }

    if (defaultAppkey != NULL)
    {
        const char* data = json_value_get_string(defaultAppkey);

        if (data != NULL)
        {
            gateway->defaultAppkey = malloc(strlen(data) + 1);
            (void)strcpy(gateway->defaultAppkey, data);
        }
    }

    json_value_free(root_value);

    return gateway;
}

static int deviceMethodCallback(const char* method_name, const unsigned char* payload, size_t size, unsigned char** response, size_t* response_size, void* userContextCallback)
{
    (void)userContextCallback;
    (void)payload;
    (void)size;

    int result = -1;
    
    if (strcmp("SetLoRaWANDevice", method_name) == 0)
    {
        result = SetLoRaWANDevice(payload);
        // SaveLoRaWANChanges should be called by the cloud client at the end of the batch of additions
    }
    else if (strcmp("BatchLoRaWANDevices", method_name) == 0)
    {
        result = BatchLoRaWANDevices(payload);
        if(result == 200){
            result = SaveLoRaWANChanges();
            //RestartLoRaWANServer(); // Don't want to do this here because it should be done end of a batch.
        }
    }
    else if (strcmp("RestartLoRaWANServer", method_name) == 0)
    {
        result = RestartLoRaWANServer();
    }
    else if (strcmp("DeleteDeviceSessions", method_name) == 0)
    {
        result = DeleteDeviceSessions();
    }
    else if (strcmp("PostDownLink", method_name) == 0)
    {   
        result = PostDownLink(payload);
    }
    else if (strcmp("ClearWhiteList", method_name) == 0)
    {   
        result = ClearWhiteList(payload);
        if(result == 200){
            result = SaveLoRaWANChanges();
            if(result == 200){
                result = DeleteDeviceSessions();
            }
        }

    }
    else if (strcmp("SaveLoRaWANChanges", method_name) == 0)
    {   
        result = SaveLoRaWANChanges();

    }
    else
    {
        // All other entries are ignored.
        result = -1;
    }

    if(result == 200){
            const char deviceMethodResponse[] = "{ \"Response\": \"Success\" }";
            *response_size = sizeof(deviceMethodResponse)-1;
            *response = malloc(*response_size);
            (void)memcpy(*response, deviceMethodResponse, *response_size);
        }
    else
        {
            const char deviceMethodResponse[] = "{ \"Response\": \"Failed\" }";
            *response_size = sizeof(deviceMethodResponse)-1;
            *response = malloc(*response_size);
            (void)memcpy(*response, deviceMethodResponse, *response_size);    
        }

    return result;
}

static void deviceTwinCallback(DEVICE_TWIN_UPDATE_STATE update_state, const unsigned char* payLoad, size_t size, void* userContextCallback)
{
    (void)update_state;
    (void)size;

    Gateway* oldGateway = (Gateway*)userContextCallback;
    Gateway* newGateway = parseFromJson((const char*)payLoad, update_state);

    if (newGateway->useWhiteListOnly != 0)
    {
        if (newGateway->useWhiteListOnly != oldGateway->useWhiteListOnly)
        {
            printf("Received new whitelist instruction = %" PRIu8 "\n", newGateway->useWhiteListOnly);
            oldGateway->useWhiteListOnly = newGateway->useWhiteListOnly;
            // Do action against Gateway's RESTAPI and Report result in WhiteListStatus
            // if success: oldGateway->WhiteListStatus = 0;
            // if failed: oldGateway->WhiteListStatus = 1;
        }
    }

    if (newGateway->defaultAppeui != NULL)
    {
        if (oldGateway->defaultAppeui == NULL ||
            strcmp(oldGateway->defaultAppeui, newGateway->defaultAppeui) != 0)
        {
            printf("Received a new default App Eui = %s\n", newGateway->defaultAppeui);

            if (oldGateway->defaultAppeui != NULL)
            {
                free(oldGateway->defaultAppeui);
            }

            oldGateway->defaultAppeui = malloc(strlen(newGateway->defaultAppeui) + 1);
            (void)strcpy(oldGateway->defaultAppeui, newGateway->defaultAppeui);
			free(newGateway->defaultAppeui);
        }
    }

    if (newGateway->defaultAppkey != NULL)
    {
        if (oldGateway->defaultAppkey == NULL ||
            strcmp(oldGateway->defaultAppkey, newGateway->defaultAppkey) != 0)
        {
            printf("Received a new default App Eui = %s\n", newGateway->defaultAppkey);

            if (oldGateway->defaultAppkey != NULL)
            {
                free(oldGateway->defaultAppkey);
            }

            oldGateway->defaultAppkey = malloc(strlen(newGateway->defaultAppkey) + 1);
            (void)strcpy(oldGateway->defaultAppkey, newGateway->defaultAppkey);
			free(newGateway->defaultAppkey);
        }
    }


    free(newGateway);
}

static void reportedStateCallback(int status_code, void* userContextCallback)
{
    (void)userContextCallback;
    printf("Device Twin reported properties update completed with result: %d\r\n", status_code);
}


static void gateway_client_run(void)
{

    // Provisioning 
    //Provisioning();

    IOTHUB_CLIENT_TRANSPORT_PROVIDER protocol;

    // Select the Protocol to use with the connection
#ifdef SAMPLE_MQTT
    protocol = MQTT_Protocol;
#endif // SAMPLE_MQTT
#ifdef SAMPLE_MQTT_OVER_WEBSOCKETS
    protocol = MQTT_WebSocket_Protocol;
#endif // SAMPLE_MQTT_OVER_WEBSOCKETS
#ifdef SAMPLE_AMQP
    protocol = AMQP_Protocol;
#endif // SAMPLE_AMQP
#ifdef SAMPLE_AMQP_OVER_WEBSOCKETS
    protocol = AMQP_Protocol_over_WebSocketsTls;
#endif // SAMPLE_AMQP_OVER_WEBSOCKETS
#ifdef SAMPLE_HTTP
    protocol = HTTP_Protocol;
#endif // SAMPLE_HTTP

    if (IoTHub_Init() != 0)
    {
        (void)printf("Failed to initialize the platform.\r\n");
    }
    else
    {
        const char* connectionString = LoadConnectionString(); // = "HostName=IoTHubTask1.azure-devices.net;DeviceId=paulfo-gw;SharedAccessKey=R89Z4U5FTWOloehjvoSjxluPtQ/i5FcdIwrJlGrn//I="; 
        printf("Loaded CS: %s\n", connectionString);
        
        if ((iotHubClientHandle = IoTHubDeviceClient_CreateFromConnectionString(connectionString, protocol)) == NULL)
        {
            (void)printf("ERROR: iotHubClientHandle is NULL!\r\n");
        }
        else
        {
            // Uncomment the following lines to enable verbose logging (e.g., for debugging).
            //bool traceOn = true;
            //(void)IoTHubDeviceClient_SetOption(iotHubClientHandle, OPTION_LOG_TRACE, &traceOn);

#ifdef SET_TRUSTED_CERT_IN_SAMPLES
            // For mbed add the certificate information
            if (IoTHubDeviceClient_SetOption(iotHubClientHandle, "TrustedCerts", certificates) != IOTHUB_CLIENT_OK)
            {
                (void)printf("failure to set option \"TrustedCerts\"\r\n");
            }
#endif // SET_TRUSTED_CERT_IN_SAMPLES

            Gateway gateway;
            memset(&gateway, 0, sizeof(Gateway));
            gateway.softwareVersion = 0.1;
            gateway.knownDevices = 10; // Populate from Gateway REST API
            gateway.useWhiteListOnly = 0; // Populate from Gateway REST API
            gateway.WhiteListStatus= 0; // Populate from Gateway REST API

            // Populate by pulling from Gateway REST API
            gateway.defaultAppeui = "";
            gateway.defaultAppkey = "";

            char* reportedProperties = serializeToJson(&gateway);

            (void)IoTHubDeviceClient_SendReportedState(iotHubClientHandle, (const unsigned char*)reportedProperties, strlen(reportedProperties), reportedStateCallback, NULL);
            (void)IoTHubDeviceClient_SetDeviceMethodCallback(iotHubClientHandle, deviceMethodCallback, NULL);
            (void)IoTHubDeviceClient_SetDeviceTwinCallback(iotHubClientHandle, deviceTwinCallback, &gateway);


            /* Create a MQTT listener thread.  */
            if(messagingEnabled){
                if (ThreadAPI_Create(
                    &(listenerThread),
                    mqtt_worker,
                    0) != THREADAPI_OK)
                {
                    printf("ThreadAPI_Create failed");
                    listenerThread = NULL;
                }
            }

            int milisec = 250; // length of time to sleep, in miliseconds
            struct timespec req = {0};
            req.tv_sec = 0;
            req.tv_nsec = milisec * 1000000L;
            
            while(!g_Cancel){
                nanosleep(&req, (struct timespec *)NULL);
            }

            IoTHubDeviceClient_Destroy(iotHubClientHandle);
            free(reportedProperties);
        }

        IoTHub_Deinit();
    }
}

int main(void)
{
    printf("Awaiting startup\n");
    struct timespec req = {0};
    req.tv_sec = 60;
    req.tv_nsec = 0 ;
    //nanosleep(&req, (struct timespec *)NULL);

    gateway_client_run();
    return 0;
}
