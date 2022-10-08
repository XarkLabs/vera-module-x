`timescale 1 ns / 1 ps
//`default_nettype none

module tb();

    initial begin
`ifdef XARK_OSS
        $dumpfile("../logs/tb.fst");
`else        
        $dumpfile("tb.vcd");
`endif
        $dumpvars(0, tb);
    end

    initial begin
         #33ms $finish;
//        #30000 $finish;
    end

    // Generate 8MHz phi2
    reg phi2 = 0;
    always #62.5 phi2 = !phi2;

    // Generate 25MHz sysclk
    reg sysclk = 0;
    always #20 sysclk = !sysclk;

    reg extbus_rw_n = 1;
    reg [15:0] extbus_a = 0;

    wire extbus_cs_n = !((extbus_a & 'hFFF0) == 'h9F20);


    reg [7:0] extbus_d_wr = 0;


    wire [7:0] extbus_d = extbus_rw_n ? 8'hZ : extbus_d_wr;

    wire extbus_wr_n = extbus_rw_n || !phi2;
    wire extbus_rd_n = !extbus_rw_n || !phi2;

`ifndef XARK_OSS
    top top(
        .clk25(sysclk),

        .extbus_cs_n(extbus_cs_n),
        .extbus_rd_n(extbus_rd_n),
        .extbus_wr_n(extbus_wr_n),
        .extbus_a(extbus_a[4:0]),
        .extbus_d(extbus_d),
        
        .spi_miso(1'b1));
`else
// NOTE: Xosera assumes 12MHz OSC jumper is shorted, and R28 RGB LED jumper is cut (using RGB for input)

    top top(
        .gpio_20(sysclk),           //  clk25 (but 12 MHz),
        .led_red(extbus_cs_n),      // extbus_cs_n,   /* Chip select */
                                    // no extbus_rd_n
        .led_green(extbus_wr_n),    // extbus_wr_n,   /* Write strobe */
        .gpio_27(extbus_a[4]),      // extbus_a[4:0],      /* Address */
        .gpio_26(extbus_a[3]),
        .gpio_25(extbus_a[2]),
        .gpio_23(extbus_a[1]),
        .led_blue(extbus_a[0]),
        .gpio_38(extbus_d[7]),
        .gpio_42(extbus_d[6]),
        .gpio_28(extbus_d[5]),
        .gpio_36(extbus_d[4]),
        .gpio_43(extbus_d[3]),
        .gpio_34(extbus_d[2]),
        .gpio_37(extbus_d[1]),
        .gpio_31(extbus_d[0]));

    // // VGA interface
    // output wire   gpio_12,        // video vga_hsync
    // output wire   gpio_21,        // video vga_vsync
    // output reg    gpio_13,        // video vga_r[3]
    // output reg    gpio_19,        // video vga_g[3]
    // output reg    gpio_18,        // video vga_b[3]
    // output reg    gpio_11,        // video vga_r[2]
    // output reg    gpio_9,         // video vga_g[2]
    // output reg    gpio_6,         // video vga_b[2]
    // output reg    gpio_44,        // video vga_r[1]
    // output reg    gpio_4,         // video vga_g[1]
    // output reg    gpio_3,         // video vga_b[1]
    // output reg    gpio_48,        // video vga_r[0]
    // output reg    gpio_45,        // video vga_g[0]
    // output reg    gpio_47         // video vga_b[0]
`endif

    task extbus_write;
        input [15:0] addr;
        input  [7:0] data;

        begin
            @(negedge phi2)
            #10; // tAH = 10ns
            // extbus_rw_n = 1'bX;
            // extbus_a = 16'bX;
            // extbus_d_wr = 8'bX;
            #20;
            extbus_a = addr; // address
            extbus_rw_n = 1'b0; // write

            @(posedge phi2)
            #25;
            extbus_d_wr = data;


            @(negedge phi2)
            #10;
            extbus_a = 16'b0;
            extbus_rw_n = 1'b1;
            // extbus_d_wr = 8'bX;

            // @(negedge phi2);
            // @(negedge phi2);
            // @(negedge phi2);

        end
    endtask

    task extbus_read;
        input [15:0] addr;

        begin
            @(negedge phi2)
            #10; // tAH = 10ns
            // extbus_rw_n = 1'bX;
            // extbus_a = 16'bX;
            // extbus_d_wr = 8'bX;
            #20;
            extbus_a = addr;    // address
            extbus_rw_n = 1'b1; // read

            @(negedge phi2)
            #10;
            extbus_a = 16'b0;
            extbus_rw_n = 1'b1;


            @(negedge phi2);
            @(negedge phi2);
            @(negedge phi2);
        end
    endtask



    initial begin
        #6000

        extbus_write(16'h9F25, 8'h01);

        extbus_write(16'h9F20, 8'h00);
        extbus_write(16'h9F21, 8'h40);
        extbus_write(16'h9F22, 8'h10);

        extbus_write(16'h9F24, 8'hA1);
        extbus_write(16'h9F24, 8'hA2);
        extbus_write(16'h9F24, 8'hA3);
        extbus_write(16'h9F24, 8'hA4);

        extbus_write(16'h9F20, 8'h00);
        extbus_write(16'h9F21, 8'h40);
        extbus_write(16'h9F22, 8'h10);

        extbus_read(16'h9F24);
        extbus_read(16'h9F24);
        extbus_read(16'h9F24);
        extbus_read(16'h9F24);


        // extbus_write(16'h9F2D, 8'h04);
        // extbus_write(16'h9F2F, 8'h01);





        // extbus_write(16'h1000, 8'h00);
        // extbus_write(16'h1001, 8'h40);
        // extbus_write(16'h1002, 8'h10);

        // extbus_write(16'h1003, 8'hA0);
        // extbus_write(16'h1003, 8'hA1);
        // extbus_write(16'h1003, 8'hA2);
        // extbus_write(16'h1003, 8'hA3);

        // extbus_write(16'h1000, 8'h00);
        // extbus_write(16'h1001, 8'h40);

        // extbus_read(16'h1003);
        // extbus_read(16'h1003);
        // extbus_read(16'h1003);
        // extbus_read(16'h1003);

        // extbus_write(16'h1002, 8'h10);
        // extbus_read(16'h1003);

        // extbus_write(16'h1000, 8'h00);
        // extbus_write(16'h1001, 8'h00);
        // extbus_write(16'h1002, 8'hA5);
        // extbus_read(16'h1003);
        // extbus_write(16'h1002, 8'h5A);
        // extbus_read(16'h1003);
        // extbus_write(16'h1002, 8'h42);
        // extbus_read(16'h1003);

        // extbus_write(16'h1003, 8'h01);
        // extbus_write(16'h1003, 8'h02);
        // extbus_write(16'h1003, 8'h03);
        // extbus_write(16'h1003, 8'h04);

        // extbus_write(16'h1000, 8'h10);
        // extbus_write(16'h1001, 8'h00);
        // extbus_write(16'h1002, 8'h00);

        // extbus_read(16'h1003);
        // extbus_read(16'h1003);
        // extbus_read(16'h1003);
        // extbus_read(16'h1003);

        // @(negedge phi2);
        // extbus_write(16'h1003, 8'h02);
        // @(negedge phi2);
        // extbus_write(16'h1003, 8'h03);
        // @(negedge phi2);
        // extbus_write(16'h1003, 8'h04);
        // @(negedge phi2);

        // extbus_write(16'h1000, 8'h10);
        // @(negedge phi2);
        // extbus_write(16'h1001, 8'h00);
        // @(negedge phi2);
        // extbus_write(16'h1002, 8'h00);
        // @(negedge phi2);




        // extbus_write(16'h1003, 8'h13);
        // extbus_write(16'h1003, 8'h42);
        // extbus_write(16'h1003, 8'h02);
        // extbus_write(16'h1003, 8'h03);

        // for (i=0; i<8; i=i+1) begin

        // end
    end



endmodule
