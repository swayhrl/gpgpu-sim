module gpu_l2_hazard_table #(
    parameter ADDR_W = 40,
    parameter OFFSET_BITS = 7,
    parameter MSHR_NUM = 64,
    parameter HAZ_ENTRIES = 64,
    // Stage-D / PR-D3:
    // Keep per-bucket lookup bounded to a tiny constant for timing-friendly synthesis.
    parameter HAZ_WAYS = 2
) (
    input                               clk,
    input                               rst_n,

    input                               push_req,
    input      [ADDR_W-OFFSET_BITS-1:0] push_line_addr,
    input      [$clog2(MSHR_NUM)-1:0]   push_tok,
    output reg                          push_accept,
    output reg                          push_blocked,

    input                               retire_req,
    input      [$clog2(MSHR_NUM)-1:0]   retire_tok,

    output reg                          wake_valid,
    output reg [$clog2(MSHR_NUM)-1:0]   wake_tok
);

localparam TOK_W = $clog2(MSHR_NUM);
localparam LINE_ADDR_W = ADDR_W - OFFSET_BITS;
localparam SLOT_W = $clog2(HAZ_ENTRIES);
localparam HAZ_BUCKETS = (HAZ_ENTRIES / HAZ_WAYS);
localparam BUCKET_W = $clog2(HAZ_BUCKETS);

reg                     haz_valid   [0:HAZ_ENTRIES-1];
reg [LINE_ADDR_W-1:0]   haz_addr    [0:HAZ_ENTRIES-1];
reg [5:0]               haz_cnt     [0:HAZ_ENTRIES-1];
reg [TOK_W-1:0]         haz_head    [0:HAZ_ENTRIES-1];
reg [TOK_W-1:0]         haz_tail    [0:HAZ_ENTRIES-1];

reg                     tok_valid   [0:MSHR_NUM-1];
reg [SLOT_W-1:0]        tok_hslot   [0:MSHR_NUM-1];
reg                     tok_next_v  [0:MSHR_NUM-1];
reg [TOK_W-1:0]         tok_next    [0:MSHR_NUM-1];
reg                     tok_prev_v  [0:MSHR_NUM-1];
reg [TOK_W-1:0]         tok_prev    [0:MSHR_NUM-1];

integer i;
integer w;
integer slot_base;
integer slot_idx;
integer hit_slot;
integer free_slot;
integer alloc_slot;
integer rhslot;
reg [TOK_W-1:0] nxt;
reg [TOK_W-1:0] prv;
reg [BUCKET_W-1:0] push_bucket;

function [BUCKET_W-1:0] hash_line;
    input [LINE_ADDR_W-1:0] line_addr;
    begin
        hash_line = line_addr % HAZ_BUCKETS;
    end
endfunction

`ifndef SYNTHESIS
initial begin
    if (HAZ_WAYS <= 0) begin
        $fatal(1, "gpu_l2_hazard_table: HAZ_WAYS must be > 0");
    end
    if (HAZ_ENTRIES < HAZ_WAYS) begin
        $fatal(1, "gpu_l2_hazard_table: HAZ_ENTRIES must be >= HAZ_WAYS");
    end
    if ((HAZ_ENTRIES % HAZ_WAYS) != 0) begin
        $fatal(1, "gpu_l2_hazard_table: HAZ_ENTRIES must be divisible by HAZ_WAYS");
    end
end
`endif

always @(*) begin
    push_bucket = hash_line(push_line_addr);
    slot_base = push_bucket * HAZ_WAYS;
    hit_slot = -1;
    free_slot = -1;

    for (w=0; w<HAZ_WAYS; w=w+1) begin
        slot_idx = slot_base + w;
        if (haz_valid[slot_idx] && (haz_addr[slot_idx] == push_line_addr)) hit_slot = slot_idx;
        if (!haz_valid[slot_idx] && (free_slot < 0)) free_slot = slot_idx;
    end

    push_accept = push_req;
    push_blocked = 1'b0;
    if (push_req) begin
        if (hit_slot >= 0) begin
            push_blocked = 1'b1;
        end else if (free_slot >= 0) begin
            push_blocked = 1'b0;
        end else begin
            // Bucket is saturated: conservatively serialize by aliasing into way0.
            // This is a constant-time approximation (possible false dependencies).
            push_blocked = 1'b1;
        end
    end
end

always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
        for (i=0; i<HAZ_ENTRIES; i=i+1) begin
            haz_valid[i] <= 1'b0;
            haz_addr[i] <= {LINE_ADDR_W{1'b0}};
            haz_cnt[i] <= 6'd0;
            haz_head[i] <= {TOK_W{1'b0}};
            haz_tail[i] <= {TOK_W{1'b0}};
        end
        for (i=0; i<MSHR_NUM; i=i+1) begin
            tok_valid[i] <= 1'b0;
            tok_hslot[i] <= {SLOT_W{1'b0}};
            tok_next_v[i] <= 1'b0;
            tok_next[i] <= {TOK_W{1'b0}};
            tok_prev_v[i] <= 1'b0;
            tok_prev[i] <= {TOK_W{1'b0}};
        end
        wake_valid <= 1'b0;
        wake_tok <= {TOK_W{1'b0}};
    end else begin
        wake_valid <= 1'b0;

        if (push_req && push_accept) begin
            alloc_slot = -1;
            if (hit_slot >= 0) begin
                alloc_slot = hit_slot;
            end else if (free_slot >= 0) begin
                alloc_slot = free_slot;
            end else begin
                alloc_slot = slot_base;
            end

            tok_valid[push_tok] <= 1'b1;
            tok_hslot[push_tok] <= alloc_slot[SLOT_W-1:0];
            tok_next_v[push_tok] <= 1'b0;
            tok_next[push_tok] <= {TOK_W{1'b0}};
            tok_prev_v[push_tok] <= 1'b0;
            tok_prev[push_tok] <= {TOK_W{1'b0}};

            if (haz_valid[alloc_slot]) begin
                tok_next_v[haz_tail[alloc_slot]] <= 1'b1;
                tok_next[haz_tail[alloc_slot]] <= push_tok;
                tok_prev_v[push_tok] <= 1'b1;
                tok_prev[push_tok] <= haz_tail[alloc_slot];
                haz_tail[alloc_slot] <= push_tok;
                haz_cnt[alloc_slot] <= haz_cnt[alloc_slot] + 1'b1;
            end else begin
                haz_valid[alloc_slot] <= 1'b1;
                haz_addr[alloc_slot] <= push_line_addr;
                haz_cnt[alloc_slot] <= 6'd1;
                haz_head[alloc_slot] <= push_tok;
                haz_tail[alloc_slot] <= push_tok;
            end
        end

        if (retire_req && tok_valid[retire_tok]) begin
            rhslot = tok_hslot[retire_tok];
            if (haz_valid[rhslot]) begin
                if (haz_cnt[rhslot] == 6'd1) begin
                    haz_valid[rhslot] <= 1'b0;
                    haz_cnt[rhslot] <= 6'd0;
                    haz_head[rhslot] <= {TOK_W{1'b0}};
                    haz_tail[rhslot] <= {TOK_W{1'b0}};
                end else begin
                    nxt = tok_next[retire_tok];
                    prv = tok_prev[retire_tok];

                    if (haz_head[rhslot] == retire_tok) begin
                        haz_head[rhslot] <= nxt;
                        tok_prev_v[nxt] <= 1'b0;
                        tok_prev[nxt] <= {TOK_W{1'b0}};
                        wake_valid <= 1'b1;
                        wake_tok <= nxt;
                    end else if (haz_tail[rhslot] == retire_tok) begin
                        haz_tail[rhslot] <= prv;
                        tok_next_v[prv] <= 1'b0;
                    end else begin
                        tok_next[prv] <= nxt;
                        tok_next_v[prv] <= 1'b1;
                        tok_prev[nxt] <= prv;
                        tok_prev_v[nxt] <= 1'b1;
                    end

                    haz_cnt[rhslot] <= haz_cnt[rhslot] - 1'b1;
                end
            end

            tok_valid[retire_tok] <= 1'b0;
            tok_next_v[retire_tok] <= 1'b0;
            tok_next[retire_tok] <= {TOK_W{1'b0}};
            tok_prev_v[retire_tok] <= 1'b0;
            tok_prev[retire_tok] <= {TOK_W{1'b0}};
        end
    end
end

endmodule
