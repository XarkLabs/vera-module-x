`default_nettype none               // mandatory for Verilog sanity

module reset_sync(
    input  wire async_rst_in,
    input  wire clk,
    output wire reset_out);

    reg dff_r, dff_rr;

`ifdef XARK_OSS
    // fix simulation issue where reset undefined so no posedge at start
    initial begin
        dff_r = 1'b0;
        dff_rr = 1'b0;
    end
`endif

    always @(posedge clk or posedge async_rst_in) begin
        if (async_rst_in) begin
            dff_r <= 1'b1;
            dff_rr <= 1'b1;

        end else begin
            dff_r <= 1'b0;
            dff_rr <= dff_r;
        end
    end

    assign reset_out = dff_rr;

endmodule
`default_nettype wire               // restore default
