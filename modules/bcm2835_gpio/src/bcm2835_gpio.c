// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>


#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "message.h"
#include "messageproperties.h"
#include "parson.h"
#include "module.h"
#include "broker.h"

#include "bcm2835_gpio.h"
#include "bcm2835_hw.h"

#define MSG_CONTEXT_LENGTH 512

typedef enum {
	INPUT,
	OUTPUT
} GPIO_PIN_DIRECTION;

typedef struct PIGPIO_DATA_TAG
{
	BROKER_HANDLE	    broker;
    THREAD_HANDLE       gpioDeviceThread;
    unsigned int        gpioDeviceRunning : 1;
	unsigned int 		period;
	char*				deviceId;
	char*				deviceKey;
	int				tts;
	char*				azure_cs_key; //for TTS
	int					pin_count;
	GPIO_PIN_CONFIG*	pins;
	int *				pin_value;
	char* 				msg_context;
} PIGPIO_DATA;

static int read_gpio(int pin)
{
	return bcm2835_gpio_lev(pin);
}
static void write_gpio(int pin, int value)
{
	if(value > 0)
		bcm2835_gpio_set(pin);
	else
		bcm2835_gpio_clr(pin);
	
	return;
}

static void doTTS(int lang,const char* tts_msg,const char* api_key)
{
	LogInfo("doTTS %d %s",lang,tts_msg);
	char buf[80];
	sprintf_s(buf,sizeof(buf),"tts.py %s %d \"%s\"",api_key,lang,tts_msg);
	system(buf);
}

static void freePIGPIO_DATA(PIGPIO_DATA* data)
{
	int i;
	if(data == NULL) return;
	if(data->deviceId != NULL) free(data->deviceId);
	if(data->deviceKey != NULL) free(data->deviceKey);
	if(data->azure_cs_key != NULL) free(data->azure_cs_key);
	if(data->pins != NULL) free(data->pins);
	if(data->pin_value != NULL) free(data->pin_value);		
	if(data->msg_context != NULL) free(data->msg_context);
}

static void PiGpio_Receive(MODULE_HANDLE moduleHandle, MESSAGE_HANDLE messageHandle)
{
	PIGPIO_DATA* module_data = (PIGPIO_DATA*)moduleHandle;

	CONSTMAP_HANDLE properties = Message_GetProperties(messageHandle);
	const char * source = ConstMap_GetValue(properties, GW_SOURCE_PROPERTY);
	if(strcmp(source,GW_IOTHUB_MODULE)!=0){
		return;
	}

	const char* deviceId = ConstMap_GetValue(properties, GW_DEVICENAME_PROPERTY);
	if(strcmp(deviceId,module_data->deviceId) != 0){
		//LogInfo("this %s deviceId %s",module_data->deviceId,deviceId);
		return;
	}

	const CONSTBUFFER * message_content = Message_GetContent(messageHandle);
	if(message_content == NULL) {
		LogError("message_content is null");
		return;
	}
	JSON_Value* json = json_parse_string((const char*)(message_content->buffer));
	if(json == NULL){
		LogError("json_parse_string return null");
		return;
	}
	//LogInfo("this %s Content %s",module_data->deviceId,(const char*)(message_content->buffer));
	
	JSON_Object* instr = json_value_get_object(json);
	if(instr==NULL){
		LogError("json_value_get_object return null");
		json_value_free(json);
		return;
	}
	GPIO_PIN_CONFIG *pin;
	JSON_Value* p;
	for(int i=0;i<module_data->pin_count;++i){
		pin = &module_data->pins[i];
		if(pin->direction == INPUT) continue;
		p = json_object_get_value(instr, pin->name);
		if(p == NULL) continue;
		int value;
		if(json_value_get_type(p)==JSONNumber){
			value = (int)json_value_get_number(p);
		}else if(json_value_get_type(p)== JSONString){
			value = atoi(json_value_get_string(p));
		}
		write_gpio(pin->pin,value);
	}
	if(module_data->tts){
		int lang = (int) json_object_get_number(instr,"lang");
		const char *tts = json_object_get_string(instr,"msg");
		if(tts != NULL){
			doTTS(lang,tts,module_data->azure_cs_key);
		}
	}
	json_value_free(json);
    return;
}

static void PiGpio_Destroy(MODULE_HANDLE moduleHandle)
{
	if (moduleHandle == NULL)
	{
		LogError("Attempt to destroy NULL module");
	}
	else
	{
		PIGPIO_DATA* module_data = (PIGPIO_DATA*)moduleHandle;
		int result;

		/* Tell thread to stop */
		module_data->gpioDeviceRunning = 0;
		/* join the thread */
		ThreadAPI_Join(module_data->gpioDeviceThread, &result);
		/* free module data */
		freePIGPIO_DATA(module_data);
		free(module_data);
	}
}
static void buildMesssageContext(PIGPIO_DATA* module_data)
{
	char buf[80];
	char *pDest = module_data->msg_context;
	char *pStar = module_data->msg_context;

	memset(pDest,0,MSG_CONTEXT_LENGTH);
	*pDest = '{';
	++pDest;
	int n;
	for(int i=0;i<module_data->pin_count;++i){
		if(i!=0){
			*pDest=',';
			++pDest;
		}
		n = sprintf_s(pDest,MSG_CONTEXT_LENGTH-(pDest-pStar),"\"%s\": %d",module_data->pins[i].name,module_data->pin_value[i]);
		if(n<=0) break;
		pDest+=n;
	}
	*pDest = '}';

}
static void send_message(PIGPIO_DATA* module_data,int value)
{

	MESSAGE_CONFIG newMessageCfg;
	MAP_HANDLE newProperties = Map_Create(NULL);
	if (newProperties == NULL) {
		LogError("Failed to create message properties");
		goto EXIT;
	}
	if (Map_Add(newProperties, GW_SOURCE_PROPERTY, GW_IDMAP_MODULE) != MAP_OK) {
		LogError("Failed to set source property");
		goto EXIT;
	}
	if (Map_Add(newProperties, GW_DEVICENAME_PROPERTY, module_data->deviceId) != MAP_OK) {
		LogError("Failed to set source property");
		goto EXIT;	
	}
	if (Map_Add(newProperties, GW_DEVICEKEY_PROPERTY, module_data->deviceKey) != MAP_OK) {
		LogError("Failed to set source property");
		goto EXIT;	
	}
	newMessageCfg.sourceProperties = newProperties;
	
	buildMesssageContext(module_data);

	newMessageCfg.size = strlen(module_data->msg_context);
	newMessageCfg.source = (const unsigned char*)module_data->msg_context;
	MESSAGE_HANDLE newMessage = Message_Create(&newMessageCfg);
	if (newMessage == NULL) {
		LogError("Failed to create new message");
		goto EXIT;
	}
	if (Broker_Publish(module_data->broker, (MODULE_HANDLE)module_data, newMessage) != BROKER_OK) {
		LogError("Failed to create new message");
	}
	Message_Destroy(newMessage);
	
EXIT:	
	if(newProperties != NULL) Map_Destroy(newProperties);
}

static bool update_gpio_statue(PIGPIO_DATA* data)
{
	int count = data->pin_count;
	bool change = false;
	int cur;
	for(int i=0;i<count;++i){
		cur = read_gpio(data->pins[i].pin);
		if(cur != data->pin_value[i])
		{
			change = true;
			data->pin_value[i] = cur;			
		}
	}	
	return change;
}
static int pi_gpio_worker(void * user_data)
{
    PIGPIO_DATA* module_data = (PIGPIO_DATA*)user_data;
	int value = -1;
	int curr;
	int sleep = module_data->period*10;
	if (user_data == NULL){
		LogError("user_data is null");
		return 0;
	}
	
	if(bcm2835_init()==0){
		LogInfo("Can't init raspbarry pi GPIO");
	}
	// Write Preset Value
	for(int i=0;i<module_data->pin_count;++i){
		if(module_data->pins[i].direction == OUTPUT){
			bcm2835_gpio_fsel(module_data->pins[i].pin,BCM2835_GPIO_FSEL_OUTP);
			write_gpio(module_data->pins[i].pin,module_data->pins[i].preset_value);
		}else{
			bcm2835_gpio_fsel(module_data->pins[i].pin,BCM2835_GPIO_FSEL_INPT);
		}
	}
	for(int i=0;i<5;++i){
		if(module_data->gpioDeviceRunning==0) break;
		ThreadAPI_Sleep(1000);
	}
	bool changed = false;
	int loop_count = 0;

	while (module_data->gpioDeviceRunning) {
		changed |= update_gpio_statue(module_data);
		if(++loop_count>sleep) changed = true;
		if(changed){
			send_message(module_data,value);
			loop_count = 0;
			changed = false;
		}
		ThreadAPI_Sleep(100);
	}
	bcm2835_close();
	return 0;
}

static MODULE_HANDLE PiGpio_Create(BROKER_HANDLE broker, const void* configuration)
{
    PIGPIO_DATA * result = NULL;
	const PI_GPIO_CONFIG *config = (const PI_GPIO_CONFIG*) configuration;
    if (broker == NULL || configuration == NULL)
    {
		LogError("invalid Pi_GPIO module args.");
        return NULL;
    }
	/* allocate module data struct */
	result = (PIGPIO_DATA*)malloc(sizeof(PIGPIO_DATA));
	if (result == NULL) {
		LogError("couldn't allocate memory for Pi_GPIO Module");
		return NULL;
    }
	memset(result,0,sizeof(PIGPIO_DATA));
	/* save the message broker */
	result->broker = broker;
	/* set module is running to true */
	result->gpioDeviceRunning = 1;
	/* save fake MacAddress */

	if( mallocAndStrcpy_s(&result->deviceId,config->deviceId) != 0){
		LogError("configure did not copy");
		goto EXIT;
	}
	if( mallocAndStrcpy_s(&result->deviceKey,config->deviceKey) != 0){
		LogError("configure did not copy");
		goto EXIT;
	}
	result->tts = config->tts;
	if(result->tts == true){
		if( mallocAndStrcpy_s(&result->azure_cs_key,config->azure_cs_ttskey) != 0){
			LogError("configure did not copy");
			goto EXIT;
		}
	}
	if(config->period <= 0) result->period = 15*60; // 15 min
	else result->period = config->period;
	
	result->pin_count = VECTOR_size(config->pins);
	result->pins = (GPIO_PIN_CONFIG*) malloc(sizeof(GPIO_PIN_CONFIG)*result->pin_count);
	result->pin_value = (int*) malloc(sizeof(int)*result->pin_count);
	result->msg_context = (char*) malloc(sizeof(char)*MSG_CONTEXT_LENGTH);
	if(result->pins == NULL || result->pin_value == NULL || result->msg_context == NULL){
		LogError("configure did not copy");
		goto EXIT;
	}
	memset(result->pins,0,sizeof(GPIO_PIN_CONFIG)*result->pin_count);

	GPIO_PIN_CONFIG *p_gpio;
	for(int i=0;i<result->pin_count;++i){
		p_gpio = (GPIO_PIN_CONFIG*) VECTOR_element(config->pins,i);
		memcpy(&result->pins[i],p_gpio,sizeof(GPIO_PIN_CONFIG));
		result->pin_value[i] = -1;
		//LogInfo("Add GPIO Pin %s %d %d",p_gpio->name,p_gpio->pin,p_gpio->direction);
	}
	return result;
EXIT:
	if(result) freePIGPIO_DATA(result);
	free(result);
	return NULL;
}

static void PiGpio_Start(MODULE_HANDLE moduleHandle)
{
	if (moduleHandle == NULL)
	{
		LogError("Attempt to start NULL module");
	}
	else
	{
		PIGPIO_DATA* module_data = (PIGPIO_DATA*)moduleHandle;
		/* OK to start */
		/* Create a fake data thread.  */
		
		/* OK to start */
		/* Create a fake data thread.  */
		if (ThreadAPI_Create(&(module_data->gpioDeviceThread),pi_gpio_worker,(void*)module_data) != THREADAPI_OK) {
			LogError("ThreadAPI_Create failed");
			module_data->gpioDeviceThread = NULL;
		}
		else
		{
			/* Thread started, module created, all complete.*/
		}
	}

}

/*
 *	Required for all modules:  the public API and the designated implementation functions.
 */
static const MODULE_APIS PiGpio_APIS_all =
{
	PiGpio_Create,
	PiGpio_Destroy,
	PiGpio_Receive,
	PiGpio_Start
};

#ifdef BUILD_MODULE_TYPE_STATIC
MODULE_EXPORT void MODULE_STATIC_GETAPIS(PI_GPIO_MODULE)(MODULE_APIS* apis)
#else
MODULE_EXPORT void Module_GetAPIS(MODULE_APIS* apis)
#endif
{
	if (!apis)
	{
		LogError("NULL passed to Module_GetAPIS");
	}
	else
	{
		(*apis) = PiGpio_APIS_all;
	}
}
