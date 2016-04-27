// Copyright (c) Microsoft. All rights reserved.
// Licesendnsed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <pgmspace.h>

/* This sample uses the _LL APIs of iothub_client for example purposes.
That does not mean that HTTP only works with the _LL APIs.
Simply changing the using the convenience layer (functions not having _LL)
and removing calls to _DoWork will yield the same results. */

#include "AzureIoT.h"

static const char* connectionString = "HostName=streammonitoring.azure-devices.net;DeviceId=lyons_creek_1;SharedAccessKey=yBP2lGUEI+9nqSHT+iyrPQ==";

// switch to determine whether or not to send air temp data to cloud
bool sendAirTemp = true;

// switch to determine whether or not device info has been sent
bool deviceInfoSent = false;

// Define the Model
BEGIN_NAMESPACE(StreamStation);

DECLARE_STRUCT(DeviceProperties,
ascii_char_ptr, DeviceID,
_Bool, HubEnabledState,
ascii_char_ptr, DeviceState,
ascii_char_ptr, Manufacturer,
ascii_char_ptr, ModelNumber,
ascii_char_ptr, FirmwareVersion,
int, Latitude,
int, Longitude
);

DECLARE_MODEL(WaterSensor,

// Event data sent by the device
WITH_DATA(ascii_char_ptr, DeviceId),
WITH_DATA(int, WaterTemp),
WITH_DATA(int, AirTemp),
WITH_DATA(ascii_char_ptr, TimeStamp),

// Info about the device
WITH_DATA(ascii_char_ptr, ObjectType),
WITH_DATA(_Bool, IsSimulatedDevice),
WITH_DATA(ascii_char_ptr, Version),
WITH_DATA(DeviceProperties, DeviceProperties),
WITH_DATA(ascii_char_ptr_no_quotes, Commands),

// Commands supported by the device
WITH_ACTION(TurnOnAir),
WITH_ACTION(TurnOffAir)
);

END_NAMESPACE(StreamStation);

DEFINE_ENUM_STRINGS(IOTHUB_CLIENT_CONFIRMATION_RESULT, IOTHUB_CLIENT_CONFIRMATION_RESULT_VALUES)

EXECUTE_COMMAND_RESULT TurnOnAir(WaterSensor* device)
{
    (void)device;
    sendAirTemp = true;
    LogInfo("Turning air temp readings on.\r\n");
    return EXECUTE_COMMAND_SUCCESS;
}

EXECUTE_COMMAND_RESULT TurnOffAir(WaterSensor* device)
{
    (void)device;
    sendAirTemp = false;
    LogInfo("Turning air temp readings off.\r\n");
    return EXECUTE_COMMAND_SUCCESS;
}

// callback function which is invoked when message is sent
void sendCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void* userContextCallback)
{
    int messageTrackingId = (intptr_t)userContextCallback;

    LogInfo("Message Id: %d Received.\r\n", messageTrackingId);

    LogInfo("Result Call Back Called! Result is: %s \r\n", ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result));
}

/*this function "links" IoTHub to the serialization library*/
static IOTHUBMESSAGE_DISPOSITION_RESULT IoTHubMessage(IOTHUB_MESSAGE_HANDLE message, void* userContextCallback)
{
    IOTHUBMESSAGE_DISPOSITION_RESULT result;
    const unsigned char* buffer;
    size_t size;
    if (IoTHubMessage_GetByteArray(message, &buffer, &size) != IOTHUB_MESSAGE_OK)
    {
        LogInfo("unable to IoTHubMessage_GetByteArray\r\n");
        result = EXECUTE_COMMAND_ERROR;
    }
    else
    {
        /*buffer is not zero terminated*/
        char* temp = malloc(size + 1);
        if (temp == NULL)
        {
            LogInfo("failed to malloc\r\n");
            result = EXECUTE_COMMAND_ERROR;
        }
        else
        {
            memcpy(temp, buffer, size);
            temp[size] = '\0';
            EXECUTE_COMMAND_RESULT executeCommandResult = EXECUTE_COMMAND(userContextCallback, temp);
            result =
                (executeCommandResult == EXECUTE_COMMAND_ERROR) ? IOTHUBMESSAGE_ABANDONED :
                (executeCommandResult == EXECUTE_COMMAND_SUCCESS) ? IOTHUBMESSAGE_ACCEPTED :
                IOTHUBMESSAGE_REJECTED;
            free(temp);
        }
    }
    return result;
}

void simplesample_http_run(int air, int water, char* sampleTime)
{
  // initialize the serializer
  if (serializer_init(NULL) != SERIALIZER_OK)
  {
    LogInfo("Failed on serializer_init\r\n");
  }
  else
  {
    // allocate an IoT Hub client handle
    // using the device connection string
    // designate protocol - i.e. http
    IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString(connectionString, HTTP_Protocol);
        
    if (iotHubClientHandle == NULL)
    {
      LogInfo("Failed on IoTHubClient_LL_Create\r\n");
    }
    else
    {
      WaterSensor* myStream = CREATE_MODEL_INSTANCE(StreamStation, WaterSensor);
      if (myStream == NULL)
      {
        LogInfo("Failed on CREATE_MODEL_INSTANCE\r\n");
      }
      else
      {
        //STRING_HANDLE commandsMetadata;
        
        // register the message callback funtion for receiving messages
        if (IoTHubClient_LL_SetMessageCallback(iotHubClientHandle, IoTHubMessage, myStream) != IOTHUB_CLIENT_OK)
        {
          LogInfo("unable to IoTHubClient_SetMessageCallback\r\n");
        }
        else
        {
          // send the device info once so that the cloud app knows
          // what commands are available and the fact that the device is up
          if (!deviceInfoSent)
          {
            // define the device info data
            myStream->ObjectType = "DeviceInfo";
            myStream->IsSimulatedDevice = false;
            myStream->Version = "1.0";
            myStream->DeviceProperties.HubEnabledState = true;
            myStream->DeviceProperties.DeviceID = "lyons_creek_1";
            myStream->DeviceProperties.DeviceState = "normal";
            myStream->DeviceProperties.Manufacturer = "SparkFun";
            myStream->DeviceProperties.ModelNumber = "Thing Dev";
            myStream->DeviceProperties.FirmwareVersion = "2.1.1";
            myStream->DeviceProperties.Latitude = 0;
            myStream->DeviceProperties.Longitude = 0;
            
            unsigned char* destination;
            size_t destinationSize;

            if (SERIALIZE(&destination, &destinationSize, myStream->ObjectType, myStream->Version, myStream->IsSimulatedDevice, myStream->DeviceProperties) != IOT_AGENT_OK)
            {
              LogInfo("Failed to serialize\r\n");
            }
            else
            {
              // create the message
              IOTHUB_MESSAGE_HANDLE messageHandle = IoTHubMessage_CreateFromByteArray(destination, destinationSize);
              if (messageHandle == NULL)
              {
                LogInfo("unable to create a new IoTHubMessage\r\n");
              }
              else
              {
                // send the message
                if (IoTHubClient_LL_SendEventAsync(iotHubClientHandle, messageHandle, sendCallback, (void*)1) != IOTHUB_CLIENT_OK)
                {
                  LogInfo("failed to hand over the message to IoTHubClient\r\n");
                }
                else
                {
                  LogInfo("IoTHubClient accepted the message for delivery\r\n");
                  deviceInfoSent = true;
                }

                IoTHubMessage_Destroy(messageHandle);
              }
              free(destination);          
            }
          }
          
          // define the event data of the message
          myStream->DeviceId = "lyons_creek_1";
          myStream->AirTemp = air;
          myStream->WaterTemp = water;
          myStream->TimeStamp = sampleTime;
          {
            //serialize the data
            unsigned char* destination;
            size_t destinationSize;
            IOT_AGENT_RESULT serializeStatus;

            // check if device should send air temp
            if (sendAirTemp)
            {
              // include air temp in the message
              serializeStatus = SERIALIZE(&destination, &destinationSize, myStream->DeviceId, myStream->AirTemp, myStream->WaterTemp, myStream-> TimeStamp);
            }
            else
            {
              // do not include air temp in the message
              serializeStatus = SERIALIZE(&destination, &destinationSize, myStream->DeviceId, myStream->WaterTemp, myStream-> TimeStamp);
            }
            
            if (serializeStatus != IOT_AGENT_OK)
            {
              LogInfo("Failed to serialize\r\n");
            }
            else
            {
              // create the message
              IOTHUB_MESSAGE_HANDLE messageHandle = IoTHubMessage_CreateFromByteArray(destination, destinationSize);
              if (messageHandle == NULL)
              {
                LogInfo("unable to create a new IoTHubMessage\r\n");
              }
              else
              {
                // send the message
                if (IoTHubClient_LL_SendEventAsync(iotHubClientHandle, messageHandle, sendCallback, (void*)1) != IOTHUB_CLIENT_OK)
                {
                  LogInfo("failed to hand over the message to IoTHubClient\r\n");
                }
                else
                {
                  LogInfo("IoTHubClient accepted the message for delivery\r\n");
                }

                IoTHubMessage_Destroy(messageHandle);
              }
              free(destination);
            }
          }

          IOTHUB_CLIENT_STATUS status;

          // send all events in the buffer
          // and get incoming messages
          while ((IoTHubClient_LL_GetSendStatus(iotHubClientHandle, &status) == IOTHUB_CLIENT_OK) && (status == IOTHUB_CLIENT_SEND_STATUS_BUSY))
          {
            IoTHubClient_LL_DoWork(iotHubClientHandle);
            ThreadAPI_Sleep(1000);
          }
          /* wait for commands
          while (1)
          {
            IoTHubClient_LL_DoWork(iotHubClientHandle);
            ThreadAPI_Sleep(100);
          }*/
        }

        // clear the model
        DESTROY_MODEL_INSTANCE(myStream);
      }

      // uninitialize the IoT Hub client handle
      IoTHubClient_LL_Destroy(iotHubClientHandle);
    }

    // uninitialize the serializer
    serializer_deinit();
  }
}
