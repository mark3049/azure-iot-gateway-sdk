LOGGER MODULE
===========

High level design
-----------------

### Overview

This module logs all the received traffic. The module has no filtering, so it logs everything into a file. The file contains a JSON object. THe JSON object 
is an array of individual JSON values. There are 2 types of such JSON values: markers for being/end of logging and effective log data.

####Additional data types
```c
typedef enum LOGGER_TYPE_TAG
{
    LOGGING_TO_FILE
}LOGGER_TYPE;

typedef struct LOGGER_CONFIG_TAG
{
    LOGGER_TYPE selector;
    union 
    {
        struct LOGGER_CONFIG_FILE_TAG
        {
            const char* name;
        } loggerConfigFile;
    }selectee;
}LOGGER_CONFIG;
```

###Logger_CreateFromJson
```c
MODULE_HANDLE Logger_CreateFromJson(BROKER_HANDLE broker, const char* configuration);
```
Creates a new LOGGER MODULE instance. `configuration` is a pointer to a const char* that contains a json object as supplied by `Gateway_CreateFromJson`.
The json object should contain: 
```json
{
    "filename": "path/to/outputfile"
}
``` 

Example:
The following Gateway config file describes a module named "logger" that is an instance of logger.dll. It instructs the logger to output messages to the file deviceCloudUploadGatewaylog.txt.
```json
{
    "modules" :
    [ 
        {
            "module name" : "logger",
            "module path" : "logger.dll",
            "args" : 
            {
                "filename":"deviceCloudUploadGatewaylog.txt"}"
            }
        }
   ]
}```


**SRS_LOGGER_05_001: [** If `broker` is NULL then `Logger_CreateFromJson` shall fail and return NULL. **]**
**SRS_LOGGER_05_003: [** If `configuration` is NULL then `Logger_CreateFromJson` shall fail and return NULL. **]**
**SRS_LOGGER_05_011: [** If configuration is not a JSON object, then `Logger_CreateFromJson` shall fail and return NULL. **]**
**SRS_LOGGER_05_012: [** If the JSON object does not contain a value named "filename" then `Logger_CreateFromJson` shall fail and return NULL. **]**
**SRS_LOGGER_05_005: [** `Logger_CreateFromJson` shall pass `broker` and the filename to `Logger_Create`. **]**
**SRS_LOGGER_05_006: [** If `Logger_Create` succeeds then `Logger_CreateFromJson` shall succeed and return a non-NULL value. **]**
**SRS_LOGGER_05_007: [** If `Logger_Create` fails then `Logger_CreateFromJson` shall fail and return NULL. **]**


###Logger_Create
```c
MODULE_HANDLE Logger_Create(BROKER_HANDLE broker, const void* configuration);
```
Creates a new LOGGER instance. `configuration` is a pointer to a `LOGGER_CONFIG`.

**SRS_LOGGER_02_001: [**If broker is NULL then Logger_Create shall fail and return NULL.**]**
**SRS_LOGGER_02_002: [**If configuration is NULL then Logger_Create shall fail and return NULL.**]**
**SRS_LOGGER_02_003: [**If configuration->selector has a value different than `LOGGING_TO_FILE` then Logger_Create shall fail and return NULL.**]**
**SRS_LOGGER_02_004: [**If configuration->selectee.loggerConfigFile.name is NULL then Logger_Create shall fail and return NULL.**]**

**SRS_LOGGER_02_005: [**Logger_Create shall allocate memory for the below structure.**]**

```c
typedef LOGGER_HANDLE_DATA_TAG
{
    FILE* fout;
}LOGGER_HANDLE_DATA;
```
**SRS_LOGGER_02_020: [**If the file selectee.loggerConfigFile.name does not exist, it shall be created.**]**
**SRS_LOGGER_02_021: [**If creating selectee.loggerConfigFile.name fails then Logger_Create shall fail and return NULL.**]**

**SRS_LOGGER_02_006: [**Logger_Create shall open the file configuration the filename selectee.loggerConfigFile.name in update (reading and writing) mode and assign the result of fopen
to fout field.
**]**
**SRS_LOGGER_02_017: [**Logger_Create shall add the following JSON value to the existing array of JSON values in the file:**]**
```json
{
    "time":"timeAsPrinted by strftime",
    "content": "Log started"
}
```
**SRS_LOGGER_02_018: [**If the file does not contain a JSON array, then it shall create it.**]**

**SRS_LOGGER_02_007: [**If Logger_Create encounters any errors while creating the LOGGER_HANDLE_DATA then it shall fail and return NULL.**]**

**SRS_LOGGER_02_008: [**Otherwise Logger_Create shall return a non-NULL pointer.**]**

###Logger_Receive
```c
void Logger_Receive(MODULE_HANDLE moduleHandle, MESSAGE_HANDLE messageHandle);
```

**SRS_LOGGER_02_009: [**If moduleHandle is NULL then Logger_Receive shall fail and return.**]**
**SRS_LOGGER_02_010: [**If messageHandle is NULL then Logger_Receive shall fail and return.**]**
**SRS_LOGGER_02_011: [**Logger_Receive shall write in the fout FILE the following information in JSON format:**]**
```json
[
    {
        "received":"timeAsPrintedBy strftime"
        "properties":
        {
            "property1":"value1",
            "property2":"value2"
        },
        "content":"base64 encode of the message content"
    },
    {
        //next message here.
    }
]    
```

**SRS_LOGGER_02_012: [**If producing the JSON format or writing it to the file fails, then Logger_Receive shall fail and return.**]**

**SRS_LOGGER_02_013: [**Logger_Receive shall return.**]**


###Logger_Destroy
```c
void Logger_Destroy(MODULE_HANDLE moduleHandle);
```
**SRS_LOGGER_02_014: [**If moduleHandle is NULL then Logger_Destroy shall return.**]**
**SRS_LOGGER_02_019: [**Logger_Destroy shall add to the log file the following end of log JSON object:**]**
```json
{
    "time":"timeAsPrinted by ctime",
    "content": "Log stopped"
}
```
**SRS_LOGGER_02_015: [**Otherwise Logger_Destroy shall unuse all used resources.**]**


###Module_GetAPIs
```c
extern const MODULE_APIS* Module_GetAPIS(void);
```

**SRS_LOGGER_26_001: [** `Module_GetAPIS` shall fill the provided `MODULE_APIS` function with the required function pointers. **]**
