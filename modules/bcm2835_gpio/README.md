Raspberry Pi GPIO module 

##Feature
* Report GPIO Status (Input Pin)
* Set GPIO from Cloud (Output Pin)
* Output Voice To Audio Jack (TTS)

##Module Json Configure
More detail please see the [gpio.json](gpio.json) file.
 
##run example

```
cd ./build
./samples/simulated_device_cloud_upload/simulated_device_cloud_upload_sample \
	../modules/bcm2835_gpio/gpio.json
```
##GPIO Output command format

Example:
```
{"Latch":0}
```

##Text to Speech function

This module use [Azure Cognitive Services Speech API](https://www.microsoft.com/cognitive-services/en-us/speech-api)
to conver Text to voice.

### Pre-request
Before use this function, you need to get a api key. please ref azure web to get one, replase [gpio.json - azure-cs-key](gpio.json)

### TTS command format

example:
```
{"lang":1,"msg":"text to speech"}
```

Lang code : [lang code](azure_tts.py) 
