/*******************************************************************************
 *
 * Copyright (c) 2013, 2014 Intel Corporation and others.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * The Eclipse Distribution License is available at
 *    http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    David Navarro, Intel Corporation - initial API and implementation
 *    Benjamin Cabé - Please refer to git log
 *    Fabien Fleutot - Please refer to git log
 *    Simon Bernard - Please refer to git log
 *    Julien Vermillard - Please refer to git log
 *    Axel Lorente - Please refer to git log
 *    Toby Jaffey - Please refer to git log
 *    Bosch Software Innovations GmbH - Please refer to git log
 *    Pascal Rieux - Please refer to git log
 *    Christian Renz - Please refer to git log
 *    Ricky Liu - Please refer to git log
 *
 *******************************************************************************/

/*
 Copyright (c) 2013, 2014 Intel Corporation

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

     * Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
     * Neither the name of Intel Corporation nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 THE POSSIBILITY OF SUCH DAMAGE.

 David Navarro <david.navarro@intel.com>
 Bosch Software Innovations GmbH - Please refer to git log

*/

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

#include "lwm2mclient.h"
#include "commandline.h"
#include "internals.h"

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

#define DEFAULT_MAX_PACKET_SIZE 1024
#ifndef WAKATIWAI_VERSION
#define WAKATIWAI_VERSION "development"
#endif /* WAKATIWAI_VERSION */

int g_reboot = 0;
int g_quit = 0;

lwm2m_object_t ** objArray = NULL;

void handle_sigint(int signum)
{
    g_quit = 1; // graceful shutdown
}

void handle_sigterm(int signum)
{
    g_quit = 2; // graceful shutdown without deregistration
}

#ifdef WITH_TINYDTLS
void * lwm2m_connect_server(uint16_t secObjInstID,
                            void * userData)
{
  client_data_t * dataP;
  lwm2m_list_t * instance;
  dtls_connection_t * newConnP = NULL;
  dataP = (client_data_t *)userData;
  lwm2m_object_t  * securityObj = dataP->securityObjP;

  instance = LWM2M_LIST_FIND(dataP->securityObjP->instanceList, secObjInstID);
  if (instance == NULL) return NULL;


  newConnP = connection_create(dataP->connList, dataP->sock, securityObj, instance->id, dataP->lwm2mH, dataP->addressFamily);
  if (newConnP == NULL)
  {
      fprintf(stderr, "Connection creation failed.\n");
      return NULL;
  }

  dataP->connList = newConnP;
  return (void *)newConnP;
}
#else
void * lwm2m_connect_server(uint16_t secObjInstID,
                            void * userData)
{
    client_data_t * dataP;
    char * uri;
    char * host;
    char * port;
    connection_t * newConnP = NULL;

    dataP = (client_data_t *)userData;

    uri = server_get_uri(dataP->securityObjP, secObjInstID);

    if (uri == NULL) return NULL;

    // parse uri in the form "coaps://[host]:[port]"
    if (0==strncmp(uri, "coaps://", strlen("coaps://"))) {
        host = uri+strlen("coaps://");
    }
    else if (0==strncmp(uri, "coap://",  strlen("coap://"))) {
        host = uri+strlen("coap://");
    }
    else {
        goto exit;
    }
    port = strrchr(host, ':');
    if (port == NULL) goto exit;
    // remove brackets
    if (host[0] == '[')
    {
        host++;
        if (*(port - 1) == ']')
        {
            *(port - 1) = 0;
        }
        else goto exit;
    }
    // split strings
    *port = 0;
    port++;

    fprintf(stderr, "Opening connection to server at %s:%s\r\n", host, port);
    newConnP = connection_create(dataP->connList, dataP->sock, host, port, dataP->addressFamily);
    if (newConnP == NULL) {
        fprintf(stderr, "Connection creation failed.\r\n");
    }
    else {
        dataP->connList = newConnP;
    }

exit:
    lwm2m_free(uri);
    return (void *)newConnP;
}
#endif

void lwm2m_close_connection(void * sessionH,
                            void * userData)
{
    LOG("Entering");
    client_data_t * app_data;
#ifdef WITH_TINYDTLS
    dtls_connection_t * targetP;
#else
    connection_t * targetP;
#endif

    app_data = (client_data_t *)userData;
#ifdef WITH_TINYDTLS
    targetP = (dtls_connection_t *)sessionH;
#else
    targetP = (connection_t *)sessionH;
#endif

    if (targetP == app_data->connList)
    {
        app_data->connList = targetP->next;
        lwm2m_free(targetP);
        LOG_ARG("1) connP(%p) freed", targetP);
    }
    else
    {
#ifdef WITH_TINYDTLS
        dtls_connection_t * parentP;
#else
        connection_t * parentP;
#endif

        parentP = app_data->connList;
        while (parentP != NULL && parentP->next != targetP)
        {
            parentP = parentP->next;
        }
        if (parentP != NULL)
        {
            parentP->next = targetP->next;
            lwm2m_free(targetP);
            LOG_ARG("2) connP(%p) freed", targetP);
        }
    }
}

#ifdef LWM2M_BOOTSTRAP

static void update_bootstrap_info(lwm2m_client_state_t * previousBootstrapState,
        lwm2m_context_t * context)
{
    if (*previousBootstrapState != context->state)
    {
        *previousBootstrapState = context->state;
        switch(context->state)
        {
            case STATE_BOOTSTRAPPING:
                LOG("[BOOTSTRAP] backup security and server objects");
                if (*objArray != NULL)
                {
                    backup_object(objArray[0]); // LWM2M_SECURITY_OBJECT_ID
                    backup_object(objArray[1]); // LWM2M_SERVER_OBJECT_ID
                }
                break;
            default:
                break;
        }
    }
}
#endif

void print_usage(void)
{
    fprintf(stderr, "Usage: " WAKATIWAI_EXECUTABLE " [OPTION]\r\n");
    fprintf(stderr, "Launch a LWM2M client.\r\n");
    fprintf(stderr, "Wakatiwai Version: " WAKATIWAI_VERSION "\r\n");
    fprintf(stderr, "Options:\r\n");
    fprintf(stderr, "  -n NAME\tSet the endpoint name of the Client. Default: wakatiwai\r\n");
    fprintf(stderr, "  -l PORT\tSet the local UDP port of the Client. Default: 56830\r\n");
    fprintf(stderr, "  -4\t\tUse IPv4 connection. Default: IPv6 connection\r\n");
    fprintf(stderr, "  -o OBJIDCSV\tSet the Object ID CSV. Default: 0,1,2,3\r\n");
    fprintf(stderr, "  -d\t\tShow packet dump\r\n");
    fprintf(stderr, "  -s\t\tMaximum receivable packet size in bytes (1024 by default, must be between 1024 and 65535)\r\n");
    fprintf(stderr, "\r\n");
}

static char * server_get_uri(lwm2m_object_t * obj, uint16_t instanceId) {
    int size = 1;
    lwm2m_data_t * dataP = lwm2m_data_new(size);
    dataP->id = 0; // security server uri
    char * uriBuffer;

    obj->readFunc(instanceId, &size, &dataP, obj);
    if (dataP != NULL &&
            (dataP->type == LWM2M_TYPE_STRING || dataP->type == LWM2M_TYPE_OPAQUE) &&
            dataP->value.asBuffer.length > 0) {
        uriBuffer = lwm2m_malloc(dataP->value.asBuffer.length + 1);
        memset(uriBuffer, 0, dataP->value.asBuffer.length + 1);
        strncpy(uriBuffer, (const char *) dataP->value.asBuffer.buffer, dataP->value.asBuffer.length);
        lwm2m_data_free(size, dataP);
        return uriBuffer;
    }
    lwm2m_data_free(size, dataP);
    return NULL;

}

static uint16_t object_id_contains(uint16_t objectId, uint16_t * objectIdArray, uint16_t len) {
    uint16_t result = 0;
    uint16_t i = 0;
    if (objectId < 1 || objectIdArray == NULL || len < 1) {
        return result;
    }
    for (; i < len; i++) {
        if (objectIdArray[i] == objectId) {
            result = 1;
            break;
        }
    }
    return result;
}

static uint16_t * parse_object_id_csv(const char * objectIdCsv, uint16_t * objCount) {
    uint16_t count = 1;
    uint16_t buffIdx = 0;
    uint16_t objectId = 0;
    uint16_t objectIndex = 0;
    uint16_t * objectIdArray;
    char buff[12];
    const char * c = objectIdCsv;
    for (; *c != '\0'; c++) {
        if (*c == ',') {
            ++count;
        }
    }
    c = objectIdCsv;
    objectIdArray = lwm2m_malloc(sizeof(uint16_t) * count);
    memset(objectIdArray, 0, sizeof(uint16_t) * count);
    for (; *c != '\0'; c++) {
        if (*c == ',') {
            if (buffIdx > 11) {
                fprintf(stderr, "Too long Object ID\r\n");
                lwm2m_free(objectIdArray);
                return NULL;
            }
            buff[buffIdx] = '\0';
            objectId = strtol(buff, NULL, 10);
            if (object_id_contains(objectId, objectIdArray, objectIndex)) {
                // duplicate object ID, ignored.
            } else  if (objectId > 3) {
                objectIdArray[objectIndex++] = objectId;
            } else {
                fprintf(stderr, "Invalid Object ID\r\n");
                lwm2m_free(objectIdArray);
                return NULL;
            }
            buffIdx = 0;
        } else {
            buff[buffIdx++] = *c;
        }
    }
    if (buffIdx > 11) {
        fprintf(stderr, "Too long Object ID\r\n");
        lwm2m_free(objectIdArray);
        return NULL;
    } else if (buffIdx > 0) {
        buff[buffIdx] = '\0';
        objectId = strtol(buff, NULL, 10);
        if (objectId > 3) {
            objectIdArray[objectIndex++] = objectId;
        } else {
            fprintf(stderr, "Invalid Object ID\r\n");
            lwm2m_free(objectIdArray);
            return NULL;
        }
    }
    *objCount = objectIndex;
#ifdef WITH_LOGS
    uint16_t i = 0;
    for (; i < objectIndex; i++) {
      fprintf(stderr, ">>> %hu => ObjectID:[%hu] \r\n", i, objectIdArray[i]);
    }
    // 4 objects(/0,/1,/2,/3) are implicitly included
    fprintf(stderr, ">>> %hu objects will be deployed as well as predfined 4 objects\r\n", *objCount);
#endif
    return objectIdArray;
}

int main(int argc, char *argv[])
{
    client_data_t data;
    int result;
    lwm2m_context_t * lwm2mH = NULL;
    const char * localPort = "56830";
    char * name = "wakatiwai";
    int opt;
    const char * objectIdCsv = NULL;
    uint16_t * objectIdArray = NULL;
    uint16_t objCount = 0;

#ifdef LWM2M_BOOTSTRAP
    lwm2m_client_state_t previousState = STATE_INITIAL;
#endif

    memset(&data, 0, sizeof(client_data_t));
    data.addressFamily = AF_INET6;
    data.maxPacketSize = DEFAULT_MAX_PACKET_SIZE;

    opt = 1;
    while (opt < argc)
    {
        if (argv[opt] == NULL
            || argv[opt][0] != '-'
            || argv[opt][2] != 0)
        {
            print_usage();
            return 0;
        }
        switch (argv[opt][1])
        {
        case 'n':
            opt++;
            if (opt >= argc)
            {
                print_usage();
                return 0;
            }
            name = argv[opt];
            break;
        case 'l':
            opt++;
            if (opt >= argc)
            {
                print_usage();
                return 0;
            }
            localPort = argv[opt];
            break;
        case '4':
            data.addressFamily = AF_INET;
            break;
        case 'd':
            data.showMessageDump = 1;
            break;
        case 'o':
            opt++;
            if (opt >= argc)
            {
                print_usage();
                return 0;
            }
            objectIdCsv = argv[opt];
            objectIdArray = parse_object_id_csv(objectIdCsv, &objCount);
            if (NULL == objectIdArray)
            {
                print_usage();
                return 0;
            }
            break;
        case 's':
            opt++;
            if (opt >= argc)
            {
                print_usage();
                return 0;
            }
            data.maxPacketSize = strtol(argv[opt], NULL, 10);
            if (data.maxPacketSize < 1024) {
                fprintf(stderr, "Too small max packet size: %s\r\n", argv[opt]);
                print_usage();
                return 0;
            }
            break;
        default:
            print_usage();
            return 0;
        }
        opt += 1;
    }

    /*
     *This call an internal function that create an IPV6 socket on the port 5683.
     */
    fprintf(stderr, "Trying to bind LWM2M Client to port %s\r\n", localPort);
    data.sock = create_socket(localPort, data.addressFamily);
    if (data.sock < 0)
    {
        fprintf(stderr, "Failed to open socket: %d %s\r\n", errno, strerror(errno));
        return -1;
    }

    /*
     * Now the main function fill an array with each object, this list will be later passed to liblwm2m.
     * Those functions are located in their respective object file.
     */
#ifdef WITH_TINYDTLS
#ifdef NDEBUG
    dtls_set_log_level(DTLS_LOG_CRIT);
#endif
#endif

    if (NULL == objArray) {
        objArray = lwm2m_malloc(sizeof(lwm2m_object_t *) * (objCount + 4));
    }
    objArray[0] = get_object(LWM2M_SECURITY_OBJECT_ID);
    if (NULL == objArray[0])
    {
        fprintf(stderr, "Failed to create security object\r\n");
        return -1;
    }
    data.securityObjP = objArray[0];

    objArray[1] = get_object(LWM2M_SERVER_OBJECT_ID);
    if (NULL == objArray[1])
    {
        fprintf(stderr, "Failed to create server object\r\n");
        return -1;
    }

    objArray[2] = get_object(LWM2M_ACL_OBJECT_ID);
    if (NULL == objArray[2])
    {
        fprintf(stderr, "Failed to create Generic Device object for LWM2M_ACL_OBJECT\r\n");
        return -1;
    }

    objArray[3] = get_object(LWM2M_DEVICE_OBJECT_ID);
    if (NULL == objArray[3])
    {
        fprintf(stderr, "Failed to create Generic Device object for LWM2M_DEVICE_OBJECT\r\n");
        return -1;
    }

    uint16_t i = 0; // next index to LWM2M_DEVICE_OBJECT_ID
    for (; i < objCount; i++) {
        uint16_t objectId = objectIdArray[i];
        objArray[i + 4] = get_object(objectId);
        if (NULL == objArray[i + 4])
        {
            fprintf(stderr, "Failed to create Generic Device object for ObjectID:%hu\r\n", objectId);
            return -1;
        }
    }
    if (NULL != objectIdArray) {
        lwm2m_free(objectIdArray);
        objectIdArray = NULL;
    }

    /*
     * The liblwm2m library is now initialized with the functions that will be in
     * charge of communication
     */
    lwm2mH = lwm2m_init(&data);
    if (NULL == lwm2mH)
    {
        fprintf(stderr, "lwm2m_init() failed\r\n");
        return -1;
    }

#ifdef WITH_TINYDTLS
    data.lwm2mH = lwm2mH;
#endif

    /*
     * We configure the liblwm2m library with the name of the client - which shall be unique for each client -
     * the number of objects we will be passing through and the objects array
     */
    result = lwm2m_configure(lwm2mH, name, NULL, NULL, objCount + 4, objArray);
    if (result != 0)
    {
        fprintf(stderr, "lwm2m_configure() failed: 0x%X\r\n", result);
        return -1;
    }

    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigterm);

    fprintf(stderr, "LWM2M Client \"%s\" started on port %s with max rcv packet size %d\r\n", name, localPort, data.maxPacketSize);
    fflush(stderr);

    /*
     * We now enter in a while loop that will handle the communications from the server
     */
    while (0 == g_quit)
    {
        struct timeval tv;
        fd_set readfds;

        if (g_reboot)
        {
            time_t tv_sec;

            tv_sec = lwm2m_gettime();
        }
        else
        {
            tv.tv_sec = 5;
        }
        tv.tv_usec = 0;

        FD_ZERO(&readfds);
        FD_SET(data.sock, &readfds);    // for IP socket
        FD_SET(STDIN_FILENO, &readfds); // for stdin

        /*
         * This function does two things:
         *  - first it does the work needed by liblwm2m (eg. (re)sending some packets).
         *  - Secondly it adjusts the timeout value (default 60s) depending on the state of the transaction
         *    (eg. retransmission) and the time between the next operation
         */
        result = lwm2m_step(lwm2mH, &(tv.tv_sec));

#ifdef WITH_LOGS
        lwm2m_server_t * serverList = lwm2mH->serverList;
        fprintf(stderr, "** ** ** ** ** ** ** ** ** ** ** ** **\r\n");
        while (serverList != NULL)
        {
            fprintf(stderr, "** ** serverList: { shortID: %d, lifetime: %" PRIu64 ", location: %s }\r\n",
                serverList->shortID, serverList->lifetime, serverList->location);
            serverList = serverList->next;
        }
        fprintf(stderr, "** ** ** ** ** ** ** ** ** ** ** ** **\r\n");
#endif

        if (previousState == lwm2mH->state && previousState != STATE_BOOTSTRAPPING && previousState != STATE_REGISTERING) {
            fprintf(stdout, "/heartbeat:0:\r\n");
            fflush(stdout);
        } else {
            // Issue a command to notify state change
            switch (lwm2mH->state)
            {
            case STATE_INITIAL:
                fprintf(stdout, "/stateChanged:20:U1RBVEVfSU5JVElBTA==\r\n");
                break;
            case STATE_BOOTSTRAP_REQUIRED:
                fprintf(stdout, "/stateChanged:32:U1RBVEVfQk9PVFNUUkFQX1JFUVVJUkVE\r\n");
                break;
            case STATE_BOOTSTRAPPING:
                fprintf(stdout, "/stateChanged:28:U1RBVEVfQk9PVFNUUkFQUElORw==\r\n");
                break;
            case STATE_REGISTER_REQUIRED:
                fprintf(stdout, "/stateChanged:31:U1RBVEVfUkVHSVNURVJfUkVRVUlSRUQ=\r\n");
                break;
            case STATE_REGISTERING:
                fprintf(stdout, "/stateChanged:24:U1RBVEVfUkVHSVNURVJJTkc=\r\n");
                break;
            case STATE_READY:
                fprintf(stdout, "/stateChanged:16:U1RBVEVfUkVBRFk=\r\n");
                break;
            default:
                break;
            }
            fflush(stdout);
        }
        LOG_ARG("lwm2m_step() result => 0x%X", result);
#ifdef LWM2M_BOOTSTRAP
        if (result != 0)
        {
            fprintf(stderr, "lwm2m_step() failed: 0x%X\r\n", result);
            if(previousState == STATE_BOOTSTRAPPING)
            {
                LOG("[BOOTSTRAP] restore security and server objects");
                restore_object(objArray[0]); // LWM2M_SECURITY_OBJECT_ID
                restore_object(objArray[1]); // LWM2M_SERVER_OBJECT_ID
                lwm2mH->state = STATE_INITIAL;
            }
            else return -1;
        }
        update_bootstrap_info(&previousState, lwm2mH);
#endif

        if ((lwm2mH->state == STATE_READY) && (lwm2mH->observedList != NULL)) {
            // Issue an Observe command to poll an external process via stdout
            fprintf(stdout, "/observe:0:\r\n");
            fflush(stdout);
        }
        /*
         * This part will set up an interruption until an event happen on SDTIN or the socket until "tv" timed out (set
         * with the precedent function)
         */
        result = select(FD_SETSIZE, &readfds, NULL, NULL, &tv);

        if (result < 0)
        {
            if (errno != EINTR)
            {
              fprintf(stderr, "Error in select(): %d %s\r\n", errno, strerror(errno));
            }
        }
        else if (result > 0)
        {
            uint8_t buffer[data.maxPacketSize];
            int numBytes;

            /*
             * If an event happens on the socket
             */
            if (FD_ISSET(data.sock, &readfds))
            {
                struct sockaddr_storage addr;
                socklen_t addrLen;

                addrLen = sizeof(addr);

                /*
                 * We retrieve the data received
                 */
                numBytes = recvfrom(data.sock, buffer, data.maxPacketSize, 0, (struct sockaddr *)&addr, &addrLen);

                if (0 > numBytes)
                {
                    fprintf(stderr, "Error in recvfrom(): %d %s\r\n", errno, strerror(errno));
                }
                else if (0 < numBytes)
                {
                    char s[INET6_ADDRSTRLEN];
                    in_port_t port;

#ifdef WITH_TINYDTLS
                    dtls_connection_t * connP;
#else
                    connection_t * connP;
#endif
                    if (AF_INET == addr.ss_family)
                    {
                        struct sockaddr_in *saddr = (struct sockaddr_in *)&addr;
                        inet_ntop(saddr->sin_family, &saddr->sin_addr, s, INET6_ADDRSTRLEN);
                        port = saddr->sin_port;
                    }
                    else if (AF_INET6 == addr.ss_family)
                    {
                        struct sockaddr_in6 *saddr = (struct sockaddr_in6 *)&addr;
                        inet_ntop(saddr->sin6_family, &saddr->sin6_addr, s, INET6_ADDRSTRLEN);
                        port = saddr->sin6_port;
                    }
                    fprintf(stderr, "%d bytes received from [%s]:%hu\r\n", numBytes, s, ntohs(port));

                    /*
                     * Display it in the STDERR
                     */
                    if (data.showMessageDump) {
                        output_buffer(stderr, buffer, numBytes, 0);
                    }

                    connP = connection_find(data.connList, &addr, addrLen);
                    if (connP != NULL)
                    {
                        /*
                         * Let liblwm2m respond to the query depending on the context
                         */
#ifdef WITH_TINYDTLS
                        int result = connection_handle_packet(connP, buffer, numBytes);
                        if (0 != result)
                        {
                             fprintf(stderr, "error handling message %d\n",result);
                        }
#else
                        lwm2m_handle_packet(lwm2mH, buffer, numBytes, connP);
#endif
                    }
                    else
                    {
                        fprintf(stderr, "received bytes ignored!\r\n");
                    }
                }
            }
            // Handle `observe` command response from an external process via stdin
            // as the command is the only one initiated from the process.
            else if (FD_ISSET(STDIN_FILENO, &readfds))
            {
                uint8_t err = handle_observe_response(lwm2mH);
                fprintf(stderr, "lwm2mclient:err => %u\r\n", err);
            }
        }
    }

    /*
     * Finally when the loop is left smoothly - asked by user in the command line interface - we unregister our client from it
     */
    if (g_quit == 1)
    {
        lwm2m_close(lwm2mH);
    }
    close(data.sock);
    connection_free(data.connList);

    free_object(objArray[0]);
    free_object(objArray[1]);
    free_object(objArray[2]);
    free_object(objArray[3]);
    for (i = 0; i < objCount; i++) {
        free_object(objArray[i + 4]);
    }
    lwm2m_free(objArray);

#ifdef MEMORY_TRACE
    if (g_quit == 1)
    {
        trace_print(0, 1);
    }
#endif

    return 0;
}
