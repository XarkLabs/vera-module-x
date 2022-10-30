`timescale 1 ns / 1 ps
//`default_nettype none

module tb();

    initial begin
        $dumpfile("tb.fst");
        $dumpvars(0, tb);
    end

    initial begin
        // #3000000 $finish;
//        #50000000 $finish;
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

    top top(
        .clk25(sysclk),

        .extbus_cs_n(extbus_cs_n),
        .extbus_rd_n(extbus_rd_n),
        .extbus_wr_n(extbus_wr_n),
        .extbus_a(extbus_a[4:0]),
        .extbus_d(extbus_d),
        
        .spi_miso(1'b1));


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

            $display("WRITE %04x <= %02x", addr, data);

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

            @(posedge phi2)
            #10;
            $display("READ  %04x => %02x", addr, extbus_d);

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

        // extbus_write(16'h9F25, 8'h01);

        // extbus_write(16'h9F20, 8'h00);
        // extbus_write(16'h9F21, 8'h40);
        // extbus_write(16'h9F22, 8'h10);

        // extbus_write(16'h9F24, 8'hA1);
        // extbus_write(16'h9F24, 8'hA2);
        // extbus_write(16'h9F24, 8'hA3);
        // extbus_write(16'h9F24, 8'hA4);

        // extbus_write(16'h9F20, 8'h00);
        // extbus_write(16'h9F21, 8'h40);
        // extbus_write(16'h9F22, 8'h10);

        // extbus_read(16'h9F24);
        // extbus_read(16'h9F24);
        // extbus_read(16'h9F24);
        // extbus_read(16'h9F24);


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


        // N.A. audio triangle

// 001188720, 0x02, 0x01
// 001188727, 0x00, 0xc0
// 001188734, 0x01, 0xf9
// 001188738, 0x03, 0x33
// # WRITE video_space[$1F9C0] = $33
// 001195618, 0x02, 0x01
// 001195625, 0x00, 0xc1
// 001195632, 0x01, 0xf9
// 001195636, 0x03, 0x02
// # WRITE video_space[$1F9C1] = $02
// 001202309, 0x07, 0x01
// 001206840, 0x02, 0x01
// 001206847, 0x00, 0xc3
// 001206854, 0x01, 0xf9
// 001206858, 0x03, 0x80
// # WRITE video_space[$1F9C3] = $80
// 001221426, 0x02, 0x01
// 001221433, 0x00, 0xc2
// 001221440, 0x01, 0xf9
// 001221444, 0x03, 0xff
// # WRITE video_space[$1F9C2] = $FF
// 001336710, 0x07, 0x01
// 001471107, 0x07, 0x01
// 001539728, 0x02, 0x01
// 001539735, 0x00, 0xc2
// 001539742, 0x01, 0xf9
// 001539746, 0x03, 0xfe

extbus_write(16'h9F3B, 8'h0F);  // PSG vol

extbus_write(16'h9F25, 8'h00);  // zero ADDRSEL DCSEL

extbus_write(16'h9F20, 8'hc0);  // 1f9c0 inc 1
extbus_write(16'h9F21, 8'hf9);
extbus_write(16'h9F22, 8'h11);

extbus_write(16'h9F23, 8'h33);  // 1f9c0 = 33
extbus_write(16'h9F23, 8'h02);  // 1f9c1 = 02 
extbus_write(16'h9F23, 8'h00);  // 1f9c2 = 00
extbus_write(16'h9F23, 8'h80);  // 1f9c3 = 80

extbus_write(16'h9F20, 8'hc2);  // 1f9c2 inc 0
extbus_write(16'h9F21, 8'hf9);
extbus_write(16'h9F22, 8'h01);



    for (integer i = 255; i >= 192; i = i - 1) begin
        extbus_write(16'h9F23, i[7:0]);
        #15000000;
    end
    $finish;
end

endmodule
