module gpu_l2_tag_array #(
    parameter ADDR_W = 40,
    parameter SETS = 64,
    parameter WAYS = 16,
    parameter OFFSET_BITS = 7
) (
    input                         clk,
    input                         rst_n,

    input      [5:0]              lookup_set,
    input      [ADDR_W-OFFSET_BITS-6-1:0] lookup_tag,
    output reg                    hit,
    output reg [3:0]              hit_way,
    output reg [3:0]              victim_way,
    output reg                    victim_found,

    input      [5:0]              query_set,
    input      [3:0]              query_way,
    output reg                    query_valid,
    output reg                    query_dirty,
    output reg                    query_lock,
    output reg [ADDR_W-OFFSET_BITS-6-1:0] query_tag,

    input                         access_en,
    input      [5:0]              access_set,
    input      [3:0]              access_way,

    input                         fill_en,
    input      [5:0]              fill_set,
    input      [3:0]              fill_way,
    input      [ADDR_W-OFFSET_BITS-6-1:0] fill_tag,
    input                         fill_valid,
    input                         fill_dirty,
    input                         fill_lock,

    input                         set_dirty_en,
    input      [5:0]              set_dirty_set,
    input      [3:0]              set_dirty_way,
    input                         set_dirty_value,

    input                         set_lock_en,
    input      [5:0]              set_lock_set,
    input      [3:0]              set_lock_way,
    input                         set_lock_value,

    input                         inv_en,
    input      [5:0]              inv_set,
    input      [3:0]              inv_way
);

localparam TAG_W = ADDR_W - OFFSET_BITS - 6;

reg [TAG_W-1:0] tag_array [0:WAYS-1][0:SETS-1];
reg             valid_array [0:WAYS-1][0:SETS-1];
reg             dirty_array [0:WAYS-1][0:SETS-1];
reg             lock_array [0:WAYS-1][0:SETS-1];
reg [3:0]       lru_age [0:WAYS-1][0:SETS-1];

integer i, j;
reg [3:0] max_age;
reg found_invalid;

always @(*) begin
    hit = 1'b0;
    hit_way = 4'd0;
    victim_way = 4'd0;
    victim_found = 1'b0;
    max_age = 4'd0;
    found_invalid = 1'b0;

    for (i=0; i<WAYS; i=i+1) begin
        if (valid_array[i][lookup_set] && (tag_array[i][lookup_set] == lookup_tag)) begin
            hit = 1'b1;
            hit_way = i[3:0];
        end
    end

    // 优先选未锁且无效路
    for (i=0; i<WAYS; i=i+1) begin
        if (!lock_array[i][lookup_set] && !valid_array[i][lookup_set]) begin
            victim_way = i[3:0];
            found_invalid = 1'b1;
            victim_found = 1'b1;
        end
    end

    // 若无无效路，选未锁且LRU年龄最大
    if (!found_invalid) begin
        max_age = 0;
        for (i=0; i<WAYS; i=i+1) begin
            if (!lock_array[i][lookup_set]) begin
                if (!victim_found || (lru_age[i][lookup_set] >= max_age)) begin
                    max_age = lru_age[i][lookup_set];
                    victim_way = i[3:0];
                    victim_found = 1'b1;
                end
            end
        end
    end

    query_valid = valid_array[query_way][query_set];
    query_dirty = dirty_array[query_way][query_set];
    query_lock  = lock_array[query_way][query_set];
    query_tag   = tag_array[query_way][query_set];
end

always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
        for (j=0; j<SETS; j=j+1) begin
            for (i=0; i<WAYS; i=i+1) begin
                tag_array[i][j] <= 0;
                valid_array[i][j] <= 1'b0;
                dirty_array[i][j] <= 1'b0;
                lock_array[i][j] <= 1'b0;
                lru_age[i][j] <= i[3:0];
            end
        end
    end else begin
        if (fill_en) begin
            tag_array[fill_way][fill_set] <= fill_tag;
            valid_array[fill_way][fill_set] <= fill_valid;
            dirty_array[fill_way][fill_set] <= fill_dirty;
            lock_array[fill_way][fill_set] <= fill_lock;
        end

        if (set_dirty_en) begin
            dirty_array[set_dirty_way][set_dirty_set] <= set_dirty_value;
        end

        if (set_lock_en) begin
            lock_array[set_lock_way][set_lock_set] <= set_lock_value;
        end

        if (inv_en) begin
            valid_array[inv_way][inv_set] <= 1'b0;
            dirty_array[inv_way][inv_set] <= 1'b0;
            lock_array[inv_way][inv_set] <= 1'b0;
        end

        if (access_en) begin
            for (i=0; i<WAYS; i=i+1) begin
                if (i == access_way)
                    lru_age[i][access_set] <= 4'd0;
                else if (lru_age[i][access_set] != 4'hF)
                    lru_age[i][access_set] <= lru_age[i][access_set] + 1'b1;
            end
        end
    end
end

endmodule
