#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>

#include "hal.h"
#include "hal_usbh.h"
#include "usbh/internal.h"
#include "usb_cdc.h"
#include "usbnet_lld.h"

typedef struct _USBCDCDriver {
	/* inherited from abstract class driver */
	_usbh_base_classdriver_data

	usbh_device_t *		cdc_dev;
	usbh_ep_t		cdc_bulkin;
	usbh_ep_t		cdc_bulkout;
	uint8_t *		cdc_rxbuf;
	usbh_urb_t		cdc_in_urb;
	usbh_urb_t		cdc_out_urb;
	size_t			cdc_rxlen;
	int			notyet;
} USBCDCDriver;

USBCDCDriver USBCDCD;

static thread_t * pCdcThread;
static void * pCdcWa;

static void cdc_init (void);
static usbh_baseclassdriver_t * cdc_load (usbh_device_t * dev,
	const uint8_t * desc, uint16_t rem);
static void cdc_unload (usbh_baseclassdriver_t * drv);
static void cdc_send (uint8_t *, uint16_t);

static const usbh_classdriver_vmt_t cdc_driver_vmt = {
	cdc_init,
	cdc_load,
	cdc_unload
};

const usbh_classdriverinfo_t usbhCustomCdcDriverInfo = {
	"UMODEM", &cdc_driver_vmt
};

static THD_FUNCTION(usbCdcThread, arg)
{
	USBCDCDriver * cdcp;
	usbh_urbstatus_t status;
	uint32_t xfer_len;

	cdcp = arg;

	chRegSetThreadName ("USBCDCEvent");

	cdcp->cdc_rxlen = 0;

	while (1) {

		status = usbhBulkTransfer (&cdcp->cdc_bulkin,
		    cdcp->cdc_rxbuf + cdcp->cdc_rxlen,
		    CDC_MTU - cdcp->cdc_rxlen, &xfer_len,
		    TIME_INFINITE);

		if (status == USBH_URBSTATUS_DISCONNECTED)
			break;

		if (status == USBH_URBSTATUS_OK) {
			if (xfer_len == 37 || xfer_len == 27 ||
			    xfer_len == 10) {
				cdcp->cdc_rxlen = 0;
				continue;
			}
			cdcp->cdc_rxlen += xfer_len;
			if (cdcp->cdc_rxlen == CDC_MTU) {
				usbnetReceive (cdcp->cdc_rxbuf,
				    cdcp->cdc_rxlen);
				cdcp->cdc_rxlen = 0;
			}
		} else {
			printf ("CDC bulk in error: %x\n", status);
			cdcp->cdc_rxlen = 0;
		}
	}

	chSysLock ();
	chThdExitS (MSG_OK);

	return;
}

static void
cdc_init (void)
{
	memset (&USBCDCD, 0, sizeof(USBCDCD));
	USBCDCD.info = &usbhCustomCdcDriverInfo;

	return;
}

static usbh_baseclassdriver_t *
cdc_load (usbh_device_t *dev, const uint8_t * desc, uint16_t rem)
{
	USBCDCDriver * cdcp;
	const usbh_interface_descriptor_t * ifdesc;
	const usbh_endpoint_descriptor_t * epdesc;
	generic_iterator_t iep;
	generic_iterator_t icfg;
	if_iterator_t iif;
	bool if_found = false;

	/*
	 * Check if this device is:
	 * Communications interface class (0x02), subclass 0, protocol 0.
	 */

	if (_usbh_match_descriptor (desc, rem, USBH_DT_DEVICE,
	    0x02, 0x00, 0x00) != HAL_SUCCESS) {
		return (NULL);
	}

	/*
	 * Iterate over the interface list and try to find the data interface.
	 * Check CDC data class (0x0A) with subclass and protocol of 0.
	 */

	cfg_iter_init (&icfg, dev->fullConfigurationDescriptor,
	    dev->basicConfigDesc.wTotalLength);

	for (if_iter_init (&iif, &icfg); iif.valid; if_iter_next (&iif)) {
		ifdesc = if_get(&iif);

		if (ifdesc->bInterfaceClass == 0x0A &&
		    ifdesc->bInterfaceSubClass == 0x00 &&
		    ifdesc->bInterfaceProtocol == 0x00) {
			if_found = true;
			break;
		}
	}

	if (if_found == false)
		return (NULL);

	cdcp = &USBCDCD;
	cdcp->cdc_dev = dev;
	cdcp->cdc_bulkin.status = USBH_EPSTATUS_UNINITIALIZED;
	cdcp->cdc_bulkout.status = USBH_EPSTATUS_UNINITIALIZED;
	cdcp->cdc_rxbuf = memalign (CACHE_LINE_SIZE, CDC_MTU);

	/*
	 * Now iterate over the endpoint list, looking for the
	 * bulk in and bulk out pipes.
	 */

	for (ep_iter_init (&iep, &iif); iep.valid; ep_iter_next (&iep)) {
		epdesc = ep_get (&iep);
		if (epdesc->bmAttributes == USBH_EPTYPE_BULK) {
			if (epdesc->bEndpointAddress & USBH_EPDIR_IN) {
				usbhEPObjectInit (&cdcp->cdc_bulkin,
				    dev, epdesc);
				usbhEPSetName (&cdcp->cdc_bulkin,
				    "UMODEM[BIN ]");
				usbhEPOpen (&cdcp->cdc_bulkin);
			} else {
				usbhEPObjectInit (&cdcp->cdc_bulkout,
				    dev, epdesc);
				usbhEPSetName (&cdcp->cdc_bulkout,
				    "UMODEM[BOUT]");
				usbhEPOpen (&cdcp->cdc_bulkout);
			}
		}
	}

	USBNETD1.usb_tx_cb = cdc_send;
	usbnetEnable (&USBNETD1);

	pCdcWa = memalign (PORT_WORKING_AREA_ALIGN,
	    THD_WORKING_AREA_SIZE(1024));
	pCdcThread = chThdCreateStatic (pCdcWa,
	    THD_WORKING_AREA_SIZE(1024), NORMALPRIO + 1,
	    usbCdcThread, cdcp);

	cdc_send ((uint8_t *)"\r", 1);
	cdc_send ((uint8_t *)"badgenet\r", 9);

	return ((usbh_baseclassdriver_t *)cdcp);
}

static void
cdc_unload (usbh_baseclassdriver_t * drv)
{
	USBCDCDriver * cdcp;

	cdcp = (USBCDCDriver *)drv;

	usbnetDisable (&USBNETD1);
	USBNETD1.usb_tx_cb = NULL;

	if (pCdcThread != NULL) {
		chThdWait (pCdcThread);
		free (pCdcWa);
		pCdcThread = NULL;
	}

	usbhEPClose (&cdcp->cdc_bulkin);
	usbhEPClose (&cdcp->cdc_bulkout);

	free (cdcp->cdc_rxbuf);
	cdcp->cdc_dev = NULL;
	cdcp->cdc_rxbuf = NULL;

	return;
}

static void
cdc_send (uint8_t * p, uint16_t len)
{
	USBCDCDriver * cdcp;
	usbh_urbstatus_t status;
	uint32_t xfer_len;

	cdcp = &USBCDCD;

	status = usbhBulkTransfer (&cdcp->cdc_bulkout, p, len,
	    &xfer_len, TIME_INFINITE);

	if (status == USBH_URBSTATUS_DISCONNECTED)
		return;

	if (status != USBH_URBSTATUS_OK)
		printf ("CDC bulk out data error: %x\n", status);

	status = usbhBulkTransfer (&cdcp->cdc_bulkout, p, 0,
	    &xfer_len, TIME_INFINITE);

	if (status == USBH_URBSTATUS_DISCONNECTED)
		return;

	if (status != USBH_URBSTATUS_OK)
		printf ("CDC bulk out trailer error: %x\n", status);

	return;
}
