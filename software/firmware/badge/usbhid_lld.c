/*-
 * Copyright (c) 2021
 *      Bill Paul <wpaul@windriver.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ch.h"
#include "hal.h"

#include "usbhid_lld.h"
#include "capture.h"

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

/*
 * ChibiOS has some USB host support, however it's still experimental,
 * and it has a few limitations:
 *
 * 1) The STM32 USBH driver doesn't support USB 2.0/HS mode with an external
 *    ULPI PHY.
 *
 * 2) It expects you to call usbhMainLoop() over and over to poll for
 *    controller state changes.
 *
 * 3) The USBHID driver has no callbacks to notify you asynchronously
 *    when HID device is connected/disconnected.
 *
 * 4) The USBHID driver invokes the user-supplied report callback
 *    directly from interrupt context.
 *
 * The first two issues have been addressed by making some tweaks to
 * the STM32 USBH driver. The only reason we want USB 2.0/HS mode to
 * work is that the STM32F746 Discovery board has its second USB port
 * configured that way and we'd like to be able to use it for testing
 * (this way the first USB port can be used for the virtual COM port
 * function without conflicting with USB keyboard support).
 *
 * Dealing with the third issue is a little more complicated. The most
 * obvious approaches would be to either modify the USBHID driver or
 * else replace it entirely. I would prefer to keep my fingers out of
 * the USBHID driver code as it's a bit grotty for my taste, and I don't
 * want to write a whole new driver as that would result in a lot of
 * duplication.
 *
 * So as a compromise, we provide our own custom USB driver which
 * includes the source of the existing USBH driver, and we wrap its
 * init/load/unload methods with some of our own. This allows us to
 * hook the load/unload events without needing to change the USBHID
 * code itself.
 *
 * We also provide our own routine to read from the interrupt pipe
 * in order to obtain keyboard reports. We want to be able to decode
 * the events and push the keycodes into the same queue that's used
 * by the virtual keyboard handler. The problem is, that queue is
 * guarded using a mutex, and you can't acquire a mutex in interrupt
 * context. So rather than using the URB callback mechanism, we wait
 * for the URB complete in thread context, and then we can post the
 * keycode events in the USB HID handler thread rather than than from
 * the USB host controller driver's ISR.
 */

#undef HAL_USBH_USE_HID
#define HAL_USBH_USE_HID	TRUE
#include "../ChibiOS/community/os/hal/src/usbh/hal_usbh_hid.c"

static const uint8_t ukbd_trmap[256] = {
	0x00, 0x00, 0x00, 0x00, 'a', 'b', 'c', 'd',
	'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',
	'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
	'u', 'v', 'w', 'x', 'y', 'z', '1', '2',
	'3', '4', '5', '6', '7', '8', '9', '0',	
	0x0D, 0x1B, 0x08, 0x09, ' ', '-', '=', '[',
	']', '\\', '#', ';', '\'', '`', ',', '.',
	'/',  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static void _my_hid_init (void);
static usbh_baseclassdriver_t * _my_hid_load (usbh_device_t *dev,
	const uint8_t * descriptor, uint16_t rem);
static void _my_hid_unload (usbh_baseclassdriver_t * drv);
static void usbhidStart (usbh_baseclassdriver_t * pDrv);
static void usbhidStop (usbh_baseclassdriver_t * pDrv);

static thread_t * pUsbhThread;
static thread_t * pHidThread;
static void * pHidWa;

static const usbh_classdriver_vmt_t my_class_driver_vmt = {
        _my_hid_init,
        _my_hid_load,
        _my_hid_unload
};

const usbh_classdriverinfo_t usbhCustomHidDriverInfo = {
        "Badge HID", &my_class_driver_vmt
};

static void
_my_hid_init (void)
{
	USBHHIDDriver * hidp;

	_hid_init ();

	hidp = &USBHHIDD[0];
	hidp->info = &usbhCustomHidDriverInfo;

	return;
}

static usbh_baseclassdriver_t *
_my_hid_load(usbh_device_t *dev, const uint8_t * descriptor, uint16_t rem)
{
	usbh_baseclassdriver_t * drv;

	drv = _hid_load (dev, descriptor, rem);

 	/* A USB HID device has been detected: invoke our connection hook. */

	if (drv != NULL)
		usbhidStart (drv);

	return (drv);
}

static void
_my_hid_unload (usbh_baseclassdriver_t * drv)
{
 	/* A USB HID device has been removed: invoke our disconnection hook. */

	usbhidStop (drv);

	_hid_unload (drv);

	return;
}

static void
ukbdCapture (uint8_t k, int press)
{
	uint32_t sym;

	sym = ukbd_trmap[k];
	if (sym == 0)
		sym = k | 0x40000000;

	if (press)
		sym |= CAPTURE_KEY_DOWN;
	else
		sym |= CAPTURE_KEY_UP;

	capture_queue_put (sym);
	return;
}

static bool
ukbdKeyIsSet (uint8_t k, ukbd_report * p)
{
	int i;

	for (i = 0; i < UKBD_KEYS; i++) {
		if (p->ukbd_keys[i] == k)
			return (TRUE);
	}

	return (FALSE);
}

static void
ukbdKeyClear (uint8_t k, ukbd_report * p)
{
	int i;

	for (i = 0; i < UKBD_KEYS; i++) {
		if (p->ukbd_keys[i] == k) {
			p->ukbd_keys[i] = 0;
			break;
		}
	}

	return;
}

static void
ukbdKeyAdd (uint8_t k, ukbd_report * p)
{
	int i;

	for (i = 0; i < UKBD_KEYS; i++) {
		if (p->ukbd_keys[i] == 0) {
			p->ukbd_keys[i] = k;
			break;
		}
	}

	return;
}

static void
ukbdCheckKeyPressed (ukbd_report * pCur, ukbd_report * pSaved)
{
	int i;
	uint8_t k;

	for (i = 0; i < UKBD_KEYS; i++) {
		k = pCur->ukbd_keys[i];
		if (k == UKBD_CODE_RESERVED || k == UKBD_CODE_ERR_ROLLOVER)
			continue;
		if (ukbdKeyIsSet (k, pSaved) == FALSE) {
			ukbdKeyAdd (k, pSaved);
			ukbdCapture (k, 1);
		}
	}

	return;
}

static void
ukbdCheckKeyReleased (ukbd_report * pCur, ukbd_report * pSaved)
{
	int i;
	uint8_t k;

	for (i = 0; i < UKBD_KEYS; i++) {
		if (pCur->ukbd_keys[i] == UKBD_CODE_ERR_ROLLOVER)
			continue;
		k = pSaved->ukbd_keys[i];
		if (ukbdKeyIsSet (k, pCur) == FALSE) {
			ukbdKeyClear (k, pSaved);
			ukbdCapture (k, 0);
		}
	}

	return;
}

static uint8_t
ukbdModifierToScancode (uint8_t m)
{
	uint8_t s = 0;

	switch (m) {
		case UKBD_MOD_CTRL_L:
			s = UKBD_KEY_CTRL_L;
			break;
		case UKBD_MOD_SHIFT_L:
			s = UKBD_KEY_SHIFT_L;
			break;
		case UKBD_MOD_ALT_L:
			s = UKBD_KEY_ALT_L;
			break;
		case UKBD_MOD_METAL_L:
			s = UKBD_KEY_META_L;
			break;
		case UKBD_MOD_CTRL_R:
			s = UKBD_KEY_CTRL_R;
			break;
		case UKBD_MOD_SHIFT_R:
			s = UKBD_KEY_SHIFT_R;
			break;
		case UKBD_MOD_ALT_R:
			s = UKBD_KEY_ALT_R;
			break;
		case UKBD_MOD_METAL_R:
			s = UKBD_KEY_META_R;
			break;
		default:
			printf ("unknown modifier mask: %x\n", m);
			break;
	}

	return (s);
}

static void
ukbdCheckModifierPressed (ukbd_report * pCur, ukbd_report * pSaved)
{
	int i;
	uint8_t m;

	for (i = 0; i < UKBD_MODIFIERS; i++) {
		m = 1 << i;
		if (pCur->ukbd_modifiers & m &&
		    !(pSaved->ukbd_modifiers & m)) {
			pSaved->ukbd_modifiers |= m;
			ukbdCapture (ukbdModifierToScancode (m), 1);
		}
	}

	return;
}

static void
ukbdCheckModifierReleased (ukbd_report * pCur, ukbd_report * pSaved)
{
	int i;
	uint8_t m;

	for (i = 0; i < UKBD_MODIFIERS; i++) {
		m = 1 << i;
		if (pSaved->ukbd_modifiers & m &&
		    !(pCur->ukbd_modifiers & m)) {
			pSaved->ukbd_modifiers &= ~m;
			ukbdCapture (ukbdModifierToScancode (m), 0);
		}
	}

	return;
}

static THD_FUNCTION(usbhThread, arg)
{
	(void)arg;

	/*
	 * This loop runs forever, but usbhMainLoop() will sleep
	 * until there's activity on the USB port, so we won't
	 * end up wasting CPU time when the controller is idle.
	 */

	while (1)
		usbhMainLoop (&USBHD2);

	/* NOTREACHED */
	return;
}

static void
usbHidUrbCb (usbh_urb_t * pUrb)
{
	thread_reference_t * t;

	osalSysLockFromISR ();
	t = pUrb->userData;
	if (*t != NULL)
		osalThreadResumeI (t, MSG_OK);
	osalSysUnlockFromISR ();

	return;
}

static THD_FUNCTION(usbHidThread, arg)
{
	USBHHIDDriver * hidp;
	thread_reference_t t;
	uint32_t interval = 0;
	ukbd_report rbuf;
	ukbd_report saved;

        chRegSetThreadName ("USBHIDEvent");

	memset (&rbuf, 0, sizeof(rbuf));
	memset (&saved, 0, sizeof(saved));
	memset (&t, 0, sizeof(t));

	hidp = arg;

	chSemWait (&hidp->sem);

	usbhEPOpen (&hidp->epin);

	usbhhidSetProtocol (hidp, USBHHID_PROTOCOL_BOOT);

	usbhhidSetIdle (hidp, 0, 0);

	hidp->state = USBHHID_STATE_READY;

	chSemSignal (&hidp->sem);

	/*
	 * We're allowed to choose a polling interval based on what
	 * the device reports in its descriptor.
	 */
		
	if (hidp->epin.bInterval <= 15)
		interval = 8;
	if (hidp->epin.bInterval >= 16 && hidp->epin.bInterval <= 35)
		interval = 16;
	if (hidp->epin.bInterval >= 36)
		interval = 32;

	/*
	 * In what I could consider a poor design choice, USB keyboards
	 * report their key changes using a USB interrupt pipe instead
	 * of a bulk input pipe. The problem with this is that when you
	 * issue an URB request to an interrupt pipe, it will complete
	 * fairly quickly with a timeout (NAK) notification if there's
	 * no data pending. By contrast, if you issue a request to a
	 * bulk in pipe and there's no data pending, the request will
	 * just stay in the pipe's pending queue and only complete once
	 * the device has data to send.
	 *
	 * The latter approach is preferable because it allows the host
	 * to stay idle until there's really a need to process any I/O,
	 * whereas with the former, you have to keep polling.
	 */

	while (1) {
		chSemWait (&hidp->sem);

		usbhURBObjectInit (&hidp->in_urb, &hidp->epin,
		    usbHidUrbCb, (void *)&t, (uint8_t *)&rbuf,
		    hidp->epin.wMaxPacketSize);
        	usbhURBSubmit (&hidp->in_urb);

		chSemSignal (&hidp->sem);

		osalSysLock ();
		/* This avoids a race condition with the callback routine */
		if (hidp->in_urb.status == USBH_URBSTATUS_PENDING)
			osalThreadSuspendS (&t);
		osalSysUnlock ();

		/* If the device was unplugged, then bail. */

		if (hidp->in_urb.status == USBH_URBSTATUS_DISCONNECTED) {

			/*
			 * When the keyboard is unplugged we should act
			 * as if any keys that were pressed prior to the
			 * unplug event have been released.
			 */

			memset (&rbuf, 0, sizeof(rbuf));
			ukbdCheckKeyReleased (&rbuf, &saved);
			ukbdCheckModifierReleased (&rbuf, &saved);
			break;
		}

		if (hidp->in_urb.status == USBH_URBSTATUS_OK) {
			ukbdCheckKeyPressed (&rbuf, &saved);
			ukbdCheckKeyReleased (&rbuf, &saved);
			ukbdCheckModifierPressed (&rbuf, &saved);
			ukbdCheckModifierReleased (&rbuf, &saved);
		}
		chThdSleepMilliseconds (interval);
	}

	chSysLock ();
	chThdExitS (MSG_OK);

	return;
}

static void
usbhidStart (usbh_baseclassdriver_t * drv)
{
	USBHHIDDriver * hidp;
	usbh_device_t * devp;

	hidp = (USBHHIDDriver *)drv;
	devp = hidp->epin.device;

	/*
	 * Check if this HID device is a boot keyboard because
	 * this is all we support.
	 */

	if (usbhhidGetType (hidp) != USBHHID_DEVTYPE_BOOT_KEYBOARD)
		return;

	printf ("Keyboard connected, vendor 0x%04X dev 0x%04X\n",
	    devp->devDesc.idVendor, devp->devDesc.idProduct);

	/* Start the keyboard handler thread */

	if (usbhhidGetState (hidp) == USBHHID_STATE_ACTIVE) {
		pHidWa = memalign (PORT_WORKING_AREA_ALIGN,
		    THD_WORKING_AREA_SIZE(1024));
		if (pHidWa == NULL)
			return;
		pHidThread = chThdCreateStatic (pHidWa,
		    THD_WORKING_AREA_SIZE(1024), NORMALPRIO + 1,
		    usbHidThread, hidp);
	}

	return;
}

static void
usbhidStop (usbh_baseclassdriver_t * drv)
{
	USBHHIDDriver * hidp;

	hidp = (USBHHIDDriver *)drv;

	usbhhidStop (hidp);

	/* Wait for the keyboard handler thread to exit */

	if (pHidThread != NULL) {
		chThdWait (pHidThread);
		free (pHidWa);
		pHidThread = NULL;
		printf ("Keyboard disconnected!\n");
	}

	return;
}

void
usbHostStart (void)
{
	/*
	 * Launch the host controller event handler thread.
	 */

	pUsbhThread = chThdCreateFromHeap (NULL, THD_WORKING_AREA_SIZE(1024),
	    "USBHEvent", 2 /*NORMALPRIO + 1*/, usbhThread, NULL);

	return;
}
