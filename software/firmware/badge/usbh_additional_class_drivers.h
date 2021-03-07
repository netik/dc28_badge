#ifndef _USBH_ADDITIONAL_CLASS_DRIVERS_H_
#define _USBH_ADDITIONAL_CLASS_DRIVERS_H_

#include "hal_usbh.h"

#if HAL_USE_USBH && HAL_USBH_USE_ADDITIONAL_CLASS_DRIVERS

/* Declarations */
extern const usbh_classdriverinfo_t usbhCustomHidDriverInfo;



/* Comma separated list of additional class drivers */
#define HAL_USBH_ADDITIONAL_CLASS_DRIVERS	\
	&usbhCustomHidDriverInfo,



#endif

#endif /* _USBH_ADDITIONAL_CLASS_DRIVERS_H_ */

