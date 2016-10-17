// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>

#include "rabbitmq.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "message.h"
#include "messageproperties.h"
#include "parson.h"
#include "module.h"
#include "broker.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

#define MSG_MAX_LENGTH 512
typedef struct {
	BROKER_HANDLE	    broker;
    THREAD_HANDLE       DeviceThread;
    unsigned int        DeviceRunning : 1;	
	pid_t child_pid;
	int client_fd;
	char* recv_buf;
	char* send_buf;
}RabbitMQ_Data;

static void RabbitMQ_Receive(MODULE_HANDLE moduleHandle, MESSAGE_HANDLE messageHandle)
{
	char buf[16];
	RabbitMQ_Data * mq = (RabbitMQ_Data*)moduleHandle;	
	CONSTMAP_HANDLE properties = Message_GetProperties(messageHandle);
	//const char * source = ConstMap_GetValue(properties, GW_SOURCE_PROPERTY);
	
	const char* deviceId = ConstMap_GetValue(properties, GW_DEVICENAME_PROPERTY);
	if(deviceId == NULL) return;
	
	const CONSTBUFFER * message_content = Message_GetContent(messageHandle);
	if(message_content == NULL) {
		LogError("message_content is null");
		return;
	}
	const char* msg = (const char*)message_content->buffer;
	sprintf_s(mq->recv_buf,MSG_MAX_LENGTH,"%s,%s",deviceId,msg);
	//LogInfo("len %d device %s context %s",strlen(mq->recv_buf),deviceId,msg);
	sprintf_s(buf,sizeof(buf),"%5d",strlen(mq->recv_buf));
	write(mq->client_fd,buf,5);
	write(mq->client_fd,mq->recv_buf,strlen(mq->recv_buf));	
	ThreadAPI_Sleep(0);
}

static void freeRabbitMQ_Data(RabbitMQ_Data* data)
{
	if(data->child_pid != 0){
		if(data->client_fd==0){
			LogInfo("Send SIGTERM to pid %d",data->child_pid);
			kill(data->child_pid,SIGTERM);
		}else{
			close(data->client_fd);
			data->client_fd = 0;
		}
		int status;
		pid_t ret_pid;
		for(int i=0;i<30;++i){ // max 3 sec
			ret_pid = waitpid(data->child_pid,&status,WNOHANG);
			if(ret_pid<0 || ret_pid==data->child_pid){
				break;
			}
			ThreadAPI_Sleep(100);
		}
	}
	if(data->client_fd) close(data->client_fd);
	if(data->recv_buf) free(data->recv_buf);
	if(data->send_buf) free(data->send_buf);
}
static void RabbitMQ_Destroy(MODULE_HANDLE moduleHandle)
{
	int result;
	RabbitMQ_Data* module_data = (RabbitMQ_Data*)moduleHandle;

	if (moduleHandle == NULL)
	{
		LogError("Attempt to destroy NULL module");
		return;
	}
	module_data->DeviceRunning = 0;
	ThreadAPI_Join(module_data->DeviceThread, &result);
	freeRabbitMQ_Data(module_data);
	free(module_data);
}
static void send_message(RabbitMQ_Data* module_data,char *buf)
{

	char* deviceName = buf;
	char *value = buf;
	while(*value!=0) {
		if(*value != ','){
			++value;
			continue;
		}
		*value = 0;
		++value;
		break;
	}
	if(*value == 0) return;

	MESSAGE_CONFIG newMessageCfg;
	MAP_HANDLE newProperties = Map_Create(NULL);
	if (newProperties == NULL) {
		LogError("Failed to create message properties");
		goto EXIT;
	}
	if (Map_Add(newProperties, GW_SOURCE_PROPERTY, GW_IOTHUB_MODULE ) != MAP_OK) {
		LogError("Failed to set source property");
		goto EXIT;
	}
	if (Map_Add(newProperties, GW_DEVICENAME_PROPERTY, deviceName) != MAP_OK) {
		LogError("Failed to set source property");
		goto EXIT;	
	}
	newMessageCfg.sourceProperties = newProperties;
	strcpy_s(module_data->send_buf,MSG_MAX_LENGTH,value);
	newMessageCfg.size = strlen(module_data->send_buf);
	newMessageCfg.source = (const unsigned char*)module_data->send_buf;
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

static int RabbitMQ_worker(void * user_data)
{
	RabbitMQ_Data* rabbitmq_data = (RabbitMQ_Data*) user_data;
	struct timeval tv;
	fd_set readfds;
	int ret;
	char buf[128];
	int fd = rabbitmq_data->client_fd;
	
	while(rabbitmq_data->DeviceRunning) {
		tv.tv_sec = 0;
		tv.tv_usec = 1000;
		FD_ZERO(&readfds);
		FD_SET(fd,&readfds);
		ret = select(fd+1,&readfds,NULL,NULL,&tv);
		if(ret == -1) break;
		if(ret == 0 ) continue;
		if(!FD_ISSET(fd,&readfds)) continue;
		memset(buf,0,sizeof(buf));
		ret = read(fd,buf,sizeof(buf));
		if(ret<=0){
			LogError("RabbitMQ_worker client close unix socket");
			break;
		}
		send_message(rabbitmq_data,buf);

	}
	return 0;
}
static pid_t create_proc(RabbitMQ_Config* config,char* tmpfilename)
{
	pid_t child_pid = -1;

	char* argv[7];
	memset(argv,0,sizeof(argv));
	if(mallocAndStrcpy_s(&argv[0],config->backend)!=0) goto ERROR;
	if(mallocAndStrcpy_s(&argv[1],config->serverName)!=0) goto ERROR;
	if(mallocAndStrcpy_s(&argv[2],config->vhost)!=0) goto ERROR;
	if(mallocAndStrcpy_s(&argv[3],config->userName)!=0) goto ERROR;
	if(mallocAndStrcpy_s(&argv[4],config->passwd)!=0) goto ERROR;
	argv[5] = tmpfilename;
	argv[6] = 0;
	
	child_pid = fork();
	if(child_pid == 0 ){ 
		if(execvp(argv[0],argv)==-1){
			LogError("Unable to load executable %s",argv[0]);
			exit(EXIT_FAILURE);
		}
	}
ERROR:
	if(argv[0]) free(argv[0]);
	if(argv[1]) free(argv[1]);
	if(argv[2]) free(argv[2]);
	if(argv[3]) free(argv[3]);
	if(argv[4]) free(argv[4]);
	return child_pid; // NAVER REACH
}

static bool run_backend(RabbitMQ_Data *data,RabbitMQ_Config *config)
{
	char tempfile[]="/tmp/rabbitmq-clientXXXXXX";
	int  fd = mkstemp(tempfile);
	close(fd);
	fd = -1;
	pid_t child_pid = create_proc(config,tempfile);
	if(child_pid < 0) return false;
	data->child_pid = child_pid;

	ThreadAPI_Sleep(1000);
	struct sockaddr_un addr;
	if((fd = socket(AF_UNIX,SOCK_STREAM,0))==-1){
		LogError("unable open Unix socket");
		return false;
	}
	memset(&addr,0,sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path,tempfile,sizeof(addr.sun_path)-1);
	for(int i=0;i<3;++i){
		if(connect(fd,(struct sockaddr*)&addr,sizeof(addr))==-1){
			ThreadAPI_Sleep(500);
		}else{
			break;
		}
	}
	data->client_fd = fd;
	return true;
ERROR:
	if(fd>0) close(fd);
	return false;
}

static MODULE_HANDLE RabbitMQ_Create(BROKER_HANDLE broker, const void* configuration)
{
	RabbitMQ_Data *result = NULL;
	RabbitMQ_Config *config = (RabbitMQ_Config*) configuration;
	
	if (broker == NULL || configuration == NULL)
    {
		LogError("invalid RabbitMQ module args.");
        return NULL;
    }
	result = (RabbitMQ_Data*)malloc(sizeof(RabbitMQ_Data));
	if (result == NULL) {
		LogError("couldn't allocate memory for RabbitMQ Module");
		return NULL;
    }
	memset(result,0,sizeof(RabbitMQ_Data));
	result->recv_buf = (char*) malloc(MSG_MAX_LENGTH);
	result->send_buf = (char*) malloc(MSG_MAX_LENGTH);
	if(result->recv_buf == NULL || result->send_buf == NULL) goto EXIT;

	result->broker = broker;
	result->DeviceRunning = 1;
	if(!run_backend(result,config)){
		LogError("Can't run backend");
		goto EXIT;
	}
	if (ThreadAPI_Create(&(result->DeviceThread),RabbitMQ_worker,(void*)result) != THREADAPI_OK) {
		LogError("ThreadAPI_Create failed");
		goto EXIT;
	}
	return result;	
EXIT:
	if(result) freeRabbitMQ_Data(result);
	free(result);
	return NULL;
}

/*
 *	Required for all modules:  the public API and the designated implementation functions.
 */
static const MODULE_APIS RabbitMQ_APIS_all =
{
	RabbitMQ_Create,
	RabbitMQ_Destroy,
	RabbitMQ_Receive
};

#ifdef BUILD_MODULE_TYPE_STATIC
MODULE_EXPORT void MODULE_STATIC_GETAPIS(RABBITMQ_MODULE)(MODULE_APIS* apis)
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
		(*apis) = RabbitMQ_APIS_all;
	}

}
