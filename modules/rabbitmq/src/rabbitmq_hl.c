// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>

#include "rabbitmq.h"
#include "rabbitmq_hl.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "message.h"
#include "messageproperties.h"

#include "module.h"
#include "broker.h"
#include "parson.h"

static void RabbitMQ_HL_Start(MODULE_HANDLE moduleHandle)
{
	MODULE_APIS apis;
	MODULE_STATIC_GETAPIS(RABBITMQ_MODULE)(&apis);
	if (apis.Module_Start != NULL)
	{
		apis.Module_Start(moduleHandle);
	}
}

static void RabbitMQ_HL_Receive(MODULE_HANDLE moduleHandle, MESSAGE_HANDLE messageHandle)
{
	MODULE_APIS apis;
	MODULE_STATIC_GETAPIS(RABBITMQ_MODULE)(&apis);
	apis.Module_Receive(moduleHandle, messageHandle);
}

static void RabbitMQ_HL_Destroy(MODULE_HANDLE moduleHandle)
{
	MODULE_APIS apis;
	MODULE_STATIC_GETAPIS(RABBITMQ_MODULE)(&apis);
	apis.Module_Destroy(moduleHandle);	
}

static MODULE_HANDLE RabbitMQ_HL_Create(BROKER_HANDLE broker, const void* configuration)
{
	MODULE_HANDLE result = NULL;
	RabbitMQ_Config* config = (RabbitMQ_Config*) malloc(sizeof(RabbitMQ_Config));

	if(config==NULL){
		LogError("couldn't allocate memory for  RabbitMQ_HL Module");
		goto EXIT_CREATE;
	}
	memset(config,0,sizeof(RabbitMQ_Config));

	if (broker == NULL || configuration == NULL ) {
		LogError("invalid RabbitMQ_HL module args.");
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
	config->serverName = json_object_get_string(root, "server");
	if (config->serverName == NULL) {
		LogError("unable to json_object_get_string [server]");
		goto EXIT_CREATE;
	}
	config->vhost = json_object_get_string(root, "vhost");
	if (config->vhost == NULL) {
		LogError("unable to json_object_get_string [vhost]");
		goto EXIT_CREATE;
	}
	config->userName = json_object_get_string(root, "user_name");
	if (config->userName == NULL) {
		LogError("unable to json_object_get_string [user_name]");
		goto EXIT_CREATE;
	}
	config->passwd = json_object_get_string(root, "passwd");
	if (config->passwd == NULL) {
		LogError("unable to json_object_get_string [passwd]");
		goto EXIT_CREATE;
	}
	config->backend = json_object_get_string(root, "backend");
	if(config->backend == NULL){
		LogError("unable to json_object_get_string [backend]");
		goto EXIT_CREATE;
	}
	MODULE_APIS apis;
	MODULE_STATIC_GETAPIS(RABBITMQ_MODULE)(&apis);
	result = apis.Module_Create(broker, (void*)config);
	
EXIT_CREATE:	
	if(config) free(config);
	if(json) json_value_free(json);
	return result;
}

/*
 *	Required for all modules:  the public API and the designated implementation functions.
 */
static const MODULE_APIS RabbitMQ_HL_APIS_all =
{
	RabbitMQ_HL_Create,
	RabbitMQ_HL_Destroy,
	RabbitMQ_HL_Receive,
	RabbitMQ_HL_Start
};

#ifdef BUILD_MODULE_TYPE_STATIC
MODULE_EXPORT void MODULE_STATIC_GETAPIS(RABBITMQ_HL_MODULE)(MODULE_APIS* apis)
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
		(*apis) = RabbitMQ_HL_APIS_all;
	}

}
