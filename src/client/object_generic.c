/**
 * @license
 * Copyright (c) 2019 CANDY LINE INC.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v2.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v20.html
 * The Eclipse Distribution License is available at
 *    http://www.eclipse.org/org/documents/edl-v10.php.
 */

#include "liblwm2m.h"
#include "lwm2mclient.h"
#include "base64.h"
#include "commandline.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <inttypes.h>

typedef struct
{
    uint16_t objectId;
    uint8_t * response;
    size_t responseLen;
} parent_context_t;

typedef struct generic_obj_instance
{   //linked list:
    struct generic_obj_instance*   next;       // matches lwm2m_list_t::next
    uint16_t                       objInstId;  // matches lwm2m_list_t::id
} generic_obj_instance_t;


static uint8_t * find_base64_from_response(char * cmd, uint8_t * resp, uint16_t ** len)
{
    // /resp:{command}:{base64 length}:{base64 payload}\r\n
    uint8_t * pc;
    // '/resp'
    pc = strtok(resp, ":");
    if (pc == NULL) {
        fprintf(stderr, "error: Not a valid response(cmd:[%s])\r\n", cmd);
        return NULL;
    }
    // '{command}'
    pc = strtok(NULL, ":");
    if (pc == NULL) {
        fprintf(stderr, "error: Not a valid response(cmd:[%s])\r\n", cmd);
        return NULL;
    }
    if (strcmp(pc, cmd) != 0) {
        fprintf(stderr, "error: Not an expected cmd response(expected cmd:[%s], actual cmd:[%s])\r\n", cmd, pc);
        return NULL;
    }
    // '{base64 length}'
    pc = strtok(NULL, ":");
    *len = atoi((const char *)pc);
    // {base64 payload}
    pc = strtok(NULL, ":");
    if (pc == NULL) {
        fprintf(stderr, "error: Not a valid response(cmd:[%s])\r\n", cmd);
        return NULL;
    }
    return pc;
}

static uint8_t handle_response(parent_context_t * context, char * cmd)
{
    size_t expectedPayloadLen;
    size_t payloadLen;
    uint8_t * payload;
    uint8_t buffer[MAX_MESSAGE_SIZE];
    size_t recvLen;
    size_t recvExpectedTotalLen;
    size_t recvTotalLen;
    uint8_t * bufferPtr;

    memset(buffer, 0, MAX_MESSAGE_SIZE);
    payload = NULL;
    recvExpectedTotalLen = 0;
    recvTotalLen = 0;
    bufferPtr = buffer;

    do {
        // read recv data
        recvLen = read(STDIN_FILENO, bufferPtr, MAX_MESSAGE_SIZE - 1);
        if (recvLen < 1) {
            fprintf(stderr, "error:COAP_500_INTERNAL_SERVER_ERROR=>[%s], empty response\r\n", cmd);
            return COAP_500_INTERNAL_SERVER_ERROR;
        }
        recvTotalLen += recvLen;
        if (payload == NULL) {
            payload = find_base64_from_response(cmd, bufferPtr, &expectedPayloadLen);
            if (NULL == payload) {
                fprintf(stderr, "error:COAP_500_INTERNAL_SERVER_ERROR=>[%s], resp=>[%s] (expectedPayloadLen:%zu)\r\n", cmd, buffer, expectedPayloadLen);
                return COAP_500_INTERNAL_SERVER_ERROR;
            }
            recvExpectedTotalLen = (payload - buffer) + expectedPayloadLen;
        }
        bufferPtr += recvLen;
    } while (recvTotalLen < recvExpectedTotalLen && recvTotalLen < MAX_MESSAGE_SIZE - 1);
    
    payloadLen = strlen((const char *)payload);

    // decoded result
    fprintf(stderr, "done:cmd=>[%s], resp=>[%s], base64=>[%s], base64Len=>[%zu], expectedPayloadLen=>[%zu]\r\n", cmd, buffer, payload, payloadLen, expectedPayloadLen);
    context->response = util_base64_decode(payload, payloadLen, &context->responseLen);
    if (context->responseLen == 0) {
        fprintf(stderr, "error:COAP_500_INTERNAL_SERVER_ERROR=>[%s], resp=>[%s]\r\n", cmd, buffer);
        return COAP_500_INTERNAL_SERVER_ERROR;
    }

    return COAP_NO_ERROR;
}

static uint8_t request_command(parent_context_t * context,
                               char * cmd,
                               uint8_t * payloadRaw,
                               size_t payloadRawLen)
{
    fd_set readfds;
    struct timeval tv;
    size_t payloadLen;
    uint8_t * payload;
    int recvResult;

    // parent process re timeout
    tv.tv_sec = 1;       // 1sec
    tv.tv_usec = 500000; // 500ms

    // setup FD
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    // encode payload
    payload = util_base64_encode(
        (const uint8_t *)payloadRaw, payloadRawLen, &payloadLen);
    if (NULL == payload) {
        fprintf(stderr, "error:COAP_400_BAD_REQUEST=>[%s]\r\n", cmd);
        return COAP_400_BAD_REQUEST;
    }

    // send command
    fprintf(stdout, "/%s:%zu:%s\r\n", cmd, payloadLen, payload);
    fflush(stdout);

    // release
    lwm2m_free(payload);
    payload = NULL;

    // wait for response
    recvResult = select(FD_SETSIZE, &readfds, NULL, NULL, &tv);
    if (recvResult < 1 || !FD_ISSET(STDIN_FILENO, &readfds)) {
        fprintf(stderr, "error:COAP_501_NOT_IMPLEMENTED=>[%s]\r\n", cmd);
        return COAP_501_NOT_IMPLEMENTED;
    }
    uint8_t err = handle_response(context, cmd);
    return err;
}

static parent_context_t * setup_parent_context(uint16_t objectId)
{
    parent_context_t * context = (parent_context_t *)lwm2m_malloc(sizeof(parent_context_t));
    memset(context, 0, sizeof(parent_context_t));
    context->objectId = objectId;
    return context;
}

static void response_free(parent_context_t * context)
{
    if (NULL != context->response) {
        lwm2m_free(context->response);
        context->response = NULL;
    }
    context->responseLen = 0;
}

static void lwm2m_data_cp(lwm2m_data_t * dataP,
                          uint8_t * data,
                          uint16_t len)
{
    char * buf;
    switch(dataP->type) {
        case LWM2M_TYPE_STRING:
            lwm2m_data_encode_nstring((const char *)data, len, dataP);
            break;
        case LWM2M_TYPE_OPAQUE:
            lwm2m_data_encode_opaque(data, len, dataP);
            break;
        case LWM2M_TYPE_INTEGER:
            buf = lwm2m_malloc(len + 1);
            memcpy(buf, data, len);
            buf[len] = '\0';
            lwm2m_data_encode_int(strtoll(buf, NULL, 10), dataP);
            lwm2m_free(buf);
            break;
        case LWM2M_TYPE_FLOAT:
            buf = lwm2m_malloc(len + 1);
            memcpy(buf, data, len);
            buf[len] = '\0';
            lwm2m_data_encode_float(strtod(buf, NULL), dataP);
            lwm2m_free(buf);
            break;
        case LWM2M_TYPE_BOOLEAN:
            lwm2m_data_encode_bool((data[0] == 1), dataP);
            break;
        case LWM2M_TYPE_OBJECT_LINK:
            lwm2m_data_encode_objlink(data[0] + (((uint16_t)data[1]) << 8),
                                      data[2] + (((uint16_t)data[3]) << 8),
                                      dataP);
            break;
        case LWM2M_TYPE_MULTIPLE_RESOURCE:
            {
                uint16_t i = 0;
                uint16_t idx = 2;
                uint16_t childSize;
                uint16_t count = data[0] + (((uint16_t)data[1]) << 8);
                lwm2m_data_t * children = lwm2m_data_new(count);
                for (; i < count; i++) {
                    children[i].id = data[idx++];
                    children[i].id += (((uint16_t)data[idx++]) << 8);
                    children[i].type = data[idx++];
                    childSize = data[idx++];
                    childSize += (((uint16_t)data[idx++]) << 8);
                    lwm2m_data_cp(&children[i], &data[idx], childSize);
                    idx += childSize;
                }
                lwm2m_data_encode_instances(children, count, dataP);
            }
            break;
        default:
            break;
    }
}

static uint8_t prv_generic_read_instances(
                                int * numDataP,
                                uint16_t ** instaceIdArrayP,
                                lwm2m_object_t * objectP)
{
    uint16_t i = 0;
    uint8_t messageId = 0x10;
    uint8_t result;
    parent_context_t * context = (parent_context_t *)objectP->userData;
    size_t payloadRawLen = 8;
    uint8_t * payloadRaw = lwm2m_malloc(payloadRawLen);
    memset(payloadRaw, 0, payloadRawLen);
    payloadRaw[i++] = 0x01;                     // Data Type: 0x01 (Request), 0x02 (Response)
    payloadRaw[i++] = messageId;                // Message Id associated with Data Type
    payloadRaw[i++] = context->objectId & 0xff; // ObjectID LSB
    payloadRaw[i++] = context->objectId >> 8;   // ObjectID MSB

    fprintf(stderr, "prv_generic_read_instances:objectId=>%hu\r\n", context->objectId);

    result = request_command(context, "readInstances", payloadRaw, payloadRawLen);
    lwm2m_free(payloadRaw);

    /*
     * Response Data Format (result = COAP_NO_ERROR)
     * 02 ... Data Type: 0x01 (Request), 0x02 (Response)
     * 00 ... Message Id associated with Data Type
     * 45 ... Result Status Code e.g. COAP_205_CONTENT
     * 00 ... ObjectID LSB
     * 00 ... ObjectID MSB
     * 00 ... # of instances LSB
     * 00 ... # of instances MSB
     * 00 ... InstanceId LSB  <============= First InstanceId LSB (index:7)
     * 00 ... InstanceId MSB
     * 00 ... InstanceId LSB  <============= Second InstanceId LSB
     * 00 ... InstanceId MSB
     * ..
     */
    uint16_t idx = 7; // First InstanceId LSB index
    uint8_t * response = context->response;
    if (COAP_NO_ERROR == result && response[0] == 0x02 && messageId == response[1]) {
        result = response[2];
        *numDataP = response[5] + (((uint16_t)response[6]) << 8);
        *instaceIdArrayP = lwm2m_malloc(*numDataP * sizeof(uint16_t));
        if (*instaceIdArrayP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
        fprintf(stderr, "prv_generic_read_instances:(lwm2m_data_new):numData=>%d\r\n",
            *numDataP);
        for (i = 0; i < *numDataP; i++)
        {
            (*instaceIdArrayP)[i] = response[idx++];
            (*instaceIdArrayP)[i] += (((uint16_t)response[idx++]) << 8);
        }
    } else {
        result = COAP_400_BAD_REQUEST;
    }
    response_free(context);
    fprintf(stderr, "prv_generic_read_instances:result=>0x%X\r\n", result);
    return result;
}

static uint8_t setup_instance_ids(lwm2m_object_t * objectP)
{
    int size = 0;
    uint16_t * instanceIdArray = NULL;
    uint16_t * instanceIdArrayBackup = NULL;
    uint8_t result = prv_generic_read_instances(&size, &instanceIdArray, objectP);
    if (result != COAP_205_CONTENT && result != COAP_404_NOT_FOUND)
    {
        if (NULL != instanceIdArray) {
            lwm2m_free(instanceIdArray);
        }
        return result;
    }
    result = COAP_NO_ERROR;
    instanceIdArrayBackup = instanceIdArray;
    int i;
    generic_obj_instance_t * targetP;
    for (i = 0; i < size; i++) {
        targetP = (generic_obj_instance_t *)lwm2m_malloc(sizeof(generic_obj_instance_t));
        if (NULL == targetP)
        {
            result = COAP_500_INTERNAL_SERVER_ERROR;
            break;
        }
        memset(targetP, 0, sizeof(generic_obj_instance_t));

        targetP->objInstId    = *instanceIdArray;
        objectP->instanceList = LWM2M_LIST_ADD(objectP->instanceList, targetP);
        ++instanceIdArray;
        fprintf(stderr, "setup_instance_ids:objectId=>%d:instanceId=%d (%d/%d)\r\n",
            objectP->objID, targetP->objInstId, i, size);
    }
    lwm2m_free(instanceIdArrayBackup);
    return result;
}

static uint8_t prv_generic_read(uint16_t instanceId,
                                int * numDataP,
                                lwm2m_data_t ** dataArrayP,
                                lwm2m_object_t * objectP)
{
    if (*numDataP > MAX_RESOURCES) {
        return COAP_400_BAD_REQUEST;
    }

    uint16_t i = 0;
    uint16_t j = 0;
    uint8_t messageId = 0x01;
    uint8_t result;
    parent_context_t * context = (parent_context_t *)objectP->userData;
    size_t payloadRawLen = 8 + *numDataP * 2;
    uint8_t * payloadRaw = lwm2m_malloc(payloadRawLen);
    payloadRaw[i++] = 0x01;                     // Data Type: 0x01 (Request), 0x02 (Response)
    payloadRaw[i++] = messageId;                // Message Id associated with Data Type
    payloadRaw[i++] = context->objectId & 0xff; // ObjectID LSB
    payloadRaw[i++] = context->objectId >> 8;   // ObjectID MSB
    payloadRaw[i++] = instanceId & 0xff;        // InstanceId LSB
    payloadRaw[i++] = instanceId >> 8;          // InstanceId MSB
    payloadRaw[i++] = *numDataP & 0xff;         // # of required data LSB (0x0000=ALL)
    payloadRaw[i++] = *numDataP >> 8;           // # of required data MSB

    fprintf(stderr, "prv_generic_read:objectId=>%hu, instanceId=>%hu, numData=>%d\r\n",
        context->objectId, instanceId, *numDataP);
    for(; i < payloadRawLen;)
    {
        uint16_t id = (*dataArrayP)[j++].id;
        payloadRaw[i++] = id & 0xff; // ResourceId LSB
        payloadRaw[i++] = id >> 8;   // ResourceId MSB
        fprintf(stderr, "prv_generic_read: [%d of %d] resourcId=>%hu\r\n", j, *numDataP, id);
    }

    result = request_command(context, "read", payloadRaw, payloadRawLen);
    lwm2m_free(payloadRaw);

    /*
     * Response Data Format (result = COAP_NO_ERROR)
     * 02 ... Data Type: 0x01 (Request), 0x02 (Response)
     * 00 ... Message Id associated with Data Type
     * 45 ... Result Status Code e.g. COAP_205_CONTENT
     * 00 ... ObjectID LSB
     * 00 ... ObjectID MSB
     * 00 ... InstanceId LSB
     * 00 ... InstanceId MSB
     * 00 ... # of resources LSB
     * 00 ... # of resources MSB
     * 00 ... ResouceId LSB  <============= First ResourceId LSB (index:9)
     * 00 ... ResouceId MSB
     * 00 ... Resouce Data Type
     * 00 ... Length of resource data LSB
     * 00 ... Length of resource data MSB
     * 00 ... Resource Data
     * 00 ... ResouceId LSB  <============= Second ResourceId LSB
     * 00 ... ResouceId MSB
     * 00 ... Resouce Data Type
     * 00 ... Length of resource data LSB
     * 00 ... Length of resource data MSB
     * 00 ... Resource Data
     * ..
     */
    uint16_t idx = 9; // First ResouceId LSB index
    uint16_t len; // Resource data length
    uint8_t * response = context->response;
    if (COAP_NO_ERROR == result && response[0] == 0x02 && messageId == response[1]) {
        result = response[2];
        if (*numDataP == 0) {
            *numDataP = response[7] + (((uint16_t)response[8]) << 8);
            *dataArrayP = lwm2m_data_new(*numDataP);
            if (*dataArrayP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
        }
        fprintf(stderr, "prv_generic_read:(lwm2m_data_new):numData=>%d\r\n",
            *numDataP);
        for (i = 0; i < *numDataP; i++)
        {
            (*dataArrayP)[i].id = response[idx++];
            (*dataArrayP)[i].id += (((uint16_t)response[idx++]) << 8);
            (*dataArrayP)[i].type = response[idx++];
            len = response[idx++];
            len += (((uint16_t)response[idx++]) << 8);
            lwm2m_data_cp(&(*dataArrayP)[i], &response[idx], len);
            idx += len;
        }
    } else {
        result = COAP_400_BAD_REQUEST;
    }
    response_free(context);
    fprintf(stderr, "prv_generic_read:result=>0x%X\r\n", result);
    return result;
}

static uint16_t lwm2m_get_payload_size(int numData,
                                       lwm2m_data_t * dataArray)
{
    uint16_t i = 0;
    uint16_t len = 0;
    for (i = 0; i < numData; i++)
    {
        len += 5; // ResourceId (16bit) + Resouce Data Type (8bit) + Length of resource data (16bit)
        switch (dataArray[i].type) {
            case LWM2M_TYPE_STRING:
            case LWM2M_TYPE_OPAQUE:
                len += dataArray[i].value.asBuffer.length;
                break;
            case LWM2M_TYPE_INTEGER:
                len += snprintf(NULL, 0, "%" PRIu64, dataArray[i].value.asInteger);
                break;
            case LWM2M_TYPE_FLOAT:
                len += snprintf(NULL, 0, "%f", dataArray[i].value.asFloat);
                break;
            case LWM2M_TYPE_BOOLEAN:
                len += 1;
                break;
            case LWM2M_TYPE_OBJECT_LINK:
                len += sizeof(uint16_t) * 2;
                break;
            case LWM2M_TYPE_MULTIPLE_RESOURCE:
                ++len; // # of resources LSB
                ++len; // # of resources MSB
                len += lwm2m_get_payload_size(
                    dataArray[i].value.asChildren.count,
                    dataArray[i].value.asChildren.array);
                break;
            default:
                break;
        }
    }
    return len;
}

static size_t lwm2m_write_payload(uint16_t * i,
                                  uint8_t * payloadRaw,
                                  int numData,
                                  lwm2m_data_t * dataArray)
{
    size_t written_len = 0;
    size_t len;
    uint16_t j;

    for (j = 0; j < numData; j++)
    {
        uint16_t id = dataArray[j].id;
        uint16_t begin = *i;
        payloadRaw[(*i)++] = id & 0xff;  // ResourceId LSB
        payloadRaw[(*i)++] = id >> 8;    // ResourceId MSB
        payloadRaw[(*i)++] = dataArray[j].type; // Resouce Data Type
        payloadRaw[(*i)++] = 0x00;       // Length of resource data LSB (Update later)
        payloadRaw[(*i)++] = 0x00;       // Length of resource data MSB (Update later)
        written_len += 5;
        len = 0;
        switch (dataArray[j].type) {
            case LWM2M_TYPE_STRING:
            case LWM2M_TYPE_OPAQUE:
                len = dataArray[j].value.asBuffer.length;
                memcpy(
                    &payloadRaw[*i],
                    dataArray[j].value.asBuffer.buffer,
                    dataArray[j].value.asBuffer.length);
                break;
            case LWM2M_TYPE_INTEGER:
                len = sprintf((char *)&payloadRaw[*i], "%" PRIu64, dataArray[j].value.asInteger);
                break;
            case LWM2M_TYPE_FLOAT:
                len = sprintf((char *)&payloadRaw[*i], "%f", dataArray[j].value.asFloat);
                break;
            case LWM2M_TYPE_BOOLEAN:
                len = 1;
                payloadRaw[*i] = dataArray[j].value.asBoolean;
                break;
            case LWM2M_TYPE_OBJECT_LINK:
                len = sizeof(uint16_t) * 2;
                payloadRaw[*i + 0] = dataArray[j].value.asObjLink.objectId & 0xff; // objectId LSB
                payloadRaw[*i + 1] = dataArray[j].value.asObjLink.objectId >> 8;   // objectId MSB
                payloadRaw[*i + 2] = dataArray[j].value.asObjLink.objectInstanceId & 0xff; // objectInstanceId LSB
                payloadRaw[*i + 3] = dataArray[j].value.asObjLink.objectInstanceId >> 8;   // objectInstanceId MSB
                break;
            case LWM2M_TYPE_MULTIPLE_RESOURCE:
                len = lwm2m_write_payload(
                    i,
                    &payloadRaw[*i],
                    dataArray[j].value.asChildren.count,
                    dataArray[j].value.asChildren.array);
                break;
            default:
                break;
        }
        *i += len;
        written_len += len;
        payloadRaw[begin + 3] = len & 0xff; // Length of resource data LSB
        payloadRaw[begin + 4] = len >> 8;   // Length of resource data MSB
    }
    return written_len;
}

static uint8_t prv_generic_write(uint16_t instanceId,
                                 int numData,
                                 lwm2m_data_t * dataArray,
                                 lwm2m_object_t * objectP)
{
    uint16_t i = 0;
    uint8_t messageId = 0x01;
    uint8_t result;
    parent_context_t * context = (parent_context_t *)objectP->userData;
    size_t payloadRawLen = 8 + lwm2m_get_payload_size(numData, dataArray);
    uint8_t * payloadRaw = lwm2m_malloc(payloadRawLen);
    payloadRaw[i++] = 0x01;                     // Data Type: 0x01 (Request), 0x02 (Response)
    payloadRaw[i++] = messageId;                // Message Id associated with Data Type
    payloadRaw[i++] = context->objectId & 0xff; // ObjectID LSB
    payloadRaw[i++] = context->objectId >> 8;   // ObjectID MSB
    payloadRaw[i++] = instanceId & 0xff;        // InstanceId LSB
    payloadRaw[i++] = instanceId >> 8;          // InstanceId MSB
    payloadRaw[i++] = numData & 0xff;           // # of required data LSB (0x0000=ALL)
    payloadRaw[i++] = numData >> 8;             // # of required data MSB
    lwm2m_write_payload(&i, payloadRaw, numData, dataArray);

    fprintf(stderr, "prv_generic_write:objectId=>%hu, instanceId=>%hu, numData=>%d\r\n",
      context->objectId, instanceId, numData);
    result = request_command(context, "write", payloadRaw, payloadRawLen);
    lwm2m_free(payloadRaw);

    /*
    * Response Data Format (result = COAP_NO_ERROR)
    * 02 ... Data Type: 0x01 (Request), 0x02 (Response)
    * 00 ... Message Id associated with Data Type
    * 45 ... Result Status Code e.g. COAP_205_CONTENT
    * 00 ... ObjectID LSB
    * 00 ... ObjectID MSB
    * 00 ... InstanceId LSB
    * 00 ... InstanceId MSB
    * 00 ... always 00
    * 00 ... always 00
    */
    uint8_t * response = context->response;
    if (COAP_NO_ERROR == result && response[0] == 0x02 && messageId == response[1]) {
        result = response[2];
    } else {
        result = COAP_400_BAD_REQUEST;
    }
    response_free(context);
    fprintf(stderr, "prv_generic_write:result=>0x%X\r\n", result);
    return result;
}

static uint8_t prv_generic_execute(uint16_t instanceId,
                                   uint16_t resourceId,
                                   uint8_t * buffer,
                                   int length,
                                   lwm2m_object_t * objectP)
{
    uint16_t i = 0;
    uint8_t messageId = 0x01;
    uint8_t result;
    parent_context_t * context = (parent_context_t *)objectP->userData;
    size_t payloadRawLen = 10 + length;
    uint8_t * payloadRaw = lwm2m_malloc(payloadRawLen);
    payloadRaw[i++] = 0x01;                     // Data Type: 0x01 (Request), 0x02 (Response)
    payloadRaw[i++] = messageId;                // Message Id associated with Data Type
    payloadRaw[i++] = context->objectId & 0xff; // ObjectID LSB
    payloadRaw[i++] = context->objectId >> 8;   // ObjectID MSB
    payloadRaw[i++] = instanceId & 0xff;        // InstanceId LSB
    payloadRaw[i++] = instanceId >> 8;          // InstanceId MSB
    payloadRaw[i++] = 1;                        // # of resources LSB (always 01)
    payloadRaw[i++] = 0;                        // # of resources MSB (always 00)
    payloadRaw[i++] = resourceId & 0xff;        // ResourceId LSB
    payloadRaw[i++] = resourceId >> 8;          // ResourceId MSB
    memcpy(&payloadRaw[i], buffer, length);

    fprintf(stderr, "prv_generic_execute:objectId=>%hu, instanceId=>%hu, resourceId=>%hu, buffer length=>%d\r\n",
    context->objectId, instanceId, resourceId, length);
    result = request_command(context, "execute", payloadRaw, payloadRawLen);
    lwm2m_free(payloadRaw);

    /*
     * Response Data Format (result = COAP_NO_ERROR)
     * 02 ... Data Type: 0x01 (Request), 0x02 (Response)
     * 00 ... Message Id associated with Data Type
     * 45 ... Result Status Code e.g. COAP_205_CONTENT
     * 00 ... ObjectID LSB
     * 00 ... ObjectID MSB
     * 00 ... InstanceId LSB
     * 00 ... InstanceId MSB
     * 00 ... always 00
     * 00 ... always 00
     */
    uint8_t * response = context->response;
    if (COAP_NO_ERROR == result && response[0] == 0x02 && messageId == response[1]) {
        result = response[2];
    } else {
        result = COAP_400_BAD_REQUEST;
    }
    response_free(context);
    fprintf(stderr, "prv_generic_execute:result=>0x%X\r\n", result);
    return result;
}

static uint8_t prv_generic_discover(uint16_t instanceId,
                                    int * numDataP,
                                    lwm2m_data_t ** dataArrayP,
                                    lwm2m_object_t * objectP)
{
    if (*numDataP > MAX_RESOURCES) {
        return COAP_400_BAD_REQUEST;
    }

    uint16_t i = 0;
    uint16_t j = 0;
    uint8_t messageId = 0x01;
    uint8_t result;
    parent_context_t * context = (parent_context_t *)objectP->userData;
    size_t payloadRawLen = 8 + *numDataP * 2;
    uint8_t * payloadRaw = lwm2m_malloc(payloadRawLen);
    payloadRaw[i++] = 0x01;                     // Data Type: 0x01 (Request), 0x02 (Response)
    payloadRaw[i++] = messageId;                // Message Id associated with Data Type
    payloadRaw[i++] = context->objectId & 0xff; // ObjectID LSB
    payloadRaw[i++] = context->objectId >> 8;   // ObjectID MSB
    payloadRaw[i++] = instanceId & 0xff;        // InstanceId LSB
    payloadRaw[i++] = instanceId >> 8;          // InstanceId MSB
    payloadRaw[i++] = *numDataP & 0xff;         // # of required data LSB (0x0000=ALL)
    payloadRaw[i++] = *numDataP >> 8;           // # of required data MSB
    for(; i < payloadRawLen;)
    {
        uint16_t id = (*dataArrayP)[j++].id;
        payloadRaw[i++] = id & 0xff; // ResourceId LSB
        payloadRaw[i++] = id >> 8;   // ResourceId MSB
    }

    fprintf(stderr, "prv_generic_discover:objectId=>%hu, instanceId=>%hu, numData=>%d\r\n",
        context->objectId, instanceId, *numDataP);
    result = request_command(context, "discover", payloadRaw, payloadRawLen);
    lwm2m_free(payloadRaw);

    /*
     * Response Data Format (result = COAP_NO_ERROR)
     * 02 ... Data Type: 0x01 (Request), 0x02 (Response)
     * 00 ... Message Id associated with Data Type
     * 45 ... Result Status Code e.g. COAP_205_CONTENT
     * 00 ... ObjectID LSB
     * 00 ... ObjectID MSB
     * 00 ... InstanceId LSB
     * 00 ... InstanceId MSB
     * 00 ... # of resources LSB
     * 00 ... # of resources MSB
     * 00 ... ResouceId LSB  <============= First ResourceId LSB (index:9)
     * 00 ... ResouceId MSB
     * 00 ... ResouceId LSB  <============= Second ResourceId LSB (index:11)
     * 00 ... ResouceId MSB
     * ..
     */
    uint16_t idx = 9; // First ResouceId LSB index
    uint8_t * response = context->response;
    if (COAP_NO_ERROR == result && response[0] == 0x02 && messageId == response[1]) {
        result = response[2];
        if (*numDataP == 0) {
            *numDataP = response[7] + (((uint16_t)response[8]) << 8);
            *dataArrayP = lwm2m_data_new(*numDataP);
            if (*dataArrayP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
            fprintf(stderr, "prv_generic_discover:(lwm2m_data_new):numData=>%d\r\n",
                *numDataP);
        }
        for (i = 0; i < *numDataP; i++)
        {
            (*dataArrayP)[i].id = response[idx++];
            (*dataArrayP)[i].id += (((uint16_t)response[idx++]) << 8);
        }
    } else {
        result = COAP_400_BAD_REQUEST;
    }
    response_free(context);
    fprintf(stderr, "prv_generic_discover:result=>0x%X\r\n", result);
    return result;
}

static uint8_t prv_generic_create(uint16_t instanceId,
                                  int numData,
                                  lwm2m_data_t * dataArray,
                                  lwm2m_object_t * objectP)
{
    uint16_t i = 0;
    uint8_t messageId = 0x01;
    uint8_t result;
    parent_context_t * context = (parent_context_t *)objectP->userData;
    size_t payloadRawLen = 8 + lwm2m_get_payload_size(numData, dataArray);
    uint8_t * payloadRaw = lwm2m_malloc(payloadRawLen);
    payloadRaw[i++] = 0x01;                     // Data Type: 0x01 (Request), 0x02 (Response)
    payloadRaw[i++] = messageId;                // Message Id associated with Data Type
    payloadRaw[i++] = context->objectId & 0xff; // ObjectID LSB
    payloadRaw[i++] = context->objectId >> 8;   // ObjectID MSB
    payloadRaw[i++] = instanceId & 0xff;        // InstanceId LSB
    payloadRaw[i++] = instanceId >> 8;          // InstanceId MSB
    payloadRaw[i++] = numData & 0xff;           // # of required data LSB (0x0000=ALL)
    payloadRaw[i++] = numData >> 8;             // # of required data MSB
    lwm2m_write_payload(&i, payloadRaw, numData, dataArray);

    fprintf(stderr, "prv_generic_create:objectId=>%hu, instanceId=>%hu, numData=>%d\r\n",
        context->objectId, instanceId, numData);
    result = request_command(context, "create", payloadRaw, payloadRawLen);
    lwm2m_free(payloadRaw);

    /*
    * Response Data Format (result = COAP_NO_ERROR)
    * 02 ... Data Type: 0x01 (Request), 0x02 (Response)
    * 00 ... Message Id associated with Data Type
    * 45 ... Result Status Code e.g. COAP_201_CREATED
    * 00 ... ObjectID LSB
    * 00 ... ObjectID MSB
    * 00 ... InstanceId LSB
    * 00 ... InstanceId MSB
    * 00 ... always 00
    * 00 ... always 00
    */
    uint8_t * response = context->response;
    if (COAP_NO_ERROR == result && response[0] == 0x02 && messageId == response[1]) {
      result = response[2];
    } else {
      result = COAP_400_BAD_REQUEST;
    }
    response_free(context);

    if (result == COAP_201_CREATED) {
        // Add a new instance ID to the existing instance ID list
        generic_obj_instance_t * targetP;
        targetP = (generic_obj_instance_t *)lwm2m_malloc(sizeof(generic_obj_instance_t));
        if (NULL == targetP)
        {
            result = COAP_500_INTERNAL_SERVER_ERROR;
        }
        else
        {
            memset(targetP, 0, sizeof(generic_obj_instance_t));
            targetP->objInstId    = instanceId;
            objectP->instanceList = LWM2M_LIST_ADD(objectP->instanceList, targetP);
        }
    }

    fprintf(stderr, "prv_generic_create:result=>0x%X\r\n", result);
    return result;
}

static uint8_t prv_generic_delete(uint16_t instanceId,
                                  lwm2m_object_t * objectP)
{
    uint16_t i = 0;
    uint8_t messageId = 0x01;
    uint8_t result;
    parent_context_t * context = (parent_context_t *)objectP->userData;
    size_t payloadRawLen = 8;
    uint8_t * payloadRaw = lwm2m_malloc(payloadRawLen);
    payloadRaw[i++] = 0x01;                     // Data Type: 0x01 (Request), 0x02 (Response)
    payloadRaw[i++] = messageId;                // Message Id associated with Data Type
    payloadRaw[i++] = context->objectId & 0xff; // ObjectID LSB
    payloadRaw[i++] = context->objectId >> 8;   // ObjectID MSB
    payloadRaw[i++] = instanceId & 0xff;        // InstanceId LSB
    payloadRaw[i++] = instanceId >> 8;          // InstanceId MSB
    payloadRaw[i++] = 0;                        // # of required data LSB (Always 0x0000)
    payloadRaw[i++] = 0;                        // # of required data MSB

    fprintf(stderr, "prv_generic_delete:objectId=>%hu, instanceId=>%hu\r\n",
      context->objectId, instanceId);
    result = request_command(context, "delete", payloadRaw, payloadRawLen);
    lwm2m_free(payloadRaw);

    /*
    * Response Data Format (result = COAP_NO_ERROR)
    * 02 ... Data Type: 0x01 (Request), 0x02 (Response)
    * 00 ... Message Id associated with Data Type
    * 45 ... Result Status Code e.g. COAP_202_DELETED
    * 00 ... ObjectID LSB
    * 00 ... ObjectID MSB
    * 00 ... InstanceId LSB
    * 00 ... InstanceId MSB
    * 00 ... always 00
    * 00 ... always 00
    */
    uint8_t * response = context->response;
    if (COAP_NO_ERROR == result && response[0] == 0x02 && messageId == response[1]) {
        result = response[2];
    } else {
        result = COAP_400_BAD_REQUEST;
    }
    response_free(context);

    if (result == COAP_202_DELETED) {
      // Delete an instance ID from the list
      generic_obj_instance_t* targetP;
      objectP->instanceList = lwm2m_list_remove(objectP->instanceList, instanceId,
                                               (lwm2m_list_t**)&targetP);
      if (NULL != targetP)
      {
          lwm2m_free(targetP);
      }
    }

    fprintf(stderr, "prv_generic_delete:result=>0x%X\r\n", result);
    return result;
}

lwm2m_object_t * get_object(uint16_t objectId)
{
    lwm2m_object_t * genericObj = (lwm2m_object_t *)lwm2m_malloc(sizeof(lwm2m_object_t));
    if (NULL == genericObj)
    {
        return NULL;
    }
    memset(genericObj, 0, sizeof(lwm2m_object_t));
    genericObj->objID = objectId;

    parent_context_t * context = setup_parent_context(objectId);
    if (NULL != context)
    {
        genericObj->userData = context;
    }
    else
    {
        free_object(genericObj);
        return NULL;
    }

    // Prepare a list of instance IDs
    uint8_t result = setup_instance_ids(genericObj);
    if (result != COAP_NO_ERROR)
    {
        free_object(genericObj);
        return NULL;
    }

    genericObj->readFunc     = prv_generic_read;
    genericObj->discoverFunc = prv_generic_discover;
    genericObj->writeFunc    = prv_generic_write;
    genericObj->executeFunc  = prv_generic_execute;
    if (LWM2M_DEVICE_OBJECT_ID != objectId) {
        genericObj->createFunc   = prv_generic_create;
        genericObj->deleteFunc   = prv_generic_delete;
    }

    return genericObj;
}

void free_object(lwm2m_object_t * objectP)
{
    if (NULL != objectP) {
        if (NULL != objectP->userData) {
            lwm2m_free(objectP->userData);
        }
        if (NULL != objectP->instanceList) {
            lwm2m_list_free(objectP->instanceList);
        }
        lwm2m_free(objectP);
    }
}

uint8_t handle_observe_response(lwm2m_context_t * lwm2mContext)
{
    uint8_t err = COAP_NO_ERROR;
    parent_context_t context;
    err = handle_response(&context, "observe");
    /*
     * Response Data Format (result = COAP_NO_ERROR)
     * 02 ... Data Type: 0x01 (Request), 0x02 (Response)
     * 00 ... Message Id associated with Data Type (always 00)
     * 45 ... Result Status Code e.g. COAP_205_CONTENT
     * 00 ... URI size LSB
     * 00 ... URI size MSB
     * 00 ... URI length LSB <============= First ResourceId LSB (index:5)
     * 00 ... URI length MSB
     * 00 ... URI String Data
     * ..
     * 00 ... URI length LSB <============= First ResourceId LSB (index:5 + URI length)
     * 00 ... URI length MSB
     * 00 ... URI String Data
     * ..
     */
    uint8_t * response = context.response;

    if (COAP_NO_ERROR != err || response[0] != 0x02) {
        return err;
    }
    uint16_t uriLen = response[3] + (((uint16_t)response[4]) << 8);

    uint8_t * b = &response[5];
    uint16_t i = 0;
    uint16_t uriStrLen;
    char uriStr[URI_STRING_MAX_LEN];
    lwm2m_uri_t uri;

    for (; i < uriLen; i++) {
        uriStrLen = *b + (((uint16_t)*(b + 1)) << 8);
        b += 2;
        if (uriStrLen > URI_STRING_MAX_LEN) {
            fprintf(stderr, "handle_observe_response:too long string => %hu\r\n", uriStrLen);
            err = COAP_400_BAD_REQUEST;
            break;
        }
        memcpy(uriStr, b, uriStrLen);
        uriStr[uriStrLen] = '\0';
        b += uriStrLen;
        if (0 == lwm2m_stringToUri(uriStr, uriStrLen, &uri)) {
            err = COAP_400_BAD_REQUEST;
            fprintf(stderr, "handle_observe_response:lwm2m_stringToUri() failed\r\n");
            break;
        }
        lwm2m_resource_value_changed(lwm2mContext, &uri);
    }
    response_free(&context);
    return err;
}

uint8_t backup_object(lwm2m_object_t * objectP)
{
    uint16_t objectId = objectP->objID;
    uint16_t i = 0;
    uint8_t messageId = 0x01;
    uint8_t result;
    parent_context_t context;
    size_t payloadRawLen = 8;
    uint8_t * payloadRaw = lwm2m_malloc(payloadRawLen);
    payloadRaw[i++] = 0x01;                     // Data Type: 0x01 (Request), 0x02 (Response)
    payloadRaw[i++] = messageId;                // Message Id associated with Data Type
    payloadRaw[i++] = objectId & 0xff;          // ObjectID LSB
    payloadRaw[i++] = objectId >> 8;            // ObjectID MSB
    payloadRaw[i++] = 0;                        // always 00
    payloadRaw[i++] = 0;                        // always 00
    payloadRaw[i++] = 0;                        // always 00
    payloadRaw[i++] = 0;                        // always 00

    fprintf(stderr, "backup_object:objectId=>%hu\r\n", objectId);
    result = request_command(&context, "backup", payloadRaw, payloadRawLen);
    lwm2m_free(payloadRaw);

    /*
    * Response Data Format (result = COAP_NO_ERROR)
    * 02 ... Data Type: 0x01 (Request), 0x02 (Response)
    * 00 ... Message Id associated with Data Type
    * 45 ... Result Status Code e.g. COAP_205_CONTENT
    * 00 ... ObjectID LSB
    * 00 ... ObjectID MSB
    * 00 ... always 00
    * 00 ... always 00
    * 00 ... always 00
    * 00 ... always 00
    */
    uint8_t * response = context.response;
    if (COAP_NO_ERROR == result && response[0] == 0x02 && messageId == response[1]) {
      result = response[2];
    } else {
      result = COAP_400_BAD_REQUEST;
    }
    response_free(&context);
    fprintf(stderr, "backup_object:result=>0x%X\r\n", result);
    return result;
}

uint8_t restore_object(lwm2m_object_t * objectP)
{
    uint16_t objectId = objectP->objID;
    uint16_t i = 0;
    uint8_t messageId = 0x01;
    uint8_t result;
    parent_context_t context;
    size_t payloadRawLen = 8;
    uint8_t * payloadRaw = lwm2m_malloc(payloadRawLen);
    payloadRaw[i++] = 0x01;                     // Data Type: 0x01 (Request), 0x02 (Response)
    payloadRaw[i++] = messageId;                // Message Id associated with Data Type
    payloadRaw[i++] = objectId & 0xff;          // ObjectID LSB
    payloadRaw[i++] = objectId >> 8;            // ObjectID MSB
    payloadRaw[i++] = 0;                        // always 00
    payloadRaw[i++] = 0;                        // always 00
    payloadRaw[i++] = 0;                        // always 00
    payloadRaw[i++] = 0;                        // always 00

    fprintf(stderr, "restore_object:objectId=>%hu\r\n", objectId);
    result = request_command(&context, "restore", payloadRaw, payloadRawLen);
    lwm2m_free(payloadRaw);

    /*
    * Response Data Format (result = COAP_NO_ERROR)
    * 02 ... Data Type: 0x01 (Request), 0x02 (Response)
    * 00 ... Message Id associated with Data Type
    * 45 ... Result Status Code e.g. COAP_205_CONTENT
    * 00 ... ObjectID LSB
    * 00 ... ObjectID MSB
    * 00 ... always 00
    * 00 ... always 00
    * 00 ... always 00
    * 00 ... always 00
    */
    uint8_t * response = context.response;
    if (COAP_NO_ERROR == result && response[0] == 0x02 && messageId == response[1]) {
      result = response[2];
    } else {
      result = COAP_400_BAD_REQUEST;
    }
    response_free(&context);
    fprintf(stderr, "restore_object:result=>0x%X\r\n", result);

    // Remove all the entries
    if (NULL != objectP->instanceList) {
        lwm2m_list_free(objectP->instanceList);
        objectP->instanceList = NULL;
    }
    // Read an Object in order to get a list of instance IDs
    result = setup_instance_ids(objectP);
    fprintf(stderr, "restore_object:setup_instance_ids:result=>0x%X\r\n", result);
    return result;
}
