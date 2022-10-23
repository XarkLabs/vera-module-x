#define VERA_BASE 0x9F20

#define VERA_ADDR_L 0x00
#define VERA_ADDR_M 0x01
#define VERA_ADDR_H 0x02
#define VERA_DATA0  0x03
#define VERA_DATA1  0x04
#define VERA_CTRL   0x05

#define VERA_IEN        0x06
#define VERA_ISR        0x07
#define VERA_IRQ_LINE_L 0x08

#define VERA_DC_VIDEO  0x09
#define VERA_DC_HSCALE 0x0A
#define VERA_DC_VSCALE 0x0B
#define VERA_DC_BORDER 0x0C

#define VERA_DC_HSTART 0x09
#define VERA_DC_HSTOP  0x0A
#define VERA_DC_VSTART 0x0B
#define VERA_DC_VSTOP  0x0C

#define VERA_L0_CONFIG    0x0D
#define VERA_L0_MAPBASE   0x0E
#define VERA_L0_TILEBASE  0x0F
#define VERA_L0_HSCROLL_L 0x10
#define VERA_L0_HSCROLL_H 0x11
#define VERA_L0_VSCROLL_L 0x12
#define VERA_L0_VSCROLL_H 0x13

#define VERA_L1_CONFIG    0x14
#define VERA_L1_MAPBASE   0x15
#define VERA_L1_TILEBASE  0x16
#define VERA_L1_HSCROLL_L 0x17
#define VERA_L1_HSCROLL_H 0x18
#define VERA_L1_VSCROLL_L 0x19
#define VERA_L1_VSCROLL_H 0x1A

#define VERA_AUDIO_CTRL 0x1B
#define VERA_AUDIO_RATE 0x1C
#define VERA_AUDIO_DATA 0x1D

#define VERA_SPI_DATA 0x1E
#define VERA_SPI_CTRL 0x1F

#define VERA_PSG_BASE     0x1F9C0
#define VERA_PALETTE_BASE 0x1FA00
#define VERA_SPRITES_BASE 0x1FC00

// VGA mode 640x480 @ 60Hz (pixel clock 25.175Mhz)
const double PIXEL_CLOCK_MHZ  = 25.000;        // pixel clock in MHz
const int    VISIBLE_WIDTH    = 640;           // horizontal active pixels
const int    VISIBLE_HEIGHT   = 480;           // vertical active lines
const int    H_FRONT_PORCH    = 16;            // H pre-sync (front porch) pixels
const int    H_SYNC_PULSE     = 96;            // H sync pulse pixels
const int    H_BACK_PORCH     = 48;            // H post-sync (back porch) pixels
const int    V_FRONT_PORCH    = 10;            // V pre-sync (front porch) lines
const int    V_SYNC_PULSE     = 2;             // V sync pulse lines
const int    V_BACK_PORCH     = 33;            // V post-sync (back porch) lines
const int    H_SYNC_POLARITY  = 0;             // H sync pulse active level
const int    V_SYNC_POLARITY  = 0;             // V sync pulse active level
const int    TOTAL_WIDTH      = H_FRONT_PORCH + H_SYNC_PULSE + H_BACK_PORCH + VISIBLE_WIDTH;
const int    TOTAL_HEIGHT     = V_FRONT_PORCH + V_SYNC_PULSE + V_BACK_PORCH + VISIBLE_HEIGHT;
const int    OFFSCREEN_WIDTH  = TOTAL_WIDTH - VISIBLE_WIDTH;
const int    OFFSCREEN_HEIGHT = TOTAL_HEIGHT - VISIBLE_HEIGHT;
