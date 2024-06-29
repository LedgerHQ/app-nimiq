/*******************************************************************************
 *   Ledger Nimiq App
 *   (c) 2018 Ledger
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 ********************************************************************************/

#ifndef _NIMIQ_UX_BAGL_MACROS_H_
#define _NIMIQ_UX_BAGL_MACROS_H_

#ifdef HAVE_BAGL

#include "ux.h"

/**
 * Similar to UX_STEP_NOCB defined in ux_flow_engine.h but with a special init method that displays this step only if
 * the given condition is fulfilled and skips it otherwise by automatically going to the next or previous step.
 */
// Code inspired by UX_STEP_NOCB_INIT
#define UX_OPTIONAL_STEP_NOCB(stepname, layoutkind, display_condition, ...) \
    void stepname ##_init (unsigned int stack_slot) { \
        if (display_condition) { \
            ux_layout_ ## layoutkind ## _init(stack_slot); \
        } else { \
            if ( \
                /* going forward and we're not the last step */ \
                (ux_flow_direction() != FLOW_DIRECTION_BACKWARD && !ux_flow_is_last()) \
                /* we're the first step, thus go to the next */ \
                || ux_flow_is_first() \
            ) { \
                ux_flow_next(); \
            } else { \
                ux_flow_prev(); \
            } \
        } \
    } \
    const ux_layout_ ## layoutkind ## _params_t stepname ##_val = __VA_ARGS__; \
    const ux_flow_step_t stepname = { \
        stepname ##  _init, \
        & stepname ## _val, \
        NULL, \
        NULL, \
    }

/**
 * Similar to UX_STEP_CB defined in ux_flow_engine.h but with a special init method that displays this step only if
 * the given condition is fulfilled and skips it otherwise by automatically going to the next or previous step.
 */
#define UX_OPTIONAL_STEP_CB(stepname, layoutkind, display_condition, validate_cb, ...) \
    UX_FLOW_CALL(stepname ## _validate, { validate_cb; }) \
    void stepname ##_init (unsigned int stack_slot) { \
        if (display_condition) { \
            ux_layout_ ## layoutkind ## _init(stack_slot); \
        } else { \
            if ( \
                /* going forward and we're not the last step */ \
                (ux_flow_direction() != FLOW_DIRECTION_BACKWARD && !ux_flow_is_last()) \
                /* we're the first step, thus go to the next */ \
                || ux_flow_is_first() \
            ) { \
                ux_flow_next(); \
            } else { \
                ux_flow_prev(); \
            } \
        } \
    } \
    const ux_layout_ ## layoutkind ## _params_t stepname ##_val = __VA_ARGS__; \
    const ux_flow_step_t stepname = { \
        stepname ##  _init, \
        & stepname ## _val, \
        stepname ## _validate, \
        NULL, \
    }

#endif // HAVE_BAGL

#endif // _NIMIQ_UX_BAGL_MACROS_H_
