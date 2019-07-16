/*******************************************************************************
 *
 * Copyright (c) 2014 Bosch Software Innovations GmbH, Germany.
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
 *    Bosch Software Innovations GmbH - Please refer to git log
 *
 *******************************************************************************/

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

/*
 * lwm2mclient.h
 *
 *  General functions of lwm2m test client.
 *
 *  Created on: 22.01.2015
 *  Author: Achim Kraus
 *  Copyright (c) 2015 Bosch Software Innovations GmbH, Germany. All rights reserved.
 */

#ifndef LWM2MCLIENT_H_
#define LWM2MCLIENT_H_

#include "liblwm2m.h"
#ifdef WITH_TINYDTLS
#include "dtlsconnection.h"
#include "dtls_debug.h"
#else
#include "connection.h"
#endif

#define MAX_MESSAGE_SIZE 65536
#define MAX_RESOURCES 65536
#define URI_STRING_MAX_LEN 1024

extern int g_reboot;

typedef struct
{
    lwm2m_object_t * securityObjP;
    int sock;
#ifdef WITH_TINYDTLS
    dtls_connection_t * connList;
    lwm2m_context_t * lwm2mH;
#else
    connection_t * connList;
#endif
    int addressFamily;
    uint8_t showMessageDump;
} client_data_t;

/*
 * object_generic.c
 */
lwm2m_object_t * get_object(uint16_t objectId);
void free_object(lwm2m_object_t * objectP);
uint8_t handle_observe_response(lwm2m_context_t * lwm2mContext);
uint8_t backup_object(lwm2m_object_t * objectP);
uint8_t restore_object(lwm2m_object_t * objectP);

#endif /* LWM2MCLIENT_H_ */
