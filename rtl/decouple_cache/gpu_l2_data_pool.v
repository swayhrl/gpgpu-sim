module gpu_l2_data_pool #(
    parameter LINE_W = 1024,
    parameter DATA_SLOTS = 1024,
    parameter BANKS = 2
) (
    input                               clk,
    input                               rst_n,

    input                               rd_en,
    input      [$clog2(DATA_SLOTS)-1:0] rd_ptr,
    output reg [LINE_W-1:0]             rd_line,

    input                               wr_en,
    input      [$clog2(DATA_SLOTS)-1:0] wr_ptr,
    input      [LINE_W-1:0]             wr_line,
    input      [LINE_W/8-1:0]           wr_strb
);

localparam PTR_W = $clog2(DATA_SLOTS);
localparam BANK_W = (BANKS <= 1) ? 1 : $clog2(BANKS);
localparam ROWS = DATA_SLOTS / BANKS;
localparam ROW_W = (ROWS <= 1) ? 1 : $clog2(ROWS);

reg [LINE_W-1:0] bank_mem [0:BANKS-1][0:ROWS-1];
integer i,j,b;
reg [LINE_W-1:0] t;
reg [BANK_W-1:0] rd_bank, wr_bank;
reg [ROW_W-1:0] rd_row, wr_row;

`ifndef SYNTHESIS
initial begin
    if (BANKS <= 0) begin
        $fatal(1, "gpu_l2_data_pool: BANKS must be > 0");
    end
    if (DATA_SLOTS < BANKS) begin
        $fatal(1, "gpu_l2_data_pool: DATA_SLOTS must be >= BANKS");
    end
    if ((DATA_SLOTS % BANKS) != 0) begin
        $fatal(1, "gpu_l2_data_pool: DATA_SLOTS must be divisible by BANKS");
    end
end
`endif

always @(*) begin
    rd_bank = rd_ptr[BANK_W-1:0];
    rd_row  = rd_ptr[PTR_W-1:BANK_W];
    rd_line = rd_en ? bank_mem[rd_bank][rd_row] : {LINE_W{1'b0}};
end

always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
        for (i=0; i<BANKS; i=i+1)
            for (j=0; j<ROWS; j=j+1)
                bank_mem[i][j] <= {LINE_W{1'b0}};
    end else if (wr_en) begin
        wr_bank = wr_ptr[BANK_W-1:0];
        wr_row  = wr_ptr[PTR_W-1:BANK_W];
        t = bank_mem[wr_bank][wr_row];
        for (b=0; b<LINE_W/8; b=b+1)
            if (wr_strb[b]) t[b*8 +: 8] = wr_line[b*8 +: 8];
        bank_mem[wr_bank][wr_row] <= t;
    end
end

endmodule
