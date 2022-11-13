`ifdef XARK_OSS     // Xark: infer BRAM vs Radiant generator
`default_nettype none               // mandatory for Verilog sanity

module palette_ram(
    input wire          wr_clk_i,
    input wire          rd_clk_i,
    input wire          rst_i,
    input wire          wr_clk_en_i,
    input wire          rd_en_i,
    input wire          rd_clk_en_i,
    input wire          wr_en_i,
    input wire   [1:0]  ben_i,
    input wire  [15:0]  wr_data_i,
    input wire   [7:0]  wr_addr_i,
    input wire   [7:0]  rd_addr_i,
    output logic [15:0] rd_data_o
);

logic unused_signals = &{ 1'b0, rst_i, wr_clk_en_i, rd_en_i, rd_clk_en_i };

// infer 16x256 color BRAM
logic [15:0] bram[0:255];

initial begin
    $readmemh("palette_ram.mem", bram, 0);
end

// infer BRAM block
always_ff @(posedge wr_clk_i) begin
    if (wr_en_i) begin
        if (ben_i[1]) begin
            bram[wr_addr_i][15:8] <= wr_data_i[15:8];
        end
        if (ben_i[0]) begin
            bram[wr_addr_i][7:0] <= wr_data_i[7:0];
        end
    end
end

always_ff @(posedge rd_clk_i) begin
    rd_data_o <= bram[rd_addr_i];
end

endmodule

`default_nettype wire               // restore default
`endif
