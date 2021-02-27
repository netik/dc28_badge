
#define HAL_USE_COMMUNITY	TRUE
#define HAL_USE_FSMC		TRUE
#define HAL_USE_SDRAM           TRUE
#define HAL_USE_SRAM            TRUE

/* USB host driver support */

#define HAL_USE_USBH            TRUE
#define HAL_USBH_USE_HID        FALSE
#define HAL_USBH_USE_ADDITIONAL_CLASS_DRIVERS TRUE

#define HAL_USBHHID_USE_INTERRUPT_OUT                 FALSE
#define HAL_USBHHID_MAX_INSTANCES                     1

#define HAL_USBH_PORT_DEBOUNCE_TIME                   200
#define HAL_USBH_PORT_RESET_TIMEOUT                   500
#define HAL_USBH_DEVICE_ADDRESS_STABILIZATION         20
#define HAL_USBH_CONTROL_REQUEST_DEFAULT_TIMEOUT      OSAL_MS2I(1000)

#define USBH_DEBUG_ENABLE                             FALSE

#define USBH_DEBUG_BUFFER                             6000
#define USBH_DEBUG_OUTPUT_CALLBACK                    usbh_debug_output

#define USBH_DEBUG_SINGLE_HOST_SELECTION              USBHD2
#define USBH_DEBUG_MULTI_HOST                         FALSE
#define USBH_DEBUG_ENABLE_TRACE                       FALSE
#define USBH_DEBUG_ENABLE_INFO                        FALSE
#define USBH_DEBUG_ENABLE_WARNINGS                    FALSE
#define USBH_DEBUG_ENABLE_ERRORS                      FALSE

#define USBH_LLD_DEBUG_ENABLE_TRACE                   FALSE
#define USBH_LLD_DEBUG_ENABLE_INFO                    FALSE
#define USBH_LLD_DEBUG_ENABLE_WARNINGS                FALSE
#define USBH_LLD_DEBUG_ENABLE_ERRORS                  FALSE

#define USBHHID_DEBUG_ENABLE_TRACE                    FALSE
#define USBHHID_DEBUG_ENABLE_INFO                     FALSE
#define USBHHID_DEBUG_ENABLE_WARNINGS                 FALSE
#define USBHHID_DEBUG_ENABLE_ERRORS                   FALSE
