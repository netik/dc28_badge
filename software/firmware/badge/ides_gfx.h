#ifndef __IDES_GFX_H__
#define __IDES_GFX_H__

/* ides_gfx.h
 *
 * Shared Graphics Routines
 * J. Adams <1/2017>
 */

extern const GWidgetStyle RedButtonStyle;
extern const GWidgetStyle GreenButtonStyle;
extern const GWidgetStyle DarkGreyStyle;
extern const GWidgetStyle DarkPurpleStyle;
extern const GWidgetStyle DarkPurpleFilledStyle;
extern const GWidgetStyle IvoryStyle;

#pragma pack(1)
typedef struct _rgbpixel {
        uint8_t p[3];
} rgbpixel;
#pragma pack()

typedef struct gdisp_image {
	uint8_t			gdi_id1;
	uint8_t			gdi_id2;
	uint8_t			gdi_width_hi;
	uint8_t			gdi_width_lo;
	uint8_t			gdi_height_hi;
	uint8_t			gdi_height_lo;
	uint8_t			gdi_fmt_hi;
	uint8_t			gdi_fmt_lo;
} GDISP_IMAGE;

/* Graphics */
extern int putImageFile (char *name, int16_t x, int16_t y);
extern void drawProgressBar(gCoord x, gCoord y, gCoord width, gCoord height, int32_t maxval, int32_t currentval, uint8_t use_leds, uint8_t reverse);
extern int putImageFile(char *name, int16_t x, int16_t y);
extern void blinkText (gCoord x, gCoord y,gCoord cx, gCoord cy, char *text, gFont font, gColor color, gJustify justify, uint8_t times, int16_t delay);
extern char *getAvatarImage(int shipclass, bool is_player, char frame, bool is_right);
extern void screen_alert_draw(uint8_t clear, char *msg);
extern void getPixelBlock (gCoord x, gCoord y,gCoord cx,
    gCoord cy, gPixel * buf);
extern void putPixelBlock (gCoord x, gCoord y,gCoord cx,
    gCoord cy, gPixel * buf);
extern void drawBufferedStringBox(
      gPixel **fb,
      gCoord x,
      gCoord y,
      gCoord cx,
      gCoord cy,
      const char* str,
      gFont font,
      gColor color,
      gJustify justify);
extern  gColor levelcolor(uint16_t player_level, uint16_t enemy_level);

#endif /* __IDES_GFX_H__ */

/* used by screen_alert_draw */
#define ALERT_DELAY_SHORT 1500
#define ALERT_DELAY  3000
