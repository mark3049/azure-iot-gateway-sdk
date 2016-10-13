// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef NODEJS_H
#define NODEJS_H

#include "module.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct NODEJS_MODULE_CONFIG_TAG
{
    const char* main_path;
    const char* configuration_json;
}NODEJS_MODULE_CONFIG;

MODULE_EXPORT void MODULE_STATIC_GETAPIS(NODEJS_MODULE)(MODULE_APIS* apis);

#ifdef __cplusplus
}
#endif

#endif /*NODEJS_H*/
