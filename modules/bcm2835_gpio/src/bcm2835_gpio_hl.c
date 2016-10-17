// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>

#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "message.h"
#include "messageproperties.h"

#include "module.h"
#include "broker.h"
#include "parson.h"

#include "bcm2835_gpio_hl.h"
#include "bcm2835_gpio.h"

static void PiGpio_HL_Start(MODULE_HANDLE moduleHandle)
{
	MODULE_APIS apis;
	MODULE_STATIC_GETAPIS(PI_GPIO_MODULE)(&apis);
	if (apis.Module_Start != NULL)
	{
		apis.Module_Start(moduleHandle);
	}
}

static void PiGpio_HL_Receive(MODULE_HANDLE moduleHandle, MESSAGE_HANDLE messageHandle)
{
	MODULE_APIS apis;
	MODULE_STATIC_GETAPIS(PI_GPIO_MODULE)(&apis);
	apis.Module_Receive(moduleHandle, messageHandle);
}

static void PiGpio_HL_Destroy(MODULE_HANDLE moduleHandle)
{
	MODULE_APIS apis;
	MODULE_STATIC_GETAPIS(PI_GPIO_MODULE)(&apis);
	apis.Module_Destroy(moduleHandle);	
}
static const char* unableToJsonObjectGetString = "unable to json_object_get_string [%s]";

static bool addRecord(VECTOR_HANDLE inputVector, JSON_Object * record)
{
	if(record == NULL) return false;
	GPIO_PIN_CONFIG config;
	memset(&config,0,sizeof(config));
	
	const char* p;
	if( (p = json_object_get_string(record,"Name")) == NULL) { 
		LogError(unableToJsonObjectGetString,"Name");
		return false;
	}
	if(strlen(p)>=sizeof(config.name)){
		LogError("Pin Name %s too long",p);
		return false;
	}
	strncpy_s(config.name,sizeof(config.name),p,sizeof(config.name));

	config.pin = json_object_get_number(record,"Pin");
	if(config.pin == 0) {
		LogError("Pin number failed");
		return false;
	}
	
	if( (p = json_object_get_string(record,"Direction")) == NULL) { 
		LogError(unableToJsonObjectGetString,"Direction");
		return false;
	}
	if(strncmp(p,"Output",6)==0) {
		config.direction= 1;
	} else if(strncmp(p,"Input",5)==0) {
		config.direction = 0;
	} else {
		LogError("Direction wrong [%s]",p);
		return false;
	}
	if(config.direction == 1) {
		config.preset_value = 0;
		if((p=json_object_get_string(record,"Preset")) != NULL){
			if(strncmp(p,"HI",2)==0){
				config.preset_value = 1;
			}else{
				LogInfo("Preset Value only support HI, but File is [%s]",p);
			}
		}
	}
	if(VECTOR_push_back(inputVector,&config,1)!=0){
		LogError("Did not push vector");
		return false;
	}	
	return true;
}
static MODULE_HANDLE PiGpio_HL_Create(BROKER_HANDLE broker, const void* configuration)
{
	MODULE_HANDLE result = NULL;
	PI_GPIO_CONFIG *config = (PI_GPIO_CONFIG*)malloc(sizeof(PI_GPIO_CONFIG));
		
	if(config == NULL) goto EXIT_CREATE;
	memset(config,0,sizeof(PI_GPIO_CONFIG));
	
    if (broker == NULL || configuration == NULL ) {
		LogError("invalid PI_GPIO_HL module args.");
		goto EXIT_CREATE;
    }
	JSON_Value* json = json_parse_string((const char*)configuration);
	if (json == NULL) {
		LogError("unable to json_parse_string");
		goto EXIT_CREATE;
    }
	JSON_Object* root = json_value_get_object(json);
	if (root == NULL) {
		LogError("unable to json_value_get_object");
		goto EXIT_CREATE;
	}
	config->deviceId = json_object_get_string(root, "deviceId");
	if (config->deviceId == NULL) {
		LogError("unable to json_object_get_string [deviceId]");
		goto EXIT_CREATE;
	}
	config->deviceKey = json_object_get_string(root,"deviceKey");
	if(config->deviceKey == NULL) {
		LogError("unable to json_object_get_string [deviceKey]");
		goto EXIT_CREATE;		
	}
	config->period = json_object_get_number(root,"period");

	config->tts= json_object_get_boolean(root,"TTS");
	if(config->tts == true){
		const char *key = json_object_get_string(root,"azure-cs-key");
		if(key == NULL){
			LogInfo("unable to json_object_get_string [azure-cs-key]");
			config->tts = false;
		}else{
			config->azure_cs_ttskey = key;
		}
	}
	
	JSON_Array* jsonArray = json_object_get_array(root,"configure");
	if(jsonArray == NULL ) {
		LogError("unable to json_object_get_array [configure]");
		goto EXIT_CREATE;
	}
	config->pins = VECTOR_create(sizeof(GPIO_PIN_CONFIG));
	if(config->pins == NULL) goto EXIT_CREATE;
	size_t numberOfRecords = json_array_get_count(jsonArray);
	bool arrayParsed = true;
	for(size_t i =0 ;i<numberOfRecords;++i) {
		if(addRecord(config->pins,json_array_get_object(jsonArray,i)) != true){
			arrayParsed = false;
			break;
		}
	}
	if(arrayParsed == true) {		
		MODULE_APIS apis;
		MODULE_STATIC_GETAPIS(PI_GPIO_MODULE)(&apis);
	    result = apis.Module_Create(broker, (void*)config);
	}
	VECTOR_destroy(config->pins);

EXIT_CREATE:
	
    if(json != NULL) json_value_free(json);
	if(config != NULL) free(config);
	return result;
}

/*
 *	Required for all modules:  the public API and the designated implementation functions.
 */
static const MODULE_APIS PiGpio_HL_APIS_all =
{
	PiGpio_HL_Create,
	PiGpio_HL_Destroy,
	PiGpio_HL_Receive,
	PiGpio_HL_Start
};

#ifdef BUILD_MODULE_TYPE_STATIC
MODULE_EXPORT void MODULE_STATIC_GETAPIS(PI_GPIO_HL_MODULE)(MODULE_APIS* apis)
#else
MODULE_EXPORT void Module_GetAPIS(MODULE_APIS* apis)
#endif
{
	if (!apis){
		LogError("NULL passed to Module_GetAPIS");
	}else{
		(*apis) = PiGpio_HL_APIS_all;
	}
}
