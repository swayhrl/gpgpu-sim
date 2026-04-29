module gpu_l2_tag_dir #(
    parameter ADDR_W = 40,
    parameter SETS = 64,
    parameter WAYS = 16,
    parameter OFFSET_BITS = 7,
    parameter DATA_SLOTS = 1024,
    parameter EPOCH_W = 3,
    parameter RD_SYNC = 0
) (
    input                               clk,
    input                               rst_n,

    input      [$clog2(SETS)-1:0]       lookup_set,
    input      [ADDR_W-OFFSET_BITS-$clog2(SETS)-1:0] lookup_tag,
    output reg                          hit,
    output reg [$clog2(WAYS)-1:0]       hit_way,
    output reg                          victim_found,
    output reg [$clog2(WAYS)-1:0]       victim_way,

    input      [$clog2(SETS)-1:0]       rd_set,
    input      [$clog2(WAYS)-1:0]       rd_way,
    output [ADDR_W-OFFSET_BITS-$clog2(SETS)+$clog2(DATA_SLOTS)+EPOCH_W+9-1:0] rd_entry,

    input                               wr_en,
    input      [$clog2(SETS)-1:0]       wr_set,
    input      [$clog2(WAYS)-1:0]       wr_way,
    input      [ADDR_W-OFFSET_BITS-$clog2(SETS)+$clog2(DATA_SLOTS)+EPOCH_W+9-1:0] wr_entry,

    input                               access_en,
    input      [$clog2(SETS)-1:0]       access_set,
    input      [$clog2(WAYS)-1:0]       access_way,
    input                               clear_all
);

localparam SET_W = $clog2(SETS);
localparam WAY_W = $clog2(WAYS);
localparam TAG_W = ADDR_W - OFFSET_BITS - SET_W;
localparam PTR_W = $clog2(DATA_SLOTS);
localparam DIR_W = TAG_W + PTR_W + EPOCH_W + 9;

// bit layout: {valid,dirty,lock,state[1:0],epoch[EPOCH_W-1:0],tag[TAG_W-1:0],ptr[PTR_W-1:0],lru[3:0]}
localparam LRU_LSB   = 0;
localparam PTR_LSB   = LRU_LSB + 4;
localparam TAG_LSB   = PTR_LSB + PTR_W;
localparam EPOCH_LSB = TAG_LSB + TAG_W;
localparam STATE_LSB = EPOCH_LSB + EPOCH_W;
localparam LOCK_BIT  = STATE_LSB + 2;
localparam DIRTY_BIT = LOCK_BIT + 1;
localparam VALID_BIT = DIRTY_BIT + 1;

reg                    valid_mem [0:SETS-1][0:WAYS-1];
reg                    dirty_mem [0:SETS-1][0:WAYS-1];
reg                    lock_mem  [0:SETS-1][0:WAYS-1];
reg [1:0]              state_mem [0:SETS-1][0:WAYS-1];
reg [EPOCH_W-1:0]      epoch_mem [0:SETS-1][0:WAYS-1];
reg [TAG_W-1:0]        tag_mem   [0:SETS-1][0:WAYS-1];
reg [PTR_W-1:0]        ptr_mem   [0:SETS-1][0:WAYS-1];
reg [3:0]              lru_mem   [0:SETS-1][0:WAYS-1];

integer i,j;
reg found_invalid;
reg [3:0] max_lru;
reg [DIR_W-1:0] rd_entry_c;
reg [DIR_W-1:0] rd_entry_r;

always @(*) begin
    hit = 1'b0;
    hit_way = {WAY_W{1'b0}};

    victim_found = 1'b0;
    victim_way = {WAY_W{1'b0}};
    found_invalid = 1'b0;
    max_lru = 4'd0;

    for (i=0; i<WAYS; i=i+1) begin
        if (valid_mem[lookup_set][i] && (tag_mem[lookup_set][i] == lookup_tag)) begin
            hit = 1'b1;
            hit_way = i[WAY_W-1:0];
        end
    end

    for (i=0; i<WAYS; i=i+1) begin
        if (!lock_mem[lookup_set][i] && !valid_mem[lookup_set][i]) begin
            victim_found = 1'b1;
            victim_way = i[WAY_W-1:0];
            found_invalid = 1'b1;
        end
    end

    if (!found_invalid) begin
        for (i=0; i<WAYS; i=i+1) begin
            if (!lock_mem[lookup_set][i]) begin
                if (!victim_found || (lru_mem[lookup_set][i] >= max_lru)) begin
                    victim_found = 1'b1;
                    victim_way = i[WAY_W-1:0];
                    max_lru = lru_mem[lookup_set][i];
                end
            end
        end
    end

    rd_entry_c = {valid_mem[rd_set][rd_way], dirty_mem[rd_set][rd_way], lock_mem[rd_set][rd_way],
                  state_mem[rd_set][rd_way], epoch_mem[rd_set][rd_way], tag_mem[rd_set][rd_way],
                  ptr_mem[rd_set][rd_way], lru_mem[rd_set][rd_way]};
end

always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
        rd_entry_r <= {DIR_W{1'b0}};
        for (j=0; j<SETS; j=j+1) begin
            for (i=0; i<WAYS; i=i+1) begin
                valid_mem[j][i] <= 1'b0;
                dirty_mem[j][i] <= 1'b0;
                lock_mem[j][i]  <= 1'b0;
                state_mem[j][i] <= 2'b00;
                epoch_mem[j][i] <= {EPOCH_W{1'b0}};
                tag_mem[j][i]   <= {TAG_W{1'b0}};
                ptr_mem[j][i]   <= {PTR_W{1'b0}};
                lru_mem[j][i]   <= i[3:0];
            end
        end
    end else if (clear_all) begin
        rd_entry_r <= {DIR_W{1'b0}};
        for (j=0; j<SETS; j=j+1) begin
            for (i=0; i<WAYS; i=i+1) begin
                valid_mem[j][i] <= 1'b0;
                dirty_mem[j][i] <= 1'b0;
                lock_mem[j][i]  <= 1'b0;
                state_mem[j][i] <= 2'b00;
                epoch_mem[j][i] <= {EPOCH_W{1'b0}};
                tag_mem[j][i]   <= {TAG_W{1'b0}};
                ptr_mem[j][i]   <= {PTR_W{1'b0}};
                lru_mem[j][i]   <= i[3:0];
            end
        end
    end else begin
        if (RD_SYNC != 0)
            rd_entry_r <= rd_entry_c;
        if (wr_en) begin
            valid_mem[wr_set][wr_way] <= wr_entry[VALID_BIT];
            dirty_mem[wr_set][wr_way] <= wr_entry[DIRTY_BIT];
            lock_mem[wr_set][wr_way]  <= wr_entry[LOCK_BIT];
            state_mem[wr_set][wr_way] <= wr_entry[STATE_LSB +: 2];
            epoch_mem[wr_set][wr_way] <= wr_entry[EPOCH_LSB +: EPOCH_W];
            tag_mem[wr_set][wr_way]   <= wr_entry[TAG_LSB +: TAG_W];
            ptr_mem[wr_set][wr_way]   <= wr_entry[PTR_LSB +: PTR_W];
            lru_mem[wr_set][wr_way]   <= wr_entry[LRU_LSB +: 4];
        end

        if (access_en) begin
            for (i=0; i<WAYS; i=i+1) begin
                if (i == access_way)
                    lru_mem[access_set][i] <= 4'd0;
                else if (lru_mem[access_set][i] != 4'hF)
                    lru_mem[access_set][i] <= lru_mem[access_set][i] + 1'b1;
            end
        end
    end
end

assign rd_entry = (RD_SYNC != 0) ? rd_entry_r : rd_entry_c;

endmodule
