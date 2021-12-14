#ifndef _USBH_ADDITIONAL_CLASS_DRIVERS_H_
#define _USBH_ADDITIONAL_CLASS_DRIVERS_H_

#include "hal_usbh.h"

#if HAL_USE_USBH && HAL_USBH_USE_ADDITIONAL_CLASS_DRIVERS

/* Declarations */
extern const usbh_classdriverinfo_t usbhCustomHidDriverInfo;
extern const usbh_classdriverinfo_t usbhCustomCdcDriverInfo;



/* Comma separated list of additional class drivers */
#define HAL_USBH_ADDITIONAL_CLASS_DRIVERS	\
	&usbhCustomHidDriverInfo,		\
	&usbhCustomCdcDriverInfo



#endif

#endif /* _USBH_ADDITIONAL_CLASS_DRIVERS_H_ */

