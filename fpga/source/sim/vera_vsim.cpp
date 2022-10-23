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

#define LOGDIR "logs/"

#define MAX_TRACE_FRAMES 3        // video frames to dump to VCD file (and then screen-shot and exit)
#define MAX_UPLOADS      8         // maximum number of "payload" uploads

// Current simulation time (64-bit unsigned)
vluint64_t main_time         = 0;
vluint64_t first_frame_start = 0;
vluint64_t frame_start_time  = 0;

volatile bool done;
bool          sim_render = SDL_RENDER;
bool          wait_close = false;

bool vsync_detect = false;
bool vtop_detect  = false;
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

#define TOP_clk   top->gpio_20
#define TOP_cs_n  top->led_red
#define TOP_rnwr  top->led_green
#define TOP_hsync top->gpio_12
#define TOP_vsync top->gpio_21
#define TOP_red   ((top->gpio_13 << 3) | (top->gpio_11 << 2) | (top->gpio_44 << 1) | (top->gpio_48 << 0))
#define TOP_green ((top->gpio_19 << 3) | (top->gpio_9 << 2) | (top->gpio_4 << 1) | (top->gpio_45 << 0))
#define TOP_blue  ((top->gpio_18 << 3) | (top->gpio_6 << 2) | (top->gpio_3 << 1) | (top->gpio_47 << 0))


//     input  wire       gpio_20,  //  clk25,

//     // External bus interface
//     input  wire       led_red,  //extbus_cs_n,   /* Chip select */
// //    input  wire       extbus_rd_n,   /* Read strobe */
//     input  wire       led_green,    //extbus_wr_n,   /* Write strobe */
// //    input  wire [4:0] extbus_a,      /* Address */
//     input  wire       gpio_27, gpio_26, gpio_25, gpio_23, led_blue,     /* Address */
// //    inout  wire [7:0] extbus_d,      /* Data (bi-directional) */
//     inout wire        gpio_28, gpio_38, gpio_42, gpio_36, gpio_43, gpio_34, gpio_37, gpio_31,
//     output wire       gpio_10,      //extbus_irq_n,  /* IRQ */

//     // VGA interface
//     output wire   gpio_12,        // video vga_hsync
//     output wire   gpio_21,        // video vga_vsync
//     output reg    gpio_13,        // video vga_r[3]
//     output reg    gpio_19,        // video vga_g[3]
//     output reg    gpio_18,        // video vga_b[3]
//     output reg    gpio_11,        // video vga_r[2]
//     output reg    gpio_9,         // video vga_g[2]
//     output reg    gpio_6,         // video vga_b[2]
//     output reg    gpio_44,        // video vga_r[1]
//     output reg    gpio_4,         // video vga_g[1]
//     output reg    gpio_3,         // video vga_b[1]
//     output reg    gpio_48,        // video vga_r[0]
//     output reg    gpio_45,        // video vga_g[0]
//     output reg    gpio_47         // video vga_b[0]


int main(int argc, char ** argv)
{
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = ctrl_c;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);

    if ((logfile = fopen(LOGDIR "vera_vsim.log", "w")) == NULL)
    {
        printf("can't create " LOGDIR "vera_vsim.log\n");
        exit(EXIT_FAILURE);
    }

    double Hz = 1000000.0 / ((TOTAL_WIDTH * TOTAL_HEIGHT) * (1.0 / PIXEL_CLOCK_MHZ));
    log_printf(
        "\nVERA simulation. Video %d x %d with %f MHz clock, for % 0.03 Hz FPS\n", VISIBLE_WIDTH, VISIBLE_HEIGHT, Hz);

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

    while (!done && !Verilated::gotFinish())
    {

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

        bool hsync = H_SYNC_POLARITY ? TOP_hsync : !TOP_hsync;
        bool vsync = V_SYNC_POLARITY ? TOP_vsync : !TOP_vsync;

#if SDL_RENDER
        if (sim_render)
        {
            // sim_render current VGA output pixel (4 bits per gun)
            SDL_SetRenderDrawColor(
                renderer, (TOP_red << 4) | TOP_red, (TOP_green << 4) | TOP_green, (TOP_blue << 4) | TOP_blue, 255);

            if (frame_num > 0)
            {
                SDL_RenderDrawPoint(renderer, current_x, current_y);
            }
        }
#endif
        current_x++;

        if (hsync)
            hsync_count++;

        hsync_detect = false;

        // end of hsync
        if (!hsync && vga_hsync_previous)
        {
            hsync_detect = true;
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
        vga_hsync_previous = hsync;

        vsync_detect = false;

        if (vsync && !vga_vsync_previous)
        {
            vtop_detect = true;
        }

        if (!vsync && vga_vsync_previous)
        {
            vsync_detect = true;
            if (current_y - 1 > y_max)
                y_max = current_y - 1;

            if (frame_num > 0)
            {
                if (frame_num == 1)
                {
                    first_frame_start = main_time;
                }
                vluint64_t frame_time = (main_time - frame_start_time) / 2;
                logonly_printf(
                    "[@t=%8lu] Frame %3d, %lu pixel-clocks (% 0.03f msec real-time), %dx%d hsync %d, vsync %d\n",
                    main_time,
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
                                   main_time,
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

        vga_vsync_previous = vsync;

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

#if SDL_RENDER
    if (sim_render)
    {
        if (!wait_close)
        {
            SDL_Delay(1000);
        }
        else
        {
            fprintf(stderr, "Press RETURN:\n");
            fgetc(stdin);
        }

        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
    }
#endif

    log_printf("Simulation ended after %d frames, %lu pixel clock ticks (%.04f milliseconds)\n",
               frame_num,
               (main_time / 2),
               ((1.0 / (PIXEL_CLOCK_MHZ * 1000000)) * (main_time / 2)) * 1000.0);

    return EXIT_SUCCESS;
}
