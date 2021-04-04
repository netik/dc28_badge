#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "ch.h"
#include "hal.h"

#include "orchard-app.h"
#include "orchard-ui.h"
#include "fontlist.h"

#include "userconfig.h"
#include "gfx.h"

#include "ides_gfx.h"
#ifdef notyet
#include "unlocks.h"
#endif

#include "src/gdriver/gdriver.h"
#include "src/ginput/ginput_driver_mouse.h"

#include "stm32sai_lld.h"

/* GHandles */
typedef struct _SetupHandles {
	GHandle ghCheckSound;
	GHandle ghCheckAirplane;
	GHandle ghCheckRotate;
	GHandle ghButtonOK;
	GHandle ghButtonCalibrate;
	GHandle ghButtonSetName;
	GListener glSetup;
} SetupHandles;

static uint32_t last_ui_time = 0;

static gFont fontSM;
static gFont fontXS;

static void
draw_setup_buttons (SetupHandles * p)
{
	userconfig * config;
	GWidgetInit wi;

	config = configGet ();

	gwinSetDefaultFont(fontSM);
	gwinWidgetClearInit(&wi);

	/* create checkbox widget: ghCheckSound */

	wi.g.show = TRUE;
	wi.g.x = 10;
	wi.g.y = 120;
	wi.g.width = 180;
	wi.g.height = 20;
	wi.text = "Sounds ON";
	wi.customDraw = gwinCheckboxDraw_CheckOnLeft;
	wi.customParam = 0;
	wi.customStyle = 0;
	p->ghCheckSound = gwinCheckboxCreate (0, &wi);
	gwinCheckboxCheck (p->ghCheckSound, config->cfg_sound);

	/* create checkbox widget: ghCheckAirplane */
	wi.g.show = TRUE;
	wi.g.y = 150;
	wi.text = "Airplane Mode";
	wi.customDraw = gwinCheckboxDraw_CheckOnLeft;
	p->ghCheckAirplane = gwinCheckboxCreate (0, &wi);
	gwinCheckboxCheck (p->ghCheckAirplane, config->cfg_airplane);

	/* create checkbox widget: ghCheckRotate */
	wi.g.show = TRUE;
	wi.g.y = 180;
	wi.text = "Rotate display";
	wi.customDraw = gwinCheckboxDraw_CheckOnLeft;
	p->ghCheckRotate = gwinCheckboxCreate (0, &wi);
	gwinCheckboxCheck (p->ghCheckRotate, config->cfg_orientation);

	/* create button widget: ghButtonCalibrate */
	wi.g.show = TRUE;
	wi.g.x = 200;
	wi.g.y = 110;
	wi.g.width = 110;
	wi.g.height = 36;
	wi.customDraw = gwinButtonDraw_Normal;
	wi.customParam = 0;
	wi.customStyle = &DarkPurpleFilledStyle;

	wi.text = "Touch Cal";
	p->ghButtonCalibrate = gwinButtonCreate (0, &wi);

	/* create button widget: ghButtonSetName */
	wi.g.show = TRUE;
	wi.g.y = 150;
	wi.text = "Set Name";
	p->ghButtonSetName = gwinButtonCreate(0, &wi);

	/* create button widget: ghButtonOK */
	wi.g.show = TRUE;
	wi.g.y = 190;
	wi.text = "Save";
	p->ghButtonOK = gwinButtonCreate (0, &wi);

	return;
}

static uint32_t
setup_init (OrchardAppContext *context)
{
	(void)context;
	last_ui_time = chVTGetSystemTime ();
	return 0;
}

static void
setup_start (OrchardAppContext *context)
{
	SetupHandles * p;

	fontSM = gdispOpenFont (FONT_SM);
	fontXS = gdispOpenFont (FONT_XS);

	gdispClear (GFX_BLACK);

	p = malloc (sizeof(SetupHandles));
	draw_setup_buttons(p);
	context->priv = p;

	/* idle ui timer (30s) */
	last_ui_time = chVTGetSystemTime();
	orchardAppTimer (context, 30000000, true);

	geventListenerInit (&p->glSetup);
	gwinAttachListener (&p->glSetup);
	geventRegisterCallback (&p->glSetup,
	    orchardAppUgfxCallback, &p->glSetup);

	return;
}

static void
setup_checkbox_event (SetupHandles * p, GEvent * pe, userconfig * c)
{
	/* handle checkbox state changes */

	if (((GEventGWinCheckbox*)pe)->gwin == p->ghCheckSound) {
		if (((GEventGWinCheckbox*)pe)->isChecked == gTrue)
			c->cfg_sound = CONFIG_SOUND_ON;
		else
			c->cfg_sound = CONFIG_SOUND_OFF;
		if (c->cfg_sound)
			i2sEnabled = TRUE;
		else
			i2sEnabled = FALSE;
      	}

	if (((GEventGWinCheckbox*)pe)->gwin == p->ghCheckAirplane) {
		if (((GEventGWinCheckbox*)pe)->isChecked == gTrue)
			c->cfg_airplane = CONFIG_RADIO_OFF;
		else
			c->cfg_airplane = CONFIG_RADIO_ON;
#ifdef notyet
		if (c->cfg_airplane)
			/* disable radio */
		else
			/* enable radio */
#endif
	}


	if (((GEventGWinCheckbox*)pe)->gwin == p->ghCheckRotate) {
		if (((GEventGWinCheckbox*)pe)->isChecked == gTrue)
			c->cfg_orientation = CONFIG_ORIENT_PORTRAIT;
		else
			c->cfg_orientation = CONFIG_ORIENT_LANDSCAPE;
 	}

	return;
}

static void
setup_button_event (SetupHandles * p, GEvent * pe, userconfig * c)
{
	GMouse * m;

	/* Handle button state changes */

	if (((GEventGWinButton*)pe)->gwin == p->ghButtonOK) {
		configSave ();
          	orchardAppExit ();
          	return;
	}

	if (((GEventGWinButton*)pe)->gwin == p->ghButtonCalibrate) {
		/*
		 * We need to allow some time for the widget redaw
		 * for the "TOUCH CAL" button to complete before we
		 * start the calibrator, otherwise the redraw might
		 * corrupt the display.
		 */
		chThdSleepMilliseconds (200);

		/* Detach the event handler from the mouse */
		geventDetachSource (&p->glSetup, NULL);
		geventRegisterCallback (&p->glSetup, NULL, NULL);
		/* Run the calibration GUI */
		(void)ginputCalibrateMouse (0);

		/* Save the calibration data */
		m = (GMouse*)gdriverGetInstance (GDRIVER_TYPE_MOUSE, 0);
		memcpy (&c->cfg_touch_data, &m->caldata,
		    sizeof(c->cfg_touch_data));
		c->cfg_touch_data_present = 1;

		configSave ();
		orchardAppExit();
		return;
	}

	if (((GEventGWinButton*)pe)->gwin == p->ghButtonSetName) {
		configSave (); /* Is this needed? */
		orchardAppRun (orchardAppByName ("Set your name"));
		return;
	}

	return;
}

static void
setup_event(OrchardAppContext *context,
	const OrchardAppEvent *event)
{
	GEvent * pe;
	userconfig * config;
	SetupHandles * p;

	config = configGet ();

	p = context->priv;

#ifdef notdef
	if (event->type == appEvent && event->app.event == appStart)
		badge_cpu_speed_set (BADGE_CPU_SPEED_SLOW);
#endif

	/* handle events */
	if (event->type == radioEvent) {
		/* Ignore radio events */
 		return;
	}

	/* idle timeout, return to home screen */
	if (event->type == timerEvent) {
		if ((chVTGetSystemTime() - last_ui_time) >
		    (UI_IDLE_TIME * 1000) && last_ui_time != 0) {
			orchardAppRun (orchardAppByName("Badge"));
		}
		return;
	}

	if (event->type == keyEvent && event->key.flags == keyPress) {
		last_ui_time = chVTGetSystemTime();
		i2sPlay("sound/click.snd");
	}

	if (event->type == ugfxEvent) {
		pe = event->ugfx.pEvent;
		last_ui_time = chVTGetSystemTime();
		i2sPlay("sound/click.snd");
		switch(pe->type) {
			case GEVENT_GWIN_CHECKBOX:
				setup_checkbox_event (p, pe, config);
				break;
			case GEVENT_GWIN_BUTTON:
				setup_button_event (p, pe, config);
				break;
			default:
				break;
		}
	}

	return;
}

static void
setup_exit (OrchardAppContext *context)
{
	SetupHandles * p;

#ifdef notdef
	badge_cpu_speed_set (BADGE_CPU_SPEED_NORMAL);
#endif

	p = context->priv;

	gwinDestroy(p->ghCheckSound);
	gwinDestroy(p->ghCheckAirplane);
	gwinDestroy(p->ghCheckRotate);
	gwinDestroy(p->ghButtonOK);
	gwinDestroy(p->ghButtonCalibrate);
	gwinDestroy(p->ghButtonSetName);

	gdispCloseFont (fontXS);
	gdispCloseFont (fontSM);

	geventDetachSource (&p->glSetup, NULL);
	geventRegisterCallback (&p->glSetup, NULL, NULL);

	free (context->priv);
	context->priv = NULL;

	return;
}

orchard_app("Setup", "icons/wheel.rgb", 0, setup_init, setup_start,
    setup_event, setup_exit, 2);
