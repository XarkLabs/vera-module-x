// C++ "driver" for VERA Verilator simulation
//
// vim: set et ts=4 sw=4
//

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "vera_defs.h"

#include "verilated.h"

#include "Vtop.h"

#include "verilated_fst_c.h"        // for VM_TRACE
#include <SDL.h>                    // for SDL_RENDER
#include <SDL_image.h>

#include "iso_charset.h"

#if !defined(NUM_ELEMENTS)
#define NUM_ELEMENTS(a) (sizeof(a) / sizeof(a[0]))
#endif

#if !defined(SDL_RENDER)
#define SDL_RENDER 0
#endif

#define LOGDIR "../logs/"

#define MAX_TRACE_FRAMES 3        // video frames to dump to VCD file (and then screen-shot and exit)

// Current simulation time (64-bit unsigned)
vluint64_t main_time         = 0;
vluint64_t first_frame_start = 0;
vluint64_t frame_start_time  = 0;

volatile bool done;
bool          sim_render = SDL_RENDER;
bool          wait_close = false;

bool vsync_detect = false;
bool hsync_detect = false;

uint16_t last_read_byte;

static FILE * logfile;
static char   log_buff[16384];

static void log_printf(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(log_buff, sizeof(log_buff), fmt, args);
    fputs(log_buff, stdout);
    fputs(log_buff, logfile);
    va_end(args);
}

static void logonly_printf(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(log_buff, sizeof(log_buff), fmt, args);
    fputs(log_buff, logfile);
    va_end(args);
}

void ctrl_c(int s)
{
    (void)s;
    done = true;
}

// Called by $time in Verilog
double sc_time_stamp()
{
    return main_time;
}

#ifdef XARK_UPDUINO

// localparam RnW_WRITE         = 1'b0;
// localparam RnW_READ          = 1'b1;
// localparam CS_ENABLED        = 1'b0;
// localparam CS_DISABLED       = 1'b1;
//     input  wire       gpio_20,       //  clk25,
//     input  wire       led_red,       // extbus_cs_n,
//     input  wire                      // extbus_rd_n (N/A)
//     input  wire       led_green,     // extbus_wr_n
//     input  wire       gpio_27, gpio_26, gpio_25, gpio_23, led_blue,     // [4:0] extbus_a
//     inout  wire       gpio_28, gpio_38, gpio_42, gpio_36, gpio_43, gpio_34, gpio_37, gpio_31; // [7:0] extbus_d
//     output wire       gpio_10,       // extbus_irq_n

#define TOP_clk   top->gpio_20
#define TOP_irq_n top->gpio_10
#define TOP_cs_n  top->led_red
#define TOP_rnwr  top->led_green

#define TOP_hsync top->gpio_12
#define TOP_vsync top->gpio_21
#define TOP_red   ((top->gpio_13 << 3) | (top->gpio_11 << 2) | (top->gpio_44 << 1) | (top->gpio_48 << 0))
#define TOP_green ((top->gpio_19 << 3) | (top->gpio_9 << 2) | (top->gpio_4 << 1) | (top->gpio_45 << 0))
#define TOP_blue  ((top->gpio_18 << 3) | (top->gpio_6 << 2) | (top->gpio_3 << 1) | (top->gpio_47 << 0))

void set_bus(Vtop * top, bool cs_n, bool rd_n, bool wr_n, uint8_t addr)
{
    assert((rd_n != wr_n) || (rd_n == 1 && wr_n == 1));
    TOP_cs_n = cs_n;
    (void)rd_n;
    TOP_rnwr      = wr_n;
    top->gpio_27  = addr & 0x10 ? 1 : 0;
    top->gpio_26  = addr & 0x08 ? 1 : 0;
    top->gpio_25  = addr & 0x04 ? 1 : 0;
    top->gpio_23  = addr & 0x02 ? 1 : 0;
    top->led_blue = addr & 0x01 ? 1 : 0;
}

void set_data(Vtop * top, uint8_t v)
{
    top->gpio_28 = (v & 0x80) ? 1 : 0;
    top->gpio_38 = (v & 0x40) ? 1 : 0;
    top->gpio_42 = (v & 0x20) ? 1 : 0;
    top->gpio_36 = (v & 0x10) ? 1 : 0;
    top->gpio_43 = (v & 0x08) ? 1 : 0;
    top->gpio_34 = (v & 0x04) ? 1 : 0;
    top->gpio_37 = (v & 0x02) ? 1 : 0;
    top->gpio_31 = (v & 0x01) ? 1 : 0;
}

uint8_t get_data(Vtop * top)
{
    return (top->gpio_28 << 7) | (top->gpio_38 << 6) | (top->gpio_42 << 5) | (top->gpio_36 << 4) | (top->gpio_43 << 3) |
           (top->gpio_34 << 2) | (top->gpio_37 << 1) | (top->gpio_31 << 0);
}

#else

#define TOP_clk   top->clk25
#define TOP_irq_n top->extbus_irq_n
#define TOP_cs_n  top->extbus_cs_n
#define TOP_rd_n  top->extbus_rd_n
#define TOP_wr_n  top->extbus_wr_n

#define TOP_hsync top->vga_hsync
#define TOP_vsync top->vga_vsync
#define TOP_red   top->vga_r
#define TOP_green top->vga_g
#define TOP_blue  top->vga_b

void set_bus(Vtop * top, bool cs_n, bool rd_n, bool wr_n, uint8_t addr)
{
    TOP_cs_n      = cs_n;
    TOP_rd_n      = rd_n;
    TOP_wr_n      = wr_n;
    top->extbus_a = addr & 0x1f;
}

void set_data(Vtop * top, uint8_t v)
{
    top->extbus_d_i = v;
}

uint8_t get_data(Vtop * top)
{
    return top->extbus_d_o;
}

#endif

enum struct Bus
{
    IDLE,
    SELECT,
    DESELECT
};

enum struct Cmd
{
    DONE,
    DELAY,
    REG_WR,
    REG_WR_VALUE,
    REG_WR_ARRAY,
    REG_WR_FILE,
    REG_RD,
    REG_RD_FILE
};

struct BusCommand
{

    Cmd     cmd;
    uint8_t regnum;
    uint8_t data;
    char *  dataptr;
    int     count;
};

#define DELAY(delay)                                                                                                   \
    {                                                                                                                  \
        Cmd::DELAY, 0, 0, nullptr, (delay)                                                                             \
    }

#define REG_WR(reg, value)                                                                                             \
    {                                                                                                                  \
        Cmd::REG_WR, (reg), (value), nullptr, 0                                                                        \
    }

#define REG_RD(reg)                                                                                                    \
    {                                                                                                                  \
        Cmd::REG_RD, (reg), 0, nullptr, 0                                                                              \
    }

#define REG_WR_VALUE(reg, value, count)                                                                                \
    {                                                                                                                  \
        Cmd::REG_WR_VALUE, (reg), (value), 0, (count)                                                                  \
    }

#define REG_WR_ARRAY(reg, array, count)                                                                                \
    {                                                                                                                  \
        Cmd::REG_WR_ARRAY, (reg), 0, (char *)(array), (count)                                                          \
    }

#define REG_WR_FILE(reg, file, count)                                                                                  \
    {                                                                                                                  \
        Cmd::REG_WR_FILE, (reg), 0, (file), (count)                                                                    \
    }

#define REG_RD_FILE(reg, file, count)                                                                                  \
    {                                                                                                                  \
        Cmd::REG_RD_FILE, (reg), 0, (file), (count)                                                                    \
    }

#define DONE()                                                                                                         \
    {                                                                                                                  \
        Cmd::DONE, 0, 0, nullptr, 0                                                                                    \
    }

uint8_t hello_msg[] =
    "H\x61"
    "e\x61"
    "l\x61"
    "l\x61"
    "o\x61";

uint8_t inc_word[0x4000];

BusCommand TestCommands[] = {
    // clear VRAM

    DELAY(500),        // delay cycles

    REG_WR(VERA_ADDR_L, 0x00),                      // addr $hmm00
    REG_WR(VERA_ADDR_M, 0x00),                      // addr $h00ll
    REG_WR(VERA_ADDR_H, 0x10),                      // addr $0mmll incr +1
    REG_WR_VALUE(VERA_DATA0, 0x00, 0x1F000),        // clear VRAM
    DELAY(500),                                     // delay cycles

    // screen_init:
    REG_WR(VERA_CTRL, 0x00),        // 	stz VERA_CTRL   ;set ADDR1 active
                                    // 	lda #2
                                    // 	jsr screen_set_charset

    //  ldx #<charset_addr
    //  stx VERA_ADDR_L
    //  ldx #>charset_addr
    //  stx VERA_ADDR_M
    //  ldx #$10 | ^charset_addr
    //  stx VERA_ADDR_H

    REG_WR(VERA_ADDR_L, VERA_CHARSET_BASE & 0xFF),                         // addr $hmm00
    REG_WR(VERA_ADDR_M, (VERA_CHARSET_BASE >> 8) & 0xFF),                  // addr $h00ll
    REG_WR(VERA_ADDR_H, 0x10 | ((VERA_CHARSET_BASE >> 16) & 0x01)),        // addr $0mmll incr +1
    REG_WR_ARRAY(VERA_DATA0, iso_charset, sizeof(iso_charset)),

    DELAY(500),        // delay cycles
    // 	; Layer 1 configuration
    // 	lda #((1<<6)|(2<<4)|(0<<0))
    // 	sta VERA_L1_CONFIG
    REG_WR(VERA_L1_CONFIG, ((1 << 6) | (2 << 4) | (0 << 0))),
    // 	lda #(screen_addr>>9)
    // 	sta VERA_L1_MAPBASE
    REG_WR(VERA_L1_MAPBASE, (VERA_CHARMAP_BASE >> 9)),
    // 	lda #((charset_addr>>11)<<2)
    // 	sta VERA_L1_TILEBASE
    REG_WR(VERA_L1_TILEBASE, ((VERA_CHARSET_BASE >> 11) << 2)),
    // 	stz VERA_L1_HSCROLL_L
    REG_WR(VERA_L1_HSCROLL_L, 0x00),
    // 	stz VERA_L1_HSCROLL_H
    REG_WR(VERA_L1_HSCROLL_H, 0x00),
    // 	stz VERA_L1_VSCROLL_L
    REG_WR(VERA_L1_VSCROLL_L, 0x00),
    // 	stz VERA_L1_VSCROLL_H
    REG_WR(VERA_L1_VSCROLL_H, 0x00),

    REG_WR(VERA_L0_CONFIG, 0x04),
    REG_WR(VERA_L0_MAPBASE, (0x0000 >> 9)),
    REG_WR(VERA_L0_TILEBASE, ((0x0000 >> 11) << 2) | 0x01),
    REG_WR(VERA_L0_HSCROLL_L, 0x00),
    REG_WR(VERA_L0_HSCROLL_H, 0x00),
    REG_WR(VERA_L0_VSCROLL_L, 0x00),
    REG_WR(VERA_L0_VSCROLL_H, 0x00),

    // 	; Display composer configuration
    // 	lda #2
    // 	sta VERA_CTRL
    REG_WR(VERA_CTRL, 0x02),
    // 	stz VERA_DC_HSTART
    REG_WR(VERA_DC_HSTART, 0x00),
    // 	lda #(640>>2)
    // 	sta VERA_DC_HSTOP
    REG_WR(VERA_DC_HSTOP, (640 >> 2)),
    // 	stz VERA_DC_VSTART
    REG_WR(VERA_DC_VSTART, 0),
    // 	lda #(480>>2)
    // 	sta VERA_DC_VSTOP
    REG_WR(VERA_DC_VSTOP, (480 >> 1)),

    // 	stz VERA_CTRL
    REG_WR(VERA_CTRL, 0x00),
    // 	lda #$21
    // 	sta VERA_DC_VIDEO
    REG_WR(VERA_DC_VIDEO, 0x11),
    // 	lda #128
    // 	sta VERA_DC_HSCALE
    REG_WR(VERA_DC_HSCALE, 128),
    // 	sta VERA_DC_VSCALE
    REG_WR(VERA_DC_VSCALE, 128),
    // 	stz VERA_DC_BORDER
    REG_WR(VERA_DC_BORDER, 0x00),

    // 	; Clear sprite attributes ($1FC00-$1FFFF)
    // 	stz VERA_ADDR_L
    // 	lda #$FC
    // 	sta VERA_ADDR_M
    // 	lda #$11
    // 	sta VERA_ADDR_H
    REG_WR(VERA_ADDR_L, VERA_SPRITES_BASE & 0xFF),                         // addr $hmm00
    REG_WR(VERA_ADDR_M, (VERA_SPRITES_BASE >> 8) & 0xFF),                  // addr $h00ll
    REG_WR(VERA_ADDR_H, 0x10 | ((VERA_SPRITES_BASE >> 16) & 0x01)),        // addr $0mmll incr +1

    // 	ldx #4
    // 	ldy #0
    // :	stz VERA_DATA0     ;clear 128*8 bytes
    // 	iny
    // 	bne :-
    // 	dex
    // 	bne :-
    REG_WR_VALUE(VERA_DATA0, 0x00, 128 * 8),

    // 	lda #$ff
    // 	sta cscrmd      ; force setting color on first mode change
    // 	rts

    REG_WR(VERA_ADDR_L, (0 * 80 + 0) & 0xFF),                         // addr $hmm00
    REG_WR(VERA_ADDR_M, ((0 * 80 + 0) >> 8) & 0xFF),                  // addr $h00ll
    REG_WR(VERA_ADDR_H, 0x10 | (((0 * 80 + 0) >> 16) & 0x01)),        // addr $0mmll incr +1
    REG_WR_VALUE(VERA_DATA0, 0xFF, 80),
    REG_WR(VERA_ADDR_L, (1 * 80 + 0) & 0xFF),                         // addr $hmm00
    REG_WR(VERA_ADDR_M, ((1 * 80 + 0) >> 8) & 0xFF),                  // addr $h00ll
    REG_WR(VERA_ADDR_H, 0xc0 | (((1 * 80 + 0) >> 16) & 0x01)),        // addr $0mmll incr +80
    REG_WR_VALUE(VERA_DATA0, 0x80, 478),
    REG_WR(VERA_ADDR_L, (1 * 80 + 79) & 0xFF),                         // addr $hmm00
    REG_WR(VERA_ADDR_M, ((1 * 80 + 79) >> 8) & 0xFF),                  // addr $h00ll
    REG_WR(VERA_ADDR_H, 0xc0 | (((1 * 80 + 79) >> 16) & 0x01)),        // addr $0mmll incr +80
    REG_WR_VALUE(VERA_DATA0, 0x01, 478),
    REG_WR(VERA_ADDR_L, (479 * 80 + 0) & 0xFF),                         // addr $hmm00
    REG_WR(VERA_ADDR_M, ((479 * 80 + 0) >> 8) & 0xFF),                  // addr $h00ll
    REG_WR(VERA_ADDR_H, 0x10 | (((479 * 80 + 0) >> 16) & 0x01)),        // addr $0mmll incr +1
    REG_WR_VALUE(VERA_DATA0, 0xFF, 80),

    REG_WR(VERA_ADDR_L, (10 * 80 + 10) & 0xFF),                         // addr $hmm00
    REG_WR(VERA_ADDR_M, ((10 * 80 + 10) >> 8) & 0xFF),                  // addr $h00ll
    REG_WR(VERA_ADDR_H, 0xc0 | (((10 * 80 + 10) >> 16) & 0x01)),        // addr $0mmll incr +80
    
    REG_WR(VERA_DATA0, 0b00000000),
    REG_WR(VERA_DATA0, 0b01100110),
    REG_WR(VERA_DATA0, 0b01100110),
    REG_WR(VERA_DATA0, 0b01111110),
    REG_WR(VERA_DATA0, 0b01100110),
    REG_WR(VERA_DATA0, 0b01100110),
    REG_WR(VERA_DATA0, 0b01100110),
    REG_WR(VERA_DATA0, 0b00000000),

    REG_WR(VERA_ADDR_L, (10 * 80 + 11) & 0xFF),                         // addr $hmm00
    REG_WR(VERA_ADDR_M, ((10 * 80 + 11) >> 8) & 0xFF),                  // addr $h00ll
    REG_WR(VERA_ADDR_H, 0xc0 | (((10 * 80 + 11) >> 16) & 0x01)),        // addr $0mmll incr +80
    
    REG_WR(VERA_DATA0, 0b00000000),
    REG_WR(VERA_DATA0, 0b00111100),
    REG_WR(VERA_DATA0, 0b00011000),
    REG_WR(VERA_DATA0, 0b00011000),
    REG_WR(VERA_DATA0, 0b00011000),
    REG_WR(VERA_DATA0, 0b00011000),
    REG_WR(VERA_DATA0, 0b00111100),
    REG_WR(VERA_DATA0, 0b00000000),

    REG_WR(VERA_ADDR_L, (10 * 80 + 12) & 0xFF),                         // addr $hmm00
    REG_WR(VERA_ADDR_M, ((10 * 80 + 12) >> 8) & 0xFF),                  // addr $h00ll
    REG_WR(VERA_ADDR_H, 0xc0 | (((10 * 80 + 12) >> 16) & 0x01)),        // addr $0mmll incr +80
    
    REG_WR(VERA_DATA0, 0b00000000),
    REG_WR(VERA_DATA0, 0b00011000),
    REG_WR(VERA_DATA0, 0b00011000),
    REG_WR(VERA_DATA0, 0b00011000),
    REG_WR(VERA_DATA0, 0b00011000),
    REG_WR(VERA_DATA0, 0b00000000),
    REG_WR(VERA_DATA0, 0b00011000),
    REG_WR(VERA_DATA0, 0b00000000),




    DELAY(50000),        // delay cycles

    DONE()        // ending command
};

bool bus_spam     = false;
int  cmd_rep_spam = 4;

const char * vera_reg_name(int r)
{
    switch (r)
    {
        case VERA_ADDR_L:
            return "ADDR_L";
        case VERA_ADDR_M:
            return "ADDR_M";
        case VERA_ADDR_H:
            return "ADDR_H";
        case VERA_DATA0:
            return "DATA0";
        case VERA_DATA1:
            return "DATA1";
        case VERA_CTRL:
            return "CTRL";
        case VERA_IEN:
            return "IEN";
        case VERA_ISR:
            return "ISR";
        case VERA_IRQ_LINE_L:
            return "IRQ_LINE_L";
        case VERA_DC_VIDEO:
            return "DC_VIDEO/HSTART";
        case VERA_DC_HSCALE:
            return "DC_HSCALE/HSTOP";
        case VERA_DC_VSCALE:
            return "DC_VSCALE/VSTART";
        case VERA_DC_BORDER:
            return "DC_BORDER/VSTOP";
        case VERA_L0_CONFIG:
            return "L0_CONFIG";
        case VERA_L0_MAPBASE:
            return "L0_MAPBASE";
        case VERA_L0_TILEBASE:
            return "L0_TILEBASE";
        case VERA_L0_HSCROLL_L:
            return "L0_HSCROLL_L";
        case VERA_L0_HSCROLL_H:
            return "L0_HSCROLL_H";
        case VERA_L0_VSCROLL_L:
            return "L0_VSCROLL_L";
        case VERA_L0_VSCROLL_H:
            return "L0_VSCROLL_H";
        case VERA_L1_CONFIG:
            return "L1_CONFIG";
        case VERA_L1_MAPBASE:
            return "L1_MAPBASE";
        case VERA_L1_TILEBASE:
            return "L1_TILEBASE";
        case VERA_L1_HSCROLL_L:
            return "L1_HSCROLL_L";
        case VERA_L1_HSCROLL_H:
            return "L1_HSCROLL_H";
        case VERA_L1_VSCROLL_L:
            return "L1_VSCROLL_L";
        case VERA_L1_VSCROLL_H:
            return "L1_VSCROLL_H";
        case VERA_AUDIO_CTRL:
            return "AUDIO_CTRL";
        case VERA_AUDIO_RATE:
            return "AUDIO_RATE";
        case VERA_AUDIO_DATA:
            return "AUDIO_DATA";
        case VERA_SPI_DATA:
            return "SPI_DATA";
        case VERA_SPI_CTRL:
            return "SPI_CTRL";
    }

    return "?";
}

const float  BusSpeed       = 8.0 / PIXEL_CLOCK_MHZ;
float        BusFraction    = 0.0;
uint64_t     BusCycle       = 0;
Bus          BusState       = Bus::IDLE;
BusCommand * BusCmdPtr      = TestCommands;
uint32_t     BusNumCmds     = NUM_ELEMENTS(TestCommands);
uint32_t     BusCmdIndex    = 0;
uint32_t     BusRepeatCount = 0;
BusCommand   BusCurCmd;

void process_bus(Vtop * top)
{
    BusFraction += BusSpeed;

    if (BusFraction >= 1.0)
    {
        BusFraction -= 1.0;

        bool rd_n = (BusCurCmd.cmd == Cmd::REG_RD || BusCurCmd.cmd == Cmd::REG_RD_FILE) ? 0 : 1;
        bool wr_n =
            (BusCurCmd.cmd == Cmd::REG_WR || BusCurCmd.cmd == Cmd::REG_WR_VALUE || BusCurCmd.cmd == Cmd::REG_WR_FILE)
                ? 0
                : 1;

        if (BusState == Bus::SELECT)
        {
            if (BusCurCmd.cmd == Cmd::REG_WR_ARRAY)
            {
                BusCurCmd.data = (uint8_t)(*BusCurCmd.dataptr);
            }
        }

        switch (BusState)
        {
            case Bus::SELECT: {
                if (bus_spam)
                {
                    logonly_printf("[%8lu/%8lu] BUS: CS_n=%d, RD_n=%d, WR_n=%d REG_ADR=0x%02x <= 0x%02x%s\n",
                                   main_time / 2,
                                   BusCycle,
                                   0,
                                   rd_n,
                                   wr_n,
                                   BusCurCmd.regnum,
                                   wr_n ? 0xff : BusCurCmd.data,
                                   wr_n ? "" : " WRITE");
                }
                set_bus(top, 0, rd_n, wr_n, BusCurCmd.regnum);
                set_data(top, BusCurCmd.data);
                BusState = Bus::DESELECT;

                break;
            }

            case Bus::DESELECT:
                if (bus_spam)
                {
                    logonly_printf("[%8lu/%8lu] BUS: CS_n=%d, RD_n=%d, WR_n=%d REG_ADR=0x%02x => 0x%02x%s\n",
                                   main_time / 2,
                                   BusCycle,
                                   1,
                                   1,
                                   1,
                                   BusCurCmd.regnum,
                                   get_data(top),
                                   rd_n ? "" : " READ");
                }
                set_bus(top, 1, 1, 1, BusCurCmd.regnum);
                BusState = Bus::IDLE;

                break;

            case Bus::IDLE:
                set_bus(top, 1, 1, 1, BusCurCmd.regnum);

                if (BusRepeatCount >= BusCurCmd.count)
                {
                    if (BusCmdPtr != nullptr && BusCmdIndex < BusNumCmds)
                    {
                        BusCurCmd      = BusCmdPtr[BusCmdIndex++];
                        BusRepeatCount = 0;
                    }
                    else
                    {
                        BusCurCmd.cmd   = Cmd::DONE;
                        BusCurCmd.count = ~0;
                    }
                }

                switch (BusCurCmd.cmd)
                {
                    case Cmd::DELAY:
                        if (!BusRepeatCount)
                        {
                            logonly_printf("[%8lu/%8lu] DELAY(%d)\n", main_time / 2, BusCycle, BusCurCmd.count);
                        }
                        else if (BusRepeatCount == BusCurCmd.count - 1)
                        {
                            logonly_printf("[%8lu/%8lu]  - DELAY elapsed\n", main_time / 2, BusCycle);
                        }
                        break;
                    case Cmd::REG_WR:
                        BusState = Bus::SELECT;
                        logonly_printf("[%8lu/%8lu] REG_WR(0x%02x %s, 0x%02x)\n",
                                       main_time / 2,
                                       BusCycle,
                                       BusCurCmd.regnum,
                                       vera_reg_name(BusCurCmd.regnum),
                                       BusCurCmd.data);
                        break;
                    case Cmd::REG_RD:
                        BusState = Bus::SELECT;
                        logonly_printf("[%8lu/%8lu] REG_RD(0x%02x %s)\n",
                                       main_time / 2,
                                       BusCycle,
                                       BusCurCmd.regnum,
                                       vera_reg_name(BusCurCmd.regnum));
                        break;
                    case Cmd::REG_WR_VALUE:
                        BusState = Bus::SELECT;
                        if (BusRepeatCount < cmd_rep_spam || BusRepeatCount >= BusCurCmd.count - cmd_rep_spam)
                        {
                            logonly_printf("[%8lu/%8lu] REG_WR_VALUE(0x%02x %s, 0x%02x, %d/%d)%s\n",
                                           main_time / 2,
                                           BusCycle,
                                           BusCurCmd.regnum,
                                           vera_reg_name(BusCurCmd.regnum),
                                           BusCurCmd.data,
                                           BusRepeatCount,
                                           BusCurCmd.count,
                                           BusRepeatCount >= cmd_rep_spam - 1 &&
                                                   BusRepeatCount < BusCurCmd.count - cmd_rep_spam
                                               ? " ..."
                                               : "");
                        }
                        break;
                    case Cmd::REG_WR_ARRAY:
                        BusState       = Bus::SELECT;
                        BusCurCmd.data = *BusCurCmd.dataptr;
                        if (BusRepeatCount < cmd_rep_spam || BusRepeatCount >= BusCurCmd.count - cmd_rep_spam)
                        {
                            logonly_printf("[%8lu/%8lu] REG_WR_ARRAY(0x%02x %s, [%p]=0x%02x, %d/%d)%s\n",
                                           main_time / 2,
                                           BusCycle,
                                           BusCurCmd.regnum,
                                           vera_reg_name(BusCurCmd.regnum),
                                           BusCurCmd.dataptr,
                                           BusCurCmd.data,
                                           BusRepeatCount,
                                           BusCurCmd.count,
                                           BusRepeatCount >= cmd_rep_spam - 1 &&
                                                   BusRepeatCount < BusCurCmd.count - cmd_rep_spam
                                               ? " ..."
                                               : "");
                        }
                        break;
                    case Cmd::REG_WR_FILE:
                        BusState = Bus::SELECT;
                        if (BusRepeatCount < cmd_rep_spam || BusRepeatCount >= BusCurCmd.count - cmd_rep_spam)
                        {
                            logonly_printf("[%8lu/%8lu] REG_WR_FILE(0x%02x %s, \"%s\", %d/%d)\n",
                                           main_time / 2,
                                           BusCycle,
                                           BusCurCmd.regnum,
                                           vera_reg_name(BusCurCmd.regnum),
                                           BusCurCmd.dataptr,
                                           BusRepeatCount,
                                           BusCurCmd.count,
                                           BusRepeatCount >= cmd_rep_spam - 1 &&
                                                   BusRepeatCount < BusCurCmd.count - cmd_rep_spam
                                               ? " ..."
                                               : "");
                        }
                        break;
                    case Cmd::REG_RD_FILE:
                        BusState = Bus::SELECT;
                        logonly_printf("[%8lu/%8lu] REG_RD_FILE(0x%02x, \"%s\", %d/%d)\n",
                                       main_time / 2,
                                       BusCycle,
                                       BusCurCmd.regnum,
                                       BusCurCmd.dataptr,
                                       BusRepeatCount,
                                       BusCurCmd.count);
                        break;
                    case Cmd::DONE:
                        BusState = Bus::IDLE;
                        if (BusCmdPtr)
                        {
                            logonly_printf("[%8lu/%8lu] DONE\n", main_time / 2, BusCycle);
                        }
                        BusCmdPtr   = nullptr;
                        BusCmdIndex = 0;
                        BusNumCmds  = 0;

                        break;
                    default:
                        log_printf("Bad command %u\n", BusCurCmd.cmd);
                        assert(false);
                        break;
                }

                if (BusRepeatCount < BusCurCmd.count)
                {
                    BusRepeatCount += 1;
                    if (BusCurCmd.cmd == Cmd::REG_WR_ARRAY)
                    {
                        BusCurCmd.dataptr++;
                    }
                }
                break;
        }

        BusCycle++;
    }
}

#ifdef XARK_UPDUINO
const char design_name[] = "VERA-UPduino";
#else
const char design_name[] = "VERA-X16";
#endif

int main(int argc, char ** argv)
{
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = ctrl_c;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);

    for (int i = 0; i < sizeof(inc_word) / 2; i++)
    {
        inc_word[i] = i;
    }

    if ((logfile = fopen(LOGDIR "vera_vsim.log", "w")) == NULL)
    {
        printf("can't create " LOGDIR "vera_vsim.log\n");
        exit(EXIT_FAILURE);
    }

    double Hz = 1000000.0 / ((TOTAL_WIDTH * TOTAL_HEIGHT) * (1.0 / PIXEL_CLOCK_MHZ));
    log_printf("\n%s simulation. Video VGA %dx%d@%0.03f Hz (pixel clock %0.03f MHz)\n",
               design_name,
               VISIBLE_WIDTH,
               VISIBLE_HEIGHT,
               Hz,
               PIXEL_CLOCK_MHZ);

    int nextarg = 1;

    while (nextarg < argc && (argv[nextarg][0] == '-' || argv[nextarg][0] == '/'))
    {
        if (strcmp(argv[nextarg] + 1, "n") == 0)
        {
            sim_render = false;
        }
        // else if (strcmp(argv[nextarg] + 1, "b") == 0)
        // {
        //     sim_bus = true;
        // }
        else if (strcmp(argv[nextarg] + 1, "w") == 0)
        {
            wait_close = true;
        }
        // if (strcmp(argv[nextarg] + 1, "u") == 0)
        // {
        //     nextarg += 1;
        //     if (nextarg >= argc)
        //     {
        //         printf("-u needs filename\n");
        //         exit(EXIT_FAILURE);
        //     }
        //     // upload_data = true;
        //     upload_name[num_uploads] = argv[nextarg];
        //     num_uploads++;
        // }
        nextarg += 1;
    }

    Verilated::commandArgs(argc, argv);

#if VM_TRACE
    Verilated::traceEverOn(true);
#endif

    Vtop * top = new Vtop;

#if SDL_RENDER
    SDL_Renderer * renderer = nullptr;
    SDL_Window *   window   = nullptr;
    if (sim_render)
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
        {
            fprintf(stderr, "SDL_Init() failed: %s\n", SDL_GetError());
            return EXIT_FAILURE;
        }
        if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0)
        {
            fprintf(stderr, "IMG_Init() failed: %s\n", SDL_GetError());
            return EXIT_FAILURE;
        }

        window = SDL_CreateWindow(
            "vera-sim", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, TOTAL_WIDTH, TOTAL_HEIGHT, SDL_WINDOW_SHOWN);

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        SDL_RenderSetScale(renderer, 1, 1);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
    }

    bool shot_all  = true;        // screenshot all frames
    bool take_shot = false;

#endif        // SDL_RENDER

    int  current_x          = 0;
    int  current_y          = 0;
    bool vga_hsync_previous = false;
    bool vga_vsync_previous = false;
    int  frame_num          = -1;
    int  x_max              = 0;
    int  y_max              = 0;
    int  hsync_count = 0, hsync_min = 0, hsync_max = 0;
    int  vsync_count  = 0;
    bool image_loaded = false;

#if VM_TRACE
    const auto trace_path = LOGDIR "vera_vsim.fst";
    logonly_printf("Writing FST waveform file to \"%s\"...\n", trace_path);
    VerilatedFstC * tfp = new VerilatedFstC;

    top->trace(tfp, 99);        // trace to heirarchal depth of 99
    tfp->open(trace_path);
#endif

    bool last_hsync = false;
    bool last_vsync = false;
    bool hsync      = false;
    bool vsync      = false;
    while (!done && !Verilated::gotFinish())
    {
        process_bus(top);

        TOP_clk = 1;        // clock rising
        top->eval();

#if VM_TRACE
        if (frame_num <= MAX_TRACE_FRAMES)
            tfp->dump(main_time);
#endif
        main_time++;

        TOP_clk = 0;        // clock falling
        top->eval();

#if VM_TRACE
        if (frame_num <= MAX_TRACE_FRAMES)
            tfp->dump(main_time);
#endif
        main_time++;

        last_hsync = hsync;
        hsync      = H_SYNC_POLARITY ? TOP_hsync : !TOP_hsync;
        last_vsync = vsync;
        vsync      = V_SYNC_POLARITY ? TOP_vsync : !TOP_vsync;

#if SDL_RENDER
        if (sim_render)
        {
            // sim_render current VGA output pixel (4 bits per gun)
            SDL_SetRenderDrawColor(
                renderer, (TOP_red << 4) | TOP_red, (TOP_green << 4) | TOP_green, (TOP_blue << 4) | TOP_blue, 255);

            if (frame_num >= 0)
            {
                SDL_RenderDrawPoint(renderer, current_x, current_y);
            }
        }
#endif
        current_x++;

        if (hsync)
            hsync_count++;

        hsync_detect = !hsync && last_hsync;

        // end of hsync
        if (hsync_detect)
        {
            if (hsync_count > hsync_max)
                hsync_max = hsync_count;
            if (hsync_count < hsync_min || !hsync_min)
                hsync_min = hsync_count;
            hsync_count = 0;

            if (current_x > x_max)
                x_max = current_x;

            current_x = 0;
            current_y++;

            if (vsync)
                vsync_count++;
        }

        vsync_detect = !vsync && last_vsync;

        if (vsync_detect)
        {
            if (current_y - 1 > y_max)
                y_max = current_y - 1;

            if (frame_num == 0)
            {
                first_frame_start = 0;        // main_time;
            }
            if (frame_num >= 0)
            {
                log_printf("[@t=%8lu] Frame %3d begin...\n", main_time / 2, frame_num);
            }
            if (frame_num > 0)
            {
                vluint64_t frame_time = (main_time - frame_start_time) / 2;
                log_printf("[@t=%8lu] Frame %3d, %lu pixel-clocks (% 0.03f msec real-time), %dx%d hsync %d, vsync %d\n",
                           main_time / 2,
                           frame_num,
                           frame_time,
                           ((1.0 / PIXEL_CLOCK_MHZ) * frame_time) / 1000.0,
                           x_max,
                           y_max + 1,
                           hsync_max,
                           vsync_count);
#if SDL_RENDER

                if (sim_render)
                {
                    if (shot_all || take_shot || frame_num == MAX_TRACE_FRAMES)
                    {
                        int  w = 0, h = 0;
                        char save_name[256] = {0};
                        SDL_GetRendererOutputSize(renderer, &w, &h);
                        SDL_Surface * screen_shot =
                            SDL_CreateRGBSurface(0, w, h, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
                        SDL_RenderReadPixels(
                            renderer, NULL, SDL_PIXELFORMAT_ARGB8888, screen_shot->pixels, screen_shot->pitch);
                        sprintf(
                            save_name, LOGDIR "vera_vsim_%dx%d_f%02d.png", VISIBLE_WIDTH, VISIBLE_HEIGHT, frame_num);
                        IMG_SavePNG(screen_shot, save_name);
                        SDL_FreeSurface(screen_shot);
                        float fnum = ((1.0 / PIXEL_CLOCK_MHZ) * ((main_time - first_frame_start) / 2)) / 1000.0;
                        log_printf("[@t=%8lu] %8.03f ms frame #%3u saved as \"%s\" (%dx%d)\n",
                                   main_time / 2,
                                   fnum,
                                   frame_num,
                                   save_name,
                                   w,
                                   h);
                        take_shot = false;
                    }

                    SDL_RenderPresent(renderer);
                    SDL_SetRenderDrawColor(renderer, 0x20, 0x20, 0x20, 0xff);
                    SDL_RenderClear(renderer);
                }
#endif
            }
            frame_start_time = main_time;
            hsync_min        = 0;
            hsync_max        = 0;
            vsync_count      = 0;
            current_y        = 0;

            if (frame_num == MAX_TRACE_FRAMES)
            {
                break;
            }

            if (TOTAL_HEIGHT == y_max + 1)
            {
                frame_num += 1;
            }
            else if (TOTAL_HEIGHT <= y_max)
            {
                log_printf("line %d >= TOTAL_HEIGHT\n", y_max);
            }
        }

#if SDL_RENDER
        if (sim_render)
        {
            SDL_Event e;
            SDL_PollEvent(&e);

            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
            {
                log_printf("Window closed\n");
                break;
            }
        }
#endif
    }

    top->final();

#if VM_TRACE
    tfp->close();
#endif

    log_printf("Simulation ended after %d frames, %lu pixel clock ticks (%.04f milliseconds)\n",
               frame_num,
               (main_time / 2),
               ((1.0 / (PIXEL_CLOCK_MHZ * 1000000)) * (main_time / 2)) * 1000.0);


#if SDL_RENDER
    if (sim_render)
    {
        if (!wait_close)
        {
            SDL_Delay(1000);
        }
        else
        {
            fprintf(stderr, "\n(Waiting for key)\n");
            while (1)
            {
                SDL_Event e;
                SDL_PollEvent(&e);

                if (e.type == SDL_QUIT || e.type == SDL_KEYDOWN)
                {
                    break;
                }
            }
        }

        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
    }
#endif

    printf("Exit.\n");

    return EXIT_SUCCESS;
}
