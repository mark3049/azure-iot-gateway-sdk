// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef RABBITMQ_H
#define RABBITMQ_H

#include "module.h"
#include "azure_c_shared_utility/vector.h"

#ifdef __cplusplus
extern "C"
{
#endif
typedef struct RabbitMQ_Config_tag{
	const char* serverName;
	const char* vhost;
	const char* userName; 
	const char* passwd;
	const char* backend;
}RabbitMQ_Config;

	MODULE_EXPORT void MODULE_STATIC_GETAPIS(RABBITMQ_MODULE)(MODULE_APIS* apis);

#ifdef __cplusplus
}
#endif

#endif /*RABBITMQ_H*/
