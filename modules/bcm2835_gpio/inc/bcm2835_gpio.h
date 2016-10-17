
#ifndef __BCM2835_GPIO_H__
#define __BCM2835_GPIO_H__

#include "module.h"
#include "azure_c_shared_utility/vector.h"

#ifdef __cplusplus
extern "C"
{
#endif
typedef struct GPIO_PIN_CONFIG_TAG
{
	char name[16];
	int pin;
	int direction;
	int preset_value;
} GPIO_PIN_CONFIG;

typedef struct PI_GPIO_CONFIG_TAG
{
	const char * deviceId;
	const char * deviceKey;
	int tts; // for TTS
	const char *azure_cs_ttskey;
	int period;
	VECTOR_HANDLE pins;
} PI_GPIO_CONFIG;
	
	MODULE_EXPORT void MODULE_STATIC_GETAPIS(PI_GPIO_MODULE)(MODULE_APIS* apis);

#ifdef __cplusplus
}
#endif

#endif /*__BCM2835_GPIO_H__*/
