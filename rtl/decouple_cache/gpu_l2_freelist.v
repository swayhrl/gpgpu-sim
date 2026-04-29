module gpu_l2_freelist #(
    parameter DATA_SLOTS = 1024
) (
    input                           clk,
    input                           rst_n,
    input                           reset_all,

    input                           alloc_req,
    output                          alloc_gnt,
    output [$clog2(DATA_SLOTS)-1:0] alloc_ptr,

    input                           free_req,
    input      [$clog2(DATA_SLOTS)-1:0] free_ptr,

    output reg [$clog2(DATA_SLOTS):0] free_count
);

localparam PTR_W = $clog2(DATA_SLOTS);
reg [PTR_W-1:0] q_mem [0:DATA_SLOTS-1];
reg [PTR_W:0] head, tail;
integer i;

assign alloc_gnt = (free_count != 0);

always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
        head <= 0;
        tail <= DATA_SLOTS;
        free_count <= DATA_SLOTS;
        for (i=0; i<DATA_SLOTS; i=i+1)
            q_mem[i] <= i[PTR_W-1:0];
    end else if (reset_all) begin
        head <= 0;
        tail <= DATA_SLOTS;
        free_count <= DATA_SLOTS;
        for (i=0; i<DATA_SLOTS; i=i+1)
            q_mem[i] <= i[PTR_W-1:0];
    end else begin
        if (alloc_req && alloc_gnt)
            head <= head + 1'b1;

        if (free_req) begin
            q_mem[tail[PTR_W-1:0]] <= free_ptr;
            tail <= tail + 1'b1;
        end

        case ({(alloc_req && alloc_gnt), free_req})
            2'b10: if (free_count != 0) free_count <= free_count - 1'b1;
            2'b01: if (free_count != DATA_SLOTS) free_count <= free_count + 1'b1;
            default: free_count <= free_count;
        endcase
    end
end

assign alloc_ptr = q_mem[head[PTR_W-1:0]];

endmodule
