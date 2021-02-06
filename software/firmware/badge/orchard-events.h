#ifndef __ORCHARD_EVENTS__
#define __ORCHARD_EVENTS__

#include "gfx.h"
#ifdef notdef
#include "joypad_lld.h"
#endif

/* Orchard event wrappers.
   These simplify the ChibiOS eventing system.  To use, initialize the event
   table, hook an event, and then dispatch events.

  static void shell_termination_handler(eventid_t id) {
    chprintf(stream, "Shell exited.  Received id %d\r\n", id);
  }

  void main(int argc, char **argv) {
    struct evt_table events;

    // Support up to 8 events
    evtTableInit(events, 8);

    // Call shell_termination_handler() when 'shell_terminated' is emitted
    evtTableHook(events, shell_terminated, shell_termination_handler);

    // Dispatch all events
    while (TRUE)
      chEvtDispatch(evtHandlers(events), chEvtWaitOne(ALL_EVENTS));
   }
 */

struct evt_table {
  int size;
  int next;
  evhandler_t *handlers;
  event_listener_t *listeners;
};

#define evtTableInit(table, capacity)                                       \
  do {                                                                      \
    static evhandler_t handlers[capacity];                                  \
    static event_listener_t listeners[capacity];                            \
    table.size = capacity;                                                  \
    table.next = 0;                                                         \
    table.handlers = handlers;                                              \
    table.listeners = listeners;                                            \
  } while(0)

#define evtTableHook(table, event, callback)                                \
  do {                                                                      \
    if (CH_DBG_ENABLE_ASSERTS != FALSE)                                     \
      if (table.next >= table.size)                                         \
        chSysHalt("event table overflow");                                  \
    chEvtRegister(&event, &table.listeners[table.next], table.next);        \
    table.handlers[table.next] = callback;                                  \
    table.next++;                                                           \
  } while(0)

#define evtTableUnhook(table, event, callback)                              \
  do {                                                                      \
    int i;                                                                  \
    for (i = 0; i < table.next; i++) {                                      \
      if (table.handlers[i] == callback)                                    \
        chEvtUnregister(&event, &table.listeners[i]);                       \
    }                                                                       \
  } while(0)

#define evtHandlers(table)                                                  \
    table.handlers

#define evtListeners(table)                                                 \
    table.listeners


/* Orchard App events */
typedef enum _OrchardAppEventType {
  keyEvent,
  appEvent,
  timerEvent,
  uiEvent,
  radioEvent,
  touchEvent,
  ugfxEvent,
} OrchardAppEventType;

/* ------- */

typedef struct _OrchardUiEvent {
  uint8_t   code;
  uint8_t   flags;
} OrchardUiEvent;

typedef enum _OrchardUiEventCode {
  uiComplete = 0x01,
} OrchardUiEventCode;

typedef enum _OrchardUiEventFlags {
  uiOK = 0x01,
  uiCancel,
  uiError,
} OrchardUiEventFlags;

typedef enum _OrchardAppEventKeyFlag {
  keyPress = 0,
  keyRelease = 1,
} OrchardAppEventKeyFlag;

typedef enum _OrchardAppEventKeyCode {
  keyAUp = 0x80,
  keyADown = 0x81,
  keyALeft = 0x82,
  keyARight = 0x83,
  keyASelect = 0x84,
  keyBUp = 0x85,
  keyBDown = 0x86,
  keyBLeft = 0x87,
  keyBRight = 0x88,
  keyBSelect = 0x89,
} OrchardAppEventKeyCode;

typedef struct _OrchardAppKeyEvent {
  OrchardAppEventKeyCode code;
  OrchardAppEventKeyFlag flags;
} OrchardAppKeyEvent;

typedef struct _OrchardAppUgfxEvent {
  GListener * pListener;
  GEvent * pEvent;
} OrchardAppUgfxEvent;

/*
 * The radio event type was originally created for Bluetooth radio
 * events. We're not using BLE anymore though: now we have sockets
 * layered on top of the radio. Sockets don't really mesh well with
 * our app framework, so socket-based apps will typically spawn their
 * own separate work thread, and the orchard app front-end just serves
 * as a placeholder. (That is, as long as that app is running, it
 * keeps posession of the system and the launcher won't take over until
 * it's finished.)
 *
 * We define some event types to support this, as well as some
 * simple socket-based network events so that in some simple cases we
 * can still use the orchard app front end for GUI support.
 */

typedef enum _OrchardAppRadioEventType {
   customEvent,			/* App custom event */
   connectEvent,		/* Connected to peer */
   disconnectEvent,		/* Disconnected from peer */
   connectTimeoutEvent,		/* Connection timed out */
   rxEvent,			/* Data received */
   txEvent,			/* Data sent */
   chatEvent,			/* Chat request received */
   challengeEvent,		/* Challenge request received */
   fwEvent,			/* Firmware update request received */
   exitEvent			/* App should exit */
} OrchardAppRadioEventType;

typedef struct _OrchardAppRadioEvent {
   OrchardAppRadioEventType type;
   uint32_t customEvent;
   uint16_t pktlen;
   uint8_t * pkt;
} OrchardAppRadioEvent;

/* ------- */

typedef enum _OrchardAppLifeEventFlag {
  appStart,
  appTerminate,
} OrchardAppLifeEventFlag;

typedef struct _OrchardAppLifeEvent {
  uint8_t   event;
} OrchardAppLifeEvent;

/* ------- */

typedef struct _OrchardAppTimerEvent {
  uint32_t  usecs;
} OrchardAppTimerEvent;

/* ------- */

typedef struct _OrchardTouchEvent {
  uint16_t  x;
  uint16_t  y;
  uint16_t  z;
  uint16_t  temp;
  uint16_t  batt;
} OrchardTouchEvent;

typedef struct _OrchardAppEvent {
  OrchardAppEventType     type;
  union {
    OrchardAppKeyEvent    key;
    OrchardAppLifeEvent   app;
    OrchardAppTimerEvent  timer;
    OrchardUiEvent        ui;
    OrchardTouchEvent     touch;
    OrchardAppUgfxEvent   ugfx;
    OrchardAppRadioEvent  radio;
  } ;
} OrchardAppEvent;

#endif /* __ORCHARD_EVENTS__ */
