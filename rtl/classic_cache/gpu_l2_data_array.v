module gpu_l2_data_array #(
    parameter LINE_W = 1024,
    parameter SETS = 64,
    parameter WAYS = 16
) (
    input                     clk,
    input                     rst_n,

    input      [5:0]          rd_set,
    input      [3:0]          rd_way,
    output reg [LINE_W-1:0]   rd_line,

    input                     wr_en,
    input      [5:0]          wr_set,
    input      [3:0]          wr_way,
    input      [LINE_W-1:0]   wr_line,
    input      [LINE_W/8-1:0] wr_strb
);

reg [LINE_W-1:0] data_array [0:WAYS-1][0:SETS-1];
integer i,j,b;
reg [LINE_W-1:0] t;

always @(*) begin
    rd_line = data_array[rd_way][rd_set];
end

always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
        for (j=0; j<SETS; j=j+1)
            for (i=0; i<WAYS; i=i+1)
                data_array[i][j] <= 0;
    end else if (wr_en) begin
        t = data_array[wr_way][wr_set];
        for (b=0; b<LINE_W/8; b=b+1)
            if (wr_strb[b]) t[b*8 +: 8] = wr_line[b*8 +: 8];
        data_array[wr_way][wr_set] <= t;
    end
end

endmodule
