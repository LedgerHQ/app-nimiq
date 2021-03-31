#ifndef _UX_MACROS_H_
#define _UX_MACROS_H_

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


#endif // _UX_MACROS_H_
