
/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef __GUAC_DRV_USER_H
#define __GUAC_DRV_USER_H

#include "config.h"
#include "agent.h"
#include "display.h"
#include "settings.h"

#include <guacamole/user.h>

/**
 * Guacamole user-specific data.
 */
typedef struct guac_drv_user_data {

    /**
     * The display to which the user is connected.
     */
    guac_drv_display* display;

    /**
     * The old button mask state.
     */
    int button_mask;

    /**
     * The settings provided by the user during the connection handshake when
     * they joined the connection.
     */
    guac_drv_settings* settings;

    /**
     * Agent X which acts on behalf of the Guacamole X.Org driver. If the agent
     * could not be started (a connection to the X server could not be
     * established), this will be NULL.
     */
    guac_drv_agent* agent;

} guac_drv_user_data;

/**
 * Handler for joining users.
 */
guac_user_join_handler guac_drv_user_join_handler;

/**
 * Handler for leaving users.
 */
guac_user_leave_handler guac_drv_user_leave_handler;

/**
 * Handler for size events.
 */
guac_user_size_handler guac_drv_user_size_handler;

/**
 * Handler for key events.
 */
guac_user_key_handler guac_drv_user_key_handler;

/**
 * Handler for mouse events.
 */
guac_user_mouse_handler guac_drv_user_mouse_handler;

#endif

