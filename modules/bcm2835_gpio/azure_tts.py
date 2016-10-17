#!/usr/bin/env python3
import sys
import http.client, urllib.parse, json
import pyaudio, wave

#AccessTokenUri = "https://api.cognitive.microsoft.com/sts/v1.0/issueToken";
support_local=[
	("ar-EG",	"Female"	,"(ar-EG, Hoda)"			),	#0
	("de-DE", 	"Female"	,"(de-DE, Hedda)"			),	#1
	("de-DE", 	"Male" 		,"(de-DE, Stefan, Apollo)"	),	#2
	("en-AU", 	"Female" 	,"(en-AU, Catherine)"		),	#3
	("en-CA", 	"Female" 	,"(en-CA, Linda)"			),	#4
	("en-GB", 	"Female" 	,"(en-GB, Susan, Apollo)"	),	#5
	("en-GB", 	"Male" 		,"(en-GB, George, Apollo)"	),	#6
	("en-IN", 	"Male" 		,"(en-IN, Ravi, Apollo)"	),	#7
	("en-US", 	"Female" 	,"(en-US, ZiraRUS)"			),	#8
	("en-US", 	"Male" 		,"(en-US, BenjaminRUS)"		),	#9
	("es-ES", 	"Female" 	,"(es-ES, Laura, Apollo)"	),	#10
	("es-ES", 	"Male" 		,"(es-ES, Pablo, Apollo)"	),	#11
	("es-MX", 	"Male" 		,"(es-MX, Raul, Apollo)"	),	#12
	("fr-CA", 	"Female" 	,"(fr-CA, Caroline)"		),	#13
	("fr-FR", 	"Female" 	,"(fr-FR, Julie, Apollo)"	),	#14
	("fr-FR", 	"Male" 		,"(fr-FR, Paul, Apollo)"	),	#15
	("it-IT", 	"Male" 		,"(it-IT, Cosimo, Apollo)"	),	#16
	("ja-JP", 	"Female" 	,"(ja-JP, Ayumi, Apollo)"	),	#17
	("ja-JP", 	"Male" 		,"(ja-JP, Ichiro, Apollo)"	),	#18
	("pt-BR", 	"Male" 		,"(pt-BR, Daniel, Apollo)"	),	#19
	("ru-RU", 	"Female" 	,"(pt-BR, Daniel, Apollo)"	),	#20
	("ru-RU", 	"Male" 		,"(ru-RU, Pavel, Apollo)"	),	#21
	("zh-CN", 	"Female" 	,"(zh-CN, HuihuiRUS)"		),	#22
	("zh-CN", 	"Female" 	,"(zh-CN, Yaoyao, Apollo)"	),	#23
	("zh-CN", 	"Male" 		,"(zh-CN, Kangkang, Apollo)"),	#24
	("zh-HK", 	"Female" 	,"(zh-HK, Tracy, Apollo)"	),	#25
	("zh-HK", 	"Male" 		,"(zh-HK, Danny, Apollo)"	),	#26
	("zh-TW", 	"Female" 	,"(zh-TW, Yating, Apollo)"	),	#27
	("zh-TW", 	"Male" 		,"(zh-TW, Zhiwei, Apollo)"	)	#28
]

def gettoken(apiKey):
	params = ""
	path = "/sts/v1.0/issueToken"
	headers = {"Ocp-Apim-Subscription-Key": apiKey}

	AccessTokenHost = "api.cognitive.microsoft.com"
	conn = http.client.HTTPSConnection(AccessTokenHost)
	conn.request("POST", path, params, headers)
	response = conn.getresponse()
	#print(response.status, response.reason)
	data = response.read()
	conn.close()
	accesstoken = data.decode("UTF-8")
	#print ("Access Token: " + accesstoken)
	return accesstoken

def getWav(token,lang,msg):
	
	bodyformat = "<speak version='1.0' xml:lang='en-us'>"\
		"<voice xml:lang='{LOCALE}' xml:gender='{GENDER}'"\
		" name='Microsoft Server Speech Text to Speech Voice {MAPPING}'>"\
		"{MSG}</voice></speak>"

	headers = {"Content-type": "application/ssml+xml", 
			"X-Microsoft-OutputFormat": "riff-16khz-16bit-mono-pcm", 
			"Authorization": "Bearer " + token, 
			"X-Search-AppId": "07D3234E49CE426DAA29772419F436CA", 
			"X-Search-ClientID": "1ECFAE91408841A480F00935DC390960", 
			"User-Agent": "TTSForPython"}
	value = support_local[lang]

	body = bodyformat.format(LOCALE=value[0],GENDER=value[1],MAPPING=value[2],MSG=msg)
			
	#Connect to server to synthesize the wave
	#print ("\nConnect to server to synthesize the wave")
	conn = http.client.HTTPSConnection("speech.platform.bing.com")
	conn.request("POST", "/synthesize", body, headers)
	response = conn.getresponse()
	#print(response.status, response.reason)

	data = response.read()
	conn.close()
	return data

def playWav(data):
	p = pyaudio.PyAudio()	
	stream = p.open(format=p.get_format_from_width(2),channels=1,rate=16000,output=True)
	stream.write(data)
	stream.stop_stream()
	stream.close()
	p.terminate()
	
if __name__ == '__main__':
	if len(sys.argv)<3:
		print("Play TTS:\n\nUsage: %s api_key langcode message"%sys.argv[0]);
		sys.exit(-1)
	if int(sys.argv[2])>=len(support_local)	:
		print("lang over support %d" % len(support_local))
		sys.exit(-1)
		
	apiKey = sys.argv[1]
	token = gettoken(apiKey)
	data = getWav(token,int(sys.argv[2]),sys.argv[3])
	playWav(data)
	
	#print("The synthesized wave length: %d" %(len(data)))
	
	
