/*-
 * Copyright (c) 2020
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

#include "orchard-app.h"
#include "ides_gfx.h"
#include "fontlist.h"

#include "stm32sai_lld.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <math.h>

typedef struct dialer_button {
	gCoord		button_x;
	gCoord		button_y;
	char *		button_text;
	uint16_t	button_freq_a;
	uint16_t	button_freq_b;
	uint16_t	button_samples;
} DIALER_BUTTON;

#define DIALER_MAXBUTTONS 19
#define DIALER_BLUEBOX

/*
 * Pre-computed sample counts are based on a sample rate of
 * 31250 Hz. The original DC25 code used 29700Hz. 31250Hz is as close
 * as we can get with the STM32 I2S module.
 *
 * What is DIALER_OFFSET for, you ask? I'm actually not sure why, but
 * there seems to be a discrepancy between the programmed sample rate
 * values listed in the manual and the actual playback rate. If we
 * program the I2S controller for an LRCLK value of X and then synthesize
 * our samples with a DIALER_SAMPLERATE of X, the resulting touch tones
 * end up too high in frequency. To compensate we use a sample rate of
 * X + 400Hz. I'm not sure if this discrepancy is in the I2S controller
 * block or the CS4344 codec, but applying this fixup is the only way
 * to get the touch tones generated accurately. (This has been tested
 * using a hardware touch tone decoder chip.)
 */


#define DIALER_OFFSET		400	/* fixup */
#define DIALER_I2SRATE		31250	/* actual I2S LRCLK rate */	
#define DIALER_SAMPLERATE	(DIALER_I2SRATE + DIALER_OFFSET)

static const DIALER_BUTTON buttons[] =  {
	{ 0,   0,   "1",     1209, 697,  1170  },
	{ 60,  0,   "2",     1336, 697,  1035  },
	{ 120, 0,   "3",     1477, 697,  315  },
	{ 180, 0,   "A",     1633, 697,  855  }, 

	{ 0,   60,  "4",     1209, 770,  1066 },
	{ 60,  60,  "5",     1336, 770,  943  },
	{ 120, 60,  "6",     1477, 770,  861  },
	{ 180, 60,  "B",     1633, 770,  779  }, 

	{ 0,   120, "7",     1209, 852,  962  },
	{ 60,  120, "8",     1336, 852,  851  },
	{ 120, 120, "9",     1477, 852,  777  },
	{ 180, 120, "C",     1633, 852,  703  }, 

	{ 0,   180, "*",     1209, 941,  858  },
	{ 60,  180, "0",     1336, 941,  759  },
        { 120, 180, "#",     1477, 941,  231  },
	{ 180, 180, "D",     1633, 941,  627  },

	{ 0,   240, "2600",  2600, 2600, 1020  },
#ifdef DIALER_BLUEBOX
	{ 130, 240, "Blue Box", 0, 0,    0    },
#else
	{ 130, 240, "",      0,    0,    0    },
#endif
	{ 70,  280, "Exit",  0,    0,    0    },
#ifdef DIALER_BLUEBOX
	{ 0,   0,   "1",     700,  900,  315  },
	{ 60,  0,   "2",     700,  1100, 1260 },
	{ 120, 0,   "3",     900,  1100, 140  },
	{ 180, 0,   "KP1",   1100, 1700, 252  }, 

	{ 0,   60,  "4",     700,  1300, 360  },
	{ 60,  60,  "5",     900,  1300, 840  },
	{ 120, 60,  "6",     1100, 1300, 168  },
	{ 180, 60,  "KP2",   1300, 1700, 72   }, 

	{ 0,   120, "7",     700,  1500, 315  },
	{ 60,  120, "8",     900,  1500, 105  },
	{ 120, 120, "9",     1100, 1500, 84   },
	{ 180, 120, "ST",    1500, 1700, 126  }, 

	{ 0,   180, "Code1", 700,  1700, 90   },
	{ 60,  180, "0",     1300, 1500, 168  },
        { 120, 180, "Code2", 900,  1700, 630  },
	{ 180, 180,  "",     0,    0,    0    },

	{ 0,   240, "2600",  2600, 2600, 1020 },
	{ 130, 240, "Silver Box",0,0,    0    },
	{ 70,  280, "Exit",  0,    0,    0    }
#endif
};

typedef struct _DHandles {
	/* GListeners */
	GListener		glDListener;

	/* GHandles */
	GHandle			ghButtons[DIALER_MAXBUTTONS];
#ifdef DIALER_BLUEBOX
	int			mode;
#endif

	gFont			font;

	gOrientation		o;
} DHandles;

#ifdef CALIBRATION

#include "badge.h"

#define DIALER_MATCHES 6
#define DIALER_SAMPLES 4096

static uint16_t buf[DIALER_SAMPLES];

void
calc (uint8_t b)
{
	uint32_t i;
	double fract1;
	double fract2;
	double point1;
	double point2;
	double result;
	double pi;
	uint16_t freqa;
	uint16_t freqb;
	uint16_t matches[DIALER_MATCHES];

	freqa = buttons[b].button_freq_a;
	freqb = buttons[b].button_freq_b;

	if (freqa == 0 && freqa == 0)
		return;

	pi = (3.14159265358979323846264338327950288 * 2);

	fract1 = (double)(pi)/(double)(DIALER_SAMPLERATE/freqa);
	fract2 = (double)(pi)/(double)(DIALER_SAMPLERATE/freqb);

	for (i = 0; i < DIALER_SAMPLES; i++) {
		point1 = fract1 * i;
		result = 16383 + (sin (point1) * 16383);
		point2 = fract2 * i;
		result += 16383 + (sin (point2) * 16383);
		result /= 2.0;
		buf[i] = (uint16_t)round (result);
		buf[i] = ~(buf[i]) + 1;
		if (i < DIALER_MATCHES)
			matches[i] = buf[i];
		if (i > 50) {
			if (memcmp (&buf[i - DIALER_MATCHES], matches,
			    sizeof(uint16_t) * DIALER_MATCHES) == 0)
				break;
		}
	}

	i -= DIALER_MATCHES;

	printf ("SAMPLES: %d\r\n", i);

	return;
}
#endif

static void
dialer_i2s_init (void)
{
	i2sWait ();
	i2sSamplesStop ();
	saiSpeed (&SAID2, I2S_SPEED_FAST);
	saiStereo (&SAID2, FALSE);
	return;
}

static void
dialer_i2s_restore (void)
{
	saiStereo (&SAID2, TRUE);
	saiSpeed (&SAID2, I2S_SPEED_NORMAL);
	return;
}

static void
tonePlay (GWidgetObject * w, uint8_t b, uint32_t duration)
{
	uint32_t i;
	double fract1;
	double fract2;
	double point1;
	double point2;
	double result;
	double pi;
	uint16_t * buf;
	uint16_t freqa;
	uint16_t freqb;
	uint16_t samples;

	freqa = buttons[b].button_freq_a;
	freqb = buttons[b].button_freq_b;

	if (freqa == 0 && freqa == 0)
		return;

	samples = buttons[b].button_samples;

	buf = memalign (CACHE_LINE_SIZE, (samples * sizeof(uint16_t)));

	pi = (3.14159265358979323846264338327950288 * 2);

	fract1 = (double)(pi)/(double)(DIALER_SAMPLERATE/freqa);
	fract2 = (double)(pi)/(double)(DIALER_SAMPLERATE/freqb);

	for (i = 0; i < samples; i++) {
		point1 = fract1 * i;
		result = 16383 + (sin (point1) * 16383);
		point2 = fract2 * i;
		result += 16383 + (sin (point2) * 16383);
		result /= 2.0;
		buf[i] = (uint16_t)round (result);
		buf[i] = ~(buf[i]) + 1;
	}

	chThdSetPriority (HIGHPRIO - 5);

	i = 0;
	while (1) {
		i2sSamplesPlay (buf, samples);
		if (w == NULL) {
			i += samples;
			if (i > (duration * (DIALER_SAMPLERATE / 1000)))
				break;
		} else {
			if ((w->g.flags & GBUTTON_FLG_PRESSED) == 0)
				break;
		}
		i2sSamplesWait ();
	}

	i2sSamplesWait ();
	i2sSamplesStop ();

	chThdSetPriority (ORCHARD_APP_PRIO);

	free (buf);

	return;
}

static uint32_t
dialer_init(OrchardAppContext *context)
{
	(void)context;

	/*
	 * We don't want any extra stack space allocated for us.
	 * We'll use the heap.
	 */

	return (0);
}
static void
draw_keypad(OrchardAppContext *context)
{
	DHandles * p;
	GWidgetInit wi;
	const DIALER_BUTTON * b;
	int i;
	int off = 0;

	p = context->priv;

	gwinWidgetClearInit (&wi);

	wi.g.show = TRUE;
	wi.g.width = 60;
	wi.g.height = 60;
	wi.customDraw = gwinButtonDraw_Normal;

	/* Create button widgets */

#ifdef DIALER_BLUEBOX
	if (p->mode) {
		wi.customStyle = &DarkPurpleFilledStyle;
		off = DIALER_MAXBUTTONS; } else
#endif
	wi.customStyle = &RedButtonStyle;

	for (i = 0; i < DIALER_MAXBUTTONS; i++) {
		if (i > 15) {
			wi.g.width = 110;
			wi.g.height = 40;
		}
		b = &buttons[i + off];
		wi.g.x = b->button_x;
		wi.g.y = b->button_y;
		wi.text = b->button_text;
		p->ghButtons[i] = gwinButtonCreate (0, &wi);
	}

	return;
}

static void destroy_keypad(OrchardAppContext *context)
{
	DHandles *p;
	int i;

	p = context->priv;

	for (i = 0; i < DIALER_MAXBUTTONS; i++)
		gwinDestroy (p->ghButtons[i]);

	return;
}

static void dialer_start(OrchardAppContext *context)
{
	DHandles * p;

	p = malloc (sizeof(DHandles));
	memset (p, 0, sizeof(DHandles));
	context->priv = p;
	p->font = gdispOpenFont (FONT_KEYBOARD);

	/*
	 * Clear the screen, and rotate to portrait mode.
	 */

	gdispClear (GFX_BLACK);
	p->o = gdispGetOrientation ();
	gdispSetOrientation (gOrientation90);

	gwinSetDefaultFont (p->font);

	draw_keypad (context);
  
	geventListenerInit(&p->glDListener);
	gwinAttachListener(&p->glDListener);

	geventRegisterCallback (&p->glDListener,
	    orchardAppUgfxNonBlockCallback, &p->glDListener);

	dialer_i2s_init ();

	return;
}

static void
dialer_event(OrchardAppContext *context, const OrchardAppEvent *event)
{
	GEvent * pe;
	DHandles *p;
	GWidgetObject * w;
	int i;
	int b;

	p = context->priv;

#ifdef notdef
	/* Handle joypad events  */

	if (event->type == keyEvent) {
		if (event->key.flags == keyPress) {
			/* any key to exit */
      			orchardAppExit();
    		}
	}
#endif

	/* Handle uGFX events  */

	if (event->type == ugfxEvent) {
    
		pe = event->ugfx.pEvent;

		if (pe->type == GEVENT_GWIN_BUTTON) {
			for (i = 0; i < DIALER_MAXBUTTONS; i++) {
				if (((GEventGWinButton*)pe)->gwin ==
				    p->ghButtons[i])
					break;
			}
			if (i == 18) {
				orchardAppExit();
			} else if (i == 17) {
#ifdef DIALER_BLUEBOX
				p->mode = !p->mode;
				destroy_keypad (context);
				draw_keypad (context);
#else
				return;
#endif
			} else {
				b = i;
				w = &((GButtonObject *)(p->ghButtons[i]))->w;
#ifdef DIALER_BLUEBOX
				if (p->mode)
					b += DIALER_MAXBUTTONS;
#endif
				tonePlay (w, b, 0);
			}
		}
	}

	return;
}

static void
dialer_exit(OrchardAppContext *context)
{
	DHandles *p;
	p = context->priv;

	destroy_keypad (context);

	gdispSetOrientation (p->o);
	gdispCloseFont (p->font);

	geventDetachSource (&p->glDListener, NULL);
	geventRegisterCallback (&p->glDListener, NULL, NULL);

	free (p);
	context->priv = NULL;

	dialer_i2s_restore ();

	return;
}

orchard_app("DTMF Dialer", "icons/bell.rgb", 0, dialer_init,
             dialer_start, dialer_event, dialer_exit, 9999);
