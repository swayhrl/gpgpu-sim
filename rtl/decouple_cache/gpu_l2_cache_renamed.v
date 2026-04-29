module gpu_l2_cache_renamed #(
    parameter ADDR_W      = 40,
    parameter ID_W        = 8,
    parameter LINE_W      = 1024,
    parameter SETS        = 64,
    parameter WAYS        = 16,
    parameter OFFSET_BITS = 7,
    parameter RESP_W      = 2,
    parameter MSHR_NUM    = 64,
    parameter DATA_SLOTS  = 1024,
    parameter EPOCH_W     = 3,
    parameter HAZ_ENTRIES = 64,
    parameter HAZ_WAYS    = 2,
    parameter DATA_BANKS  = 2,
    parameter RESP_IN_ORDER = 1,
    parameter TAGDIR_RD_SYNC = 0,
    parameter INDEX_HASH_MODE = 0 // 0:none 1:fermi-xor 2:ipoly-like
) (
    input                       clk,
    input                       rst_n,

    input                       xbnd_valid,
    output                      xbnd_ready,
    input      [ID_W-1:0]       xbnd_id,
    input      [ADDR_W-1:0]     xbnd_addr,
    input                       xbnd_bypass,
    input      [4:0]            xbnd_calg,

    input                       xbwd_valid,
    output                      xbwd_ready,
    input      [ID_W-1:0]       xbwd_id,
    input      [ADDR_W-1:0]     xbwd_addr,
    input      [LINE_W-1:0]     xbwd_wdata,
    input      [LINE_W/8-1:0]   xbwd_wstrb,
    input                       xbwd_is_atomic,
    input                       xbwd_bypass,
    input      [4:0]            xbwd_calg,
    input      [1:0]            xbwd_atomic_op,
    input                       xbwd_atomic_size,
    input      [63:0]           xbwd_atomic_arg,
    input      [63:0]           xbwd_atomic_cmp,

    input                       flush_req,
    output reg                  flush_ack,
    input                       cache_inv,

    output reg                  xbwu_valid,
    input                       xbwu_ready,
    output reg [ID_W-1:0]       xbwu_id,
    output reg [LINE_W-1:0]     xbwu_rdata,
    output reg [RESP_W-1:0]     xbwu_resp,

    output reg                  xbnu_valid,
    input                       xbnu_ready,
    output reg [ID_W-1:0]       xbnu_id,
    output reg [RESP_W-1:0]     xbnu_resp,

    output reg                  ar_valid,
    input                       ar_ready,
    output reg [$clog2(MSHR_NUM)-1:0] ar_id,
    output reg [ADDR_W-1:0]     ar_addr,

    output reg                  aw_valid,
    input                       aw_ready,
    output reg [$clog2(MSHR_NUM)-1:0] aw_id,
    output reg [ADDR_W-1:0]     aw_addr,
    output reg [LINE_W-1:0]     aw_wdata,
    output reg [LINE_W/8-1:0]   aw_wstrb,
    output reg                  aw_is_atomic,

    input                       r_valid,
    output                      r_ready,
    input      [$clog2(MSHR_NUM)-1:0] r_id,
    input      [LINE_W-1:0]     r_data,
    input      [RESP_W-1:0]     r_resp,

    input                       b_valid,
    output                      b_ready,
    input      [$clog2(MSHR_NUM)-1:0] b_id,
    input      [RESP_W-1:0]     b_resp
);

localparam SET_W  = $clog2(SETS);
localparam WAY_W  = $clog2(WAYS);
localparam TAG_W  = ADDR_W - OFFSET_BITS - SET_W;
localparam PTR_W  = $clog2(DATA_SLOTS);
localparam TOK_W  = $clog2(MSHR_NUM);
localparam LINE_ADDR_W = ADDR_W - OFFSET_BITS;
localparam CALG_INV      = 0;
localparam CALG_BYPASS   = 1;
localparam CALG_NOINIT   = 2;
localparam CALG_READBACK = 3;
localparam CALG_LOCK     = 4;

reg                  m_valid    [0:MSHR_NUM-1];
reg                  m_is_read  [0:MSHR_NUM-1];
reg [ID_W-1:0]       m_id       [0:MSHR_NUM-1];
reg [ADDR_W-1:0]     m_addr     [0:MSHR_NUM-1];
reg [LINE_W-1:0]     m_wdata    [0:MSHR_NUM-1];
reg [LINE_W/8-1:0]   m_wstrb    [0:MSHR_NUM-1];
reg [LINE_W-1:0]     m_aw_line  [0:MSHR_NUM-1];
reg                  m_is_atomic [0:MSHR_NUM-1];
reg [1:0]            m_atomic_op [0:MSHR_NUM-1];
reg                  m_atomic_size [0:MSHR_NUM-1];
reg [63:0]           m_atomic_arg [0:MSHR_NUM-1];
reg [63:0]           m_atomic_cmp [0:MSHR_NUM-1];
reg                  m_bypass   [0:MSHR_NUM-1];
reg [4:0]            m_calg     [0:MSHR_NUM-1];
reg                  m_no_fill  [0:MSHR_NUM-1];
reg                  m_need_aw  [0:MSHR_NUM-1];
reg                  m_blocked  [0:MSHR_NUM-1];
reg                  m_issued   [0:MSHR_NUM-1];
reg                  m_wait_r   [0:MSHR_NUM-1];
reg                  m_wait_b   [0:MSHR_NUM-1];
reg                  m_done     [0:MSHR_NUM-1];
reg                  m_in_issue_q [0:MSHR_NUM-1];
reg [RESP_W-1:0]     m_resp     [0:MSHR_NUM-1];
reg [LINE_W-1:0]     m_rdata    [0:MSHR_NUM-1];

reg [TOK_W-1:0] head_ptr, tail_ptr;
reg [TOK_W:0]   inflight_cnt;

// separated issue queues
reg [TOK_W-1:0] r_q [0:MSHR_NUM-1];
reg [TOK_W-1:0] w_q [0:MSHR_NUM-1];
reg [TOK_W-1:0] r_q_head, r_q_tail;
reg [TOK_W-1:0] w_q_head, w_q_tail;
reg [TOK_W:0]   r_q_cnt, w_q_cnt;

integer i;
reg req_is_read;
reg req_fire;
reg [ID_W-1:0] req_id;
reg [ADDR_W-1:0] req_addr;
reg [LINE_W-1:0] req_wdata;
reg [LINE_W/8-1:0] req_wstrb;
reg [LINE_ADDR_W-1:0] req_line_addr;

// hazard table signals
reg haz_push_req;
wire haz_push_accept;
wire haz_push_blocked;
reg [LINE_ADDR_W-1:0] haz_push_line_addr;
reg [TOK_W-1:0] haz_push_tok;
reg haz_retire_req;
reg [TOK_W-1:0] haz_retire_tok;
wire haz_wake_valid;
wire [TOK_W-1:0] haz_wake_tok;

// directory/data/freelist signals
wire dir_hit;
wire [WAY_W-1:0] dir_hit_way;
wire dir_victim_found;
wire [WAY_W-1:0] dir_victim_way;
wire [TAG_W+PTR_W+EPOCH_W+9-1:0] dir_rd_entry;
wire [LINE_W-1:0] data_rd_line;
wire [PTR_W-1:0] fl_alloc_ptr;
wire fl_alloc_gnt;
wire [PTR_W:0] fl_free_count;

reg [TOK_W-1:0] retire_tok_sel;
reg retire_found;
wire [SET_W-1:0] lookup_set_w;
wire [TAG_W-1:0] lookup_tag_w;
reg [ADDR_W-1:0] lookup_addr_w;
reg [SET_W-1:0] dir_rd_set;
reg [WAY_W-1:0] dir_rd_way;
reg dir_wr_en;
reg [SET_W-1:0] dir_wr_set;
reg [WAY_W-1:0] dir_wr_way;
reg [TAG_W+PTR_W+EPOCH_W+9-1:0] dir_wr_entry;
reg dir_access_en;
reg [SET_W-1:0] dir_access_set;
reg [WAY_W-1:0] dir_access_way;
reg data_rd_en;
reg [PTR_W-1:0] data_rd_ptr;
reg data_wr_en;
reg [PTR_W-1:0] data_wr_ptr;
reg [LINE_W-1:0] data_wr_line;
reg [LINE_W/8-1:0] data_wr_strb;
reg fl_alloc_req;
reg fl_free_req;
reg [PTR_W-1:0] fl_free_ptr;
reg rd_hit_pending;
reg [TOK_W-1:0] rd_hit_tok;
reg [SET_W-1:0] rd_hit_set;
reg [WAY_W-1:0] rd_hit_way;
reg r_issue_valid;
reg [TOK_W-1:0] r_issue_tok;
reg w_issue_valid;
reg [TOK_W-1:0] w_issue_tok;
reg dir_clear_all;
reg fl_reset_all;
reg pend_aw_found;
reg [TOK_W-1:0] pend_aw_tok;
// Stage-D / PR-D4 observability counters (accessible from TB via hierarchy).
reg [31:0] perf_cycles;
reg [31:0] perf_req_accept;
reg [31:0] perf_req_blocked;
reg [31:0] perf_haz_wake;
reg [31:0] perf_read_hit_issue;
reg [31:0] perf_read_miss_issue;
reg [31:0] perf_aw_issue;
reg [31:0] perf_retire;
reg [31:0] perf_req_read_accept;
reg [31:0] perf_req_write_accept;
reg [31:0] perf_req_stall_mshr_full;
reg [31:0] perf_req_stall_hazard;
reg [31:0] perf_cyc_wait_ar_ready;
reg [31:0] perf_cyc_wait_aw_ready;
reg [31:0] perf_cyc_wait_r_valid;
reg [31:0] perf_cyc_wait_b_valid;
reg [31:0] perf_sum_inflight;
reg [31:0] perf_sum_rq_depth;
reg [31:0] perf_sum_wq_depth;
reg [31:0] perf_peak_inflight;
reg [31:0] perf_peak_rq_depth;
reg [31:0] perf_peak_wq_depth;
reg [31:0] perf_retire_read;
reg [31:0] perf_retire_write;
reg any_wait_r;
reg any_wait_b;
reg perf_wait_ar_cycle;
reg perf_wait_aw_cycle;
reg perf_stall_mshr_cycle;
reg perf_stall_haz_cycle;

localparam LRU_LSB   = 0;
localparam PTR_LSB   = LRU_LSB + 4;
localparam TAG_LSB   = PTR_LSB + PTR_W;
localparam EPOCH_LSB = TAG_LSB + TAG_W;
localparam STATE_LSB = EPOCH_LSB + EPOCH_W;
localparam LOCK_BIT  = STATE_LSB + 2;
localparam DIRTY_BIT = LOCK_BIT + 1;
localparam VALID_BIT = DIRTY_BIT + 1;

assign lookup_set_w = hash_set_from_addr(lookup_addr_w);
assign lookup_tag_w = lookup_addr_w[ADDR_W-1 -: TAG_W];

assign xbnd_ready = (inflight_cnt < MSHR_NUM) && haz_push_accept;
assign xbwd_ready = !xbnd_valid && (inflight_cnt < MSHR_NUM) && haz_push_accept;
assign r_ready = 1'b1;
assign b_ready = 1'b1;

function [LINE_W-1:0] apply_wstrb;
    input [LINE_W-1:0] old_line;
    input [LINE_W-1:0] new_line;
    input [LINE_W/8-1:0] strb;
    integer b;
    begin
        apply_wstrb = old_line;
        for (b=0; b<LINE_W/8; b=b+1)
            if (strb[b]) apply_wstrb[b*8 +: 8] = new_line[b*8 +: 8];
    end
endfunction

function [LINE_W-1:0] apply_atomic;
    input [LINE_W-1:0] line_i;
    input [ADDR_W-1:0] addr_i;
    input [1:0] op_i;
    input size64_i;
    input [63:0] arg_i;
    input [63:0] cmp_i;
    reg [LINE_W-1:0] line_n;
    reg [63:0] old64,new64;
    reg [31:0] old32,new32;
    reg [3:0] idx64;
    reg [4:0] idx32;
    begin
        line_n = line_i;
        idx64 = addr_i[6:3];
        idx32 = addr_i[6:2];
        if (size64_i) begin
            old64 = line_i[idx64*64 +: 64];
            case (op_i)
                2'b00: new64 = old64 + arg_i;
                2'b01: new64 = old64 - arg_i;
                2'b10: new64 = arg_i;
                2'b11: new64 = (old64 == cmp_i) ? arg_i : old64;
                default: new64 = old64;
            endcase
            line_n[idx64*64 +: 64] = new64;
        end else begin
            old32 = line_i[idx32*32 +: 32];
            case (op_i)
                2'b00: new32 = old32 + arg_i[31:0];
                2'b01: new32 = old32 - arg_i[31:0];
                2'b10: new32 = arg_i[31:0];
                2'b11: new32 = (old32 == cmp_i[31:0]) ? arg_i[31:0] : old32;
                default: new32 = old32;
            endcase
            line_n[idx32*32 +: 32] = new32;
        end
        apply_atomic = line_n;
    end
endfunction

function [SET_W-1:0] hash_set_from_line;
    input [LINE_ADDR_W-1:0] line_addr;
    reg [SET_W-1:0] raw_set, mix1, mix2, t;
    begin
        raw_set = line_addr[SET_W-1:0];
        mix1 = (line_addr >> SET_W);
        mix2 = (line_addr >> (2*SET_W));
        case (INDEX_HASH_MODE)
            1: hash_set_from_line = raw_set ^ mix1 ^ mix2; // fermi-like xor hash
            2: begin // ipoly-like lightweight mixing
                t = raw_set ^ mix1;
                hash_set_from_line = t ^ {t[SET_W-2:0], t[SET_W-1]} ^ {t[0], t[SET_W-1:1]} ^ mix2;
            end
            default: hash_set_from_line = raw_set;
        endcase
    end
endfunction

function [SET_W-1:0] hash_set_from_addr;
    input [ADDR_W-1:0] addr_i;
    begin
        hash_set_from_addr = hash_set_from_line(addr_i[ADDR_W-1:OFFSET_BITS]);
    end
endfunction

always @(*) begin
    req_is_read = xbnd_valid;
    req_fire = (xbnd_valid && xbnd_ready) || ((!xbnd_valid) && xbwd_valid && xbwd_ready);
    req_id = req_is_read ? xbnd_id : xbwd_id;
    req_addr = req_is_read ? xbnd_addr : xbwd_addr;
    req_wdata = xbwd_wdata;
    req_wstrb = xbwd_wstrb;
    req_line_addr = req_addr[ADDR_W-1:OFFSET_BITS];
    lookup_addr_w = req_addr;
    if (r_valid && m_valid[r_id] && m_wait_r[r_id]) lookup_addr_w = m_addr[r_id];
    else if (r_issue_valid) lookup_addr_w = m_addr[r_issue_tok];
    else if (w_issue_valid) lookup_addr_w = m_addr[w_issue_tok];
    else if (r_q_cnt != 0) lookup_addr_w = m_addr[r_q[r_q_head]];
    else if (w_q_cnt != 0) lookup_addr_w = m_addr[w_q[w_q_head]];

    haz_push_req = xbnd_valid || ((!xbnd_valid) && xbwd_valid);
    haz_push_line_addr = req_line_addr;
    haz_push_tok = tail_ptr;

    retire_found = 1'b0;
    retire_tok_sel = head_ptr;
    if (RESP_IN_ORDER != 1) begin
      for (i=0; i<MSHR_NUM; i=i+1) begin
        if (!retire_found && m_valid[i] && m_done[i]) begin
          retire_found = 1'b1;
          retire_tok_sel = i[TOK_W-1:0];
        end
      end
    end

    pend_aw_found = 1'b0;
    pend_aw_tok = 0;
    any_wait_r = 1'b0;
    any_wait_b = 1'b0;
    for (i=0; i<MSHR_NUM; i=i+1) begin
        if (!pend_aw_found && m_valid[i] && m_need_aw[i] && !m_wait_r[i] && !m_wait_b[i]) begin
            pend_aw_found = 1'b1;
            pend_aw_tok = i[TOK_W-1:0];
        end
        if (m_valid[i] && m_wait_r[i]) any_wait_r = 1'b1;
        if (m_valid[i] && m_wait_b[i]) any_wait_b = 1'b1;
    end

    perf_stall_mshr_cycle =
      (xbnd_valid && !xbnd_ready && (inflight_cnt >= MSHR_NUM)) ||
      ((!xbnd_valid) && xbwd_valid && !xbwd_ready && (inflight_cnt >= MSHR_NUM));
    perf_stall_haz_cycle =
      (xbnd_valid && !xbnd_ready && (inflight_cnt < MSHR_NUM)) ||
      ((!xbnd_valid) && xbwd_valid && !xbwd_ready && (inflight_cnt < MSHR_NUM));

    perf_wait_ar_cycle =
      (r_issue_valid && !rd_hit_pending && !(!m_bypass[r_issue_tok] && dir_hit) && !ar_ready) ||
      (w_issue_valid && aw_ready &&
       ((m_is_atomic[w_issue_tok]) || (!dir_hit && m_calg[w_issue_tok][CALG_READBACK])) &&
       !ar_ready);
    perf_wait_aw_cycle =
      ((pend_aw_found || w_issue_valid) && !aw_ready);
end

gpu_l2_hazard_table #(
    .ADDR_W(ADDR_W), .OFFSET_BITS(OFFSET_BITS), .MSHR_NUM(MSHR_NUM),
    .HAZ_ENTRIES(HAZ_ENTRIES), .HAZ_WAYS(HAZ_WAYS)
) u_hazard (
    .clk(clk), .rst_n(rst_n),
    .push_req(haz_push_req), .push_line_addr(haz_push_line_addr), .push_tok(haz_push_tok),
    .push_accept(haz_push_accept), .push_blocked(haz_push_blocked),
    .retire_req(haz_retire_req), .retire_tok(haz_retire_tok),
    .wake_valid(haz_wake_valid), .wake_tok(haz_wake_tok)
);

gpu_l2_tag_dir #(
    .ADDR_W(ADDR_W), .SETS(SETS), .WAYS(WAYS), .OFFSET_BITS(OFFSET_BITS),
    .DATA_SLOTS(DATA_SLOTS), .EPOCH_W(EPOCH_W), .RD_SYNC(TAGDIR_RD_SYNC)
) u_tag_dir (
    .clk(clk), .rst_n(rst_n),
    .lookup_set(lookup_set_w), .lookup_tag(lookup_tag_w),
    .hit(dir_hit), .hit_way(dir_hit_way), .victim_found(dir_victim_found), .victim_way(dir_victim_way),
    .rd_set(dir_rd_set), .rd_way(dir_rd_way), .rd_entry(dir_rd_entry),
    .wr_en(dir_wr_en), .wr_set(dir_wr_set), .wr_way(dir_wr_way), .wr_entry(dir_wr_entry),
    .access_en(dir_access_en), .access_set(dir_access_set), .access_way(dir_access_way),
    .clear_all(dir_clear_all)
);

gpu_l2_data_pool #(.LINE_W(LINE_W), .DATA_SLOTS(DATA_SLOTS), .BANKS(DATA_BANKS)) u_data_pool (
    .clk(clk), .rst_n(rst_n),
    .rd_en(data_rd_en), .rd_ptr(data_rd_ptr), .rd_line(data_rd_line),
    .wr_en(data_wr_en), .wr_ptr(data_wr_ptr), .wr_line(data_wr_line), .wr_strb(data_wr_strb)
);

gpu_l2_freelist #(.DATA_SLOTS(DATA_SLOTS)) u_freelist (
    .clk(clk), .rst_n(rst_n),
    .reset_all(fl_reset_all),
    .alloc_req(fl_alloc_req), .alloc_gnt(fl_alloc_gnt), .alloc_ptr(fl_alloc_ptr),
    .free_req(fl_free_req), .free_ptr(fl_free_ptr), .free_count(fl_free_count)
);

always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
        head_ptr <= {TOK_W{1'b0}};
        tail_ptr <= {TOK_W{1'b0}};
        inflight_cnt <= {(TOK_W+1){1'b0}};
        r_q_head <= 0; r_q_tail <= 0; r_q_cnt <= 0;
        w_q_head <= 0; w_q_tail <= 0; w_q_cnt <= 0;

        xbwu_valid <= 1'b0; xbwu_id <= 0; xbwu_rdata <= 0; xbwu_resp <= 0;
        xbnu_valid <= 1'b0; xbnu_id <= 0; xbnu_resp <= 0;
        flush_ack <= 1'b0;

        ar_valid <= 1'b0; ar_id <= 0; ar_addr <= 0;
        aw_valid <= 1'b0; aw_id <= 0; aw_addr <= 0; aw_wdata <= 0; aw_wstrb <= 0; aw_is_atomic <= 0;

        haz_retire_req <= 1'b0;
        haz_retire_tok <= 0;
        dir_rd_set <= 0;
        dir_rd_way <= 0;
        dir_wr_en <= 1'b0;
        dir_wr_set <= 0;
        dir_wr_way <= 0;
        dir_wr_entry <= 0;
        dir_access_en <= 1'b0;
        dir_access_set <= 0;
        dir_access_way <= 0;
        data_rd_en <= 1'b0;
        data_rd_ptr <= 0;
        data_wr_en <= 1'b0;
        data_wr_ptr <= 0;
        data_wr_line <= 0;
        data_wr_strb <= 0;
        fl_alloc_req <= 1'b0;
        fl_free_req <= 1'b0;
        fl_free_ptr <= 0;
        rd_hit_pending <= 1'b0;
        rd_hit_tok <= 0;
        rd_hit_set <= 0;
        rd_hit_way <= 0;
        r_issue_valid <= 1'b0;
        r_issue_tok <= 0;
        w_issue_valid <= 1'b0;
        w_issue_tok <= 0;
        dir_clear_all <= 1'b0;
        fl_reset_all <= 1'b0;
        perf_cycles <= 32'd0;
        perf_req_accept <= 32'd0;
        perf_req_blocked <= 32'd0;
        perf_haz_wake <= 32'd0;
        perf_read_hit_issue <= 32'd0;
        perf_read_miss_issue <= 32'd0;
        perf_aw_issue <= 32'd0;
        perf_retire <= 32'd0;
        perf_req_read_accept <= 32'd0;
        perf_req_write_accept <= 32'd0;
        perf_req_stall_mshr_full <= 32'd0;
        perf_req_stall_hazard <= 32'd0;
        perf_cyc_wait_ar_ready <= 32'd0;
        perf_cyc_wait_aw_ready <= 32'd0;
        perf_cyc_wait_r_valid <= 32'd0;
        perf_cyc_wait_b_valid <= 32'd0;
        perf_sum_inflight <= 32'd0;
        perf_sum_rq_depth <= 32'd0;
        perf_sum_wq_depth <= 32'd0;
        perf_peak_inflight <= 32'd0;
        perf_peak_rq_depth <= 32'd0;
        perf_peak_wq_depth <= 32'd0;
        perf_retire_read <= 32'd0;
        perf_retire_write <= 32'd0;

        for (i=0; i<MSHR_NUM; i=i+1) begin
            m_valid[i] <= 0; m_is_read[i] <= 0; m_id[i] <= 0; m_addr[i] <= 0;
            m_wdata[i] <= 0; m_wstrb[i] <= 0; m_aw_line[i] <= 0; m_is_atomic[i] <= 0; m_atomic_op[i] <= 0; m_atomic_size[i] <= 0; m_atomic_arg[i] <= 0; m_atomic_cmp[i] <= 0;
            m_bypass[i] <= 0; m_calg[i] <= 0; m_no_fill[i] <= 0; m_need_aw[i] <= 0;
            m_blocked[i] <= 0; m_issued[i] <= 0;
            m_wait_r[i] <= 0; m_wait_b[i] <= 0; m_done[i] <= 0; m_in_issue_q[i] <= 0;
            m_resp[i] <= 0; m_rdata[i] <= 0;
            r_q[i] <= 0; w_q[i] <= 0;
        end
    end else begin
        perf_cycles <= perf_cycles + 1'b1;
        perf_sum_inflight <= perf_sum_inflight + inflight_cnt;
        perf_sum_rq_depth <= perf_sum_rq_depth + r_q_cnt;
        perf_sum_wq_depth <= perf_sum_wq_depth + w_q_cnt;
        if (inflight_cnt > perf_peak_inflight) perf_peak_inflight <= inflight_cnt;
        if (r_q_cnt > perf_peak_rq_depth) perf_peak_rq_depth <= r_q_cnt;
        if (w_q_cnt > perf_peak_wq_depth) perf_peak_wq_depth <= w_q_cnt;
        if (perf_stall_mshr_cycle) perf_req_stall_mshr_full <= perf_req_stall_mshr_full + 1'b1;
        if (perf_stall_haz_cycle) perf_req_stall_hazard <= perf_req_stall_hazard + 1'b1;
        if (perf_wait_ar_cycle) perf_cyc_wait_ar_ready <= perf_cyc_wait_ar_ready + 1'b1;
        if (perf_wait_aw_cycle) perf_cyc_wait_aw_ready <= perf_cyc_wait_aw_ready + 1'b1;
        if (any_wait_r && !r_valid) perf_cyc_wait_r_valid <= perf_cyc_wait_r_valid + 1'b1;
        if (any_wait_b && !b_valid) perf_cyc_wait_b_valid <= perf_cyc_wait_b_valid + 1'b1;
        ar_valid <= 1'b0;
        aw_valid <= 1'b0;
        flush_ack <= 1'b0;
        haz_retire_req <= 1'b0;
        dir_wr_en <= 1'b0;
        dir_access_en <= 1'b0;
        data_rd_en <= 1'b0;
        data_wr_en <= 1'b0;
        fl_alloc_req <= 1'b0;
        fl_free_req <= 1'b0;
        dir_clear_all <= 1'b0;
        fl_reset_all <= 1'b0;
        if (rd_hit_pending) begin
            m_done[rd_hit_tok] <= 1'b1;
            m_resp[rd_hit_tok] <= {RESP_W{1'b0}};
            m_rdata[rd_hit_tok] <= data_rd_line;
            dir_access_en <= 1'b1;
            dir_access_set <= rd_hit_set;
            dir_access_way <= rd_hit_way;
            rd_hit_pending <= 1'b0;
        end

        if (xbwu_valid && xbwu_ready) xbwu_valid <= 1'b0;
        if (xbnu_valid && xbnu_ready) xbnu_valid <= 1'b0;

        if (flush_req || cache_inv) begin
            dir_clear_all <= 1'b1;
            fl_reset_all <= 1'b1;
            flush_ack <= 1'b1;
            head_ptr <= 0;
            tail_ptr <= 0;
            inflight_cnt <= 0;
            r_q_head <= 0; r_q_tail <= 0; r_q_cnt <= 0;
            w_q_head <= 0; w_q_tail <= 0; w_q_cnt <= 0;
            rd_hit_pending <= 1'b0;
            r_issue_valid <= 1'b0;
            w_issue_valid <= 1'b0;
            xbwu_valid <= 1'b0;
            xbnu_valid <= 1'b0;
            for (i=0; i<MSHR_NUM; i=i+1) begin
                m_valid[i] <= 0;
                m_wait_r[i] <= 0;
                m_wait_b[i] <= 0;
                m_done[i] <= 0;
                m_in_issue_q[i] <= 0;
                m_blocked[i] <= 0;
                m_issued[i] <= 0;
                m_need_aw[i] <= 0;
            end
        end else begin

        // lookup defaults (new request address)
        dir_rd_set <= lookup_set_w;
        dir_rd_way <= dir_hit ? dir_hit_way : dir_victim_way;
        data_rd_ptr <= dir_rd_entry[PTR_LSB +: PTR_W];

        // allocate token + bind hazard blocked flag
        if (req_fire) begin
            perf_req_accept <= perf_req_accept + 1'b1;
            if (haz_push_blocked) perf_req_blocked <= perf_req_blocked + 1'b1;
            if (req_is_read) perf_req_read_accept <= perf_req_read_accept + 1'b1;
            else perf_req_write_accept <= perf_req_write_accept + 1'b1;
            m_valid[tail_ptr] <= 1'b1;
            m_is_read[tail_ptr] <= req_is_read;
            m_id[tail_ptr] <= req_id;
            m_addr[tail_ptr] <= req_addr;
            m_wdata[tail_ptr] <= req_wdata;
            m_wstrb[tail_ptr] <= req_wstrb;
            m_aw_line[tail_ptr] <= req_wdata;
            m_is_atomic[tail_ptr] <= (!req_is_read) && xbwd_is_atomic;
            m_atomic_op[tail_ptr] <= xbwd_atomic_op;
            m_atomic_size[tail_ptr] <= xbwd_atomic_size;
            m_atomic_arg[tail_ptr] <= xbwd_atomic_arg;
            m_atomic_cmp[tail_ptr] <= xbwd_atomic_cmp;
            m_bypass[tail_ptr] <= req_is_read ? (xbnd_bypass | xbnd_calg[CALG_BYPASS]) : (xbwd_bypass | xbwd_calg[CALG_BYPASS]);
            m_calg[tail_ptr] <= req_is_read ? xbnd_calg : xbwd_calg;
            m_no_fill[tail_ptr] <= req_is_read ? (xbnd_bypass | xbnd_calg[CALG_BYPASS]) :
                                   ((xbwd_bypass | xbwd_calg[CALG_BYPASS]) | (xbwd_calg[CALG_NOINIT] && (&xbwd_wstrb)));
            m_need_aw[tail_ptr] <= 1'b0;
            m_blocked[tail_ptr] <= haz_push_blocked;
            m_issued[tail_ptr] <= 1'b0;
            m_wait_r[tail_ptr] <= 1'b0;
            m_wait_b[tail_ptr] <= 1'b0;
            m_done[tail_ptr] <= 1'b0;
            m_in_issue_q[tail_ptr] <= !haz_push_blocked;
            m_resp[tail_ptr] <= 0;
            m_rdata[tail_ptr] <= 0;

            if (!haz_push_blocked) begin
                if (req_is_read) begin
                    r_q[r_q_tail] <= tail_ptr;
                    r_q_tail <= r_q_tail + 1'b1;
                    r_q_cnt <= r_q_cnt + 1'b1;
                end else begin
                    w_q[w_q_tail] <= tail_ptr;
                    w_q_tail <= w_q_tail + 1'b1;
                    w_q_cnt <= w_q_cnt + 1'b1;
                end
            end

            tail_ptr <= tail_ptr + 1'b1;
            inflight_cnt <= inflight_cnt + 1'b1;
        end

        // wake successor from hazard
        if (haz_wake_valid && m_valid[haz_wake_tok] && !m_issued[haz_wake_tok] && !m_in_issue_q[haz_wake_tok]) begin
            perf_haz_wake <= perf_haz_wake + 1'b1;
            m_blocked[haz_wake_tok] <= 1'b0;
            m_in_issue_q[haz_wake_tok] <= 1'b1;
            if (m_is_read[haz_wake_tok]) begin
                r_q[r_q_tail] <= haz_wake_tok;
                r_q_tail <= r_q_tail + 1'b1;
                r_q_cnt <= r_q_cnt + 1'b1;
            end else begin
                w_q[w_q_tail] <= haz_wake_tok;
                w_q_tail <= w_q_tail + 1'b1;
                w_q_cnt <= w_q_cnt + 1'b1;
            end
        end

        // stage-0 issue token capture
        if (!r_issue_valid && (r_q_cnt != 0) && !rd_hit_pending) begin
            r_issue_valid <= 1'b1;
            r_issue_tok <= r_q[r_q_head];
        end
        if (!w_issue_valid && (w_q_cnt != 0)) begin
            w_issue_valid <= 1'b1;
            w_issue_tok <= w_q[w_q_head];
        end

        // independent AR issue path (stage-1)
        if (r_issue_valid && !rd_hit_pending) begin
            dir_rd_set <= hash_set_from_addr(m_addr[r_issue_tok]);
            if (!m_bypass[r_issue_tok] && dir_hit) begin
                perf_read_hit_issue <= perf_read_hit_issue + 1'b1;
                // read hit: directory ptr -> data pool
                data_rd_en <= 1'b1;
                data_rd_ptr <= dir_rd_entry[PTR_LSB +: PTR_W];
                m_issued[r_issue_tok] <= 1'b1;
                m_in_issue_q[r_issue_tok] <= 1'b0;
                rd_hit_pending <= 1'b1;
                rd_hit_tok <= r_issue_tok;
                rd_hit_set <= hash_set_from_addr(m_addr[r_issue_tok]);
                rd_hit_way <= dir_hit_way;
                r_q_head <= r_q_head + 1'b1;
                r_q_cnt <= r_q_cnt - 1'b1;
                r_issue_valid <= 1'b0;
            end else if (ar_ready) begin
                perf_read_miss_issue <= perf_read_miss_issue + 1'b1;
                // read miss: memory fetch, refill directory/data pool on response
                ar_valid <= 1'b1;
                ar_id <= r_issue_tok;
                ar_addr <= m_addr[r_issue_tok];
                m_issued[r_issue_tok] <= 1'b1;
                m_wait_r[r_issue_tok] <= 1'b1;
                m_in_issue_q[r_issue_tok] <= 1'b0;
                r_q_head <= r_q_head + 1'b1;
                r_q_cnt <= r_q_cnt - 1'b1;
                r_issue_valid <= 1'b0;
            end
        end

        // pending AW-from-readback/atomic path has priority
        if (pend_aw_found && aw_ready) begin
                perf_aw_issue <= perf_aw_issue + 1'b1;
                aw_valid <= 1'b1;
                aw_id <= pend_aw_tok;
                aw_addr <= m_addr[pend_aw_tok];
                aw_wdata <= m_aw_line[pend_aw_tok];
                aw_wstrb <= m_is_atomic[pend_aw_tok] ? {LINE_W/8{1'b1}} : m_wstrb[pend_aw_tok];
                aw_is_atomic <= m_is_atomic[pend_aw_tok];
                m_need_aw[pend_aw_tok] <= 1'b0;
                m_wait_b[pend_aw_tok] <= 1'b1;
        end else if (w_issue_valid) begin
            if (aw_ready) begin
                perf_aw_issue <= perf_aw_issue + 1'b1;
                aw_valid <= 1'b1;
                aw_id <= w_issue_tok;
                aw_addr <= m_addr[w_issue_tok];
                aw_wdata <= m_wdata[w_issue_tok];
                aw_wstrb <= m_wstrb[w_issue_tok];
                aw_is_atomic <= m_is_atomic[w_issue_tok];
                // write path: try update directory/data path as write-allocate
                dir_rd_set <= hash_set_from_addr(m_addr[w_issue_tok]);
                if (m_is_atomic[w_issue_tok]) begin
                    // atomic uses read-modify-write path via AR data
                    if (ar_ready) begin
                        ar_valid <= 1'b1;
                        ar_id <= w_issue_tok;
                        ar_addr <= m_addr[w_issue_tok];
                        m_wait_r[w_issue_tok] <= 1'b1;
                        m_need_aw[w_issue_tok] <= 1'b1;
                        aw_valid <= 1'b0;
                    end else begin
                        aw_valid <= 1'b0;
                    end
                end else if (!dir_hit && m_calg[w_issue_tok][CALG_READBACK]) begin
                    // read-back miss path: AR then merge -> AW
                    if (ar_ready) begin
                        ar_valid <= 1'b1;
                        ar_id <= w_issue_tok;
                        ar_addr <= m_addr[w_issue_tok];
                        m_wait_r[w_issue_tok] <= 1'b1;
                        m_need_aw[w_issue_tok] <= 1'b1;
                        aw_valid <= 1'b0;
                    end else begin
                        aw_valid <= 1'b0;
                    end
                end else if (m_bypass[w_issue_tok]) begin
                    // bypass write: lower-memory path only, no local allocation/update
                end else if ((r_q_cnt == 0) && dir_hit) begin
                    data_wr_en <= 1'b1;
                    data_wr_ptr <= dir_rd_entry[PTR_LSB +: PTR_W];
                    data_wr_line <= m_wdata[w_issue_tok];
                    data_wr_strb <= m_wstrb[w_issue_tok];
                    dir_wr_en <= 1'b1;
                    dir_wr_set <= hash_set_from_addr(m_addr[w_issue_tok]);
                    dir_wr_way <= dir_hit_way;
                    dir_wr_entry <= {1'b1,1'b1,(dir_rd_entry[LOCK_BIT] | m_calg[w_issue_tok][CALG_LOCK]),dir_rd_entry[STATE_LSB +: 2],dir_rd_entry[EPOCH_LSB +: EPOCH_W],m_addr[w_issue_tok][ADDR_W-1 -: TAG_W],dir_rd_entry[PTR_LSB +: PTR_W],4'd0};
                    dir_access_en <= 1'b1;
                    dir_access_set <= hash_set_from_addr(m_addr[w_issue_tok]);
                    dir_access_way <= dir_hit_way;
                end else if (m_calg[w_issue_tok][CALG_NOINIT] && (&m_wstrb[w_issue_tok])) begin
                    // no_init full-line write miss: do not allocate in local cache
                end else if ((r_q_cnt == 0) && dir_victim_found && fl_alloc_gnt) begin
                    fl_alloc_req <= 1'b1;
                    if (dir_rd_entry[VALID_BIT]) begin
                        fl_free_req <= 1'b1;
                        fl_free_ptr <= dir_rd_entry[PTR_LSB +: PTR_W];
                    end
                    data_wr_en <= 1'b1;
                    data_wr_ptr <= fl_alloc_ptr;
                    data_wr_line <= m_wdata[w_issue_tok];
                    data_wr_strb <= m_wstrb[w_issue_tok];
                    dir_wr_en <= 1'b1;
                    dir_wr_set <= hash_set_from_addr(m_addr[w_issue_tok]);
                    dir_wr_way <= dir_victim_way;
                    dir_wr_entry <= {1'b1,1'b1,m_calg[w_issue_tok][CALG_LOCK],2'b00,{EPOCH_W{1'b0}},m_addr[w_issue_tok][ADDR_W-1 -: TAG_W],fl_alloc_ptr,4'd0};
                    dir_access_en <= 1'b1;
                    dir_access_set <= hash_set_from_addr(m_addr[w_issue_tok]);
                    dir_access_way <= dir_victim_way;
                end
                m_issued[w_issue_tok] <= 1'b1;
                if (!(m_is_atomic[w_issue_tok] || (!dir_hit && m_calg[w_issue_tok][CALG_READBACK])))
                    m_wait_b[w_issue_tok] <= 1'b1;
                m_in_issue_q[w_issue_tok] <= 1'b0;
                w_q_head <= w_q_head + 1'b1;
                w_q_cnt <= w_q_cnt - 1'b1;
                w_issue_valid <= 1'b0;
            end
        end

        // response capture
        if (r_valid && m_valid[r_id] && m_wait_r[r_id]) begin
            m_wait_r[r_id] <= 1'b0;
            m_resp[r_id] <= r_resp;
            if (m_need_aw[r_id]) begin
                if (m_is_atomic[r_id])
                    m_aw_line[r_id] <= apply_atomic(r_data, m_addr[r_id], m_atomic_op[r_id], m_atomic_size[r_id], m_atomic_arg[r_id], m_atomic_cmp[r_id]);
                else
                    m_aw_line[r_id] <= apply_wstrb(r_data, m_wdata[r_id], m_wstrb[r_id]);

                // optional cache fill/update from read-back data
                if (!m_no_fill[r_id] && dir_victim_found && fl_alloc_gnt) begin
                    fl_alloc_req <= 1'b1;
                    if (dir_rd_entry[VALID_BIT]) begin
                        fl_free_req <= 1'b1;
                        fl_free_ptr <= dir_rd_entry[PTR_LSB +: PTR_W];
                    end
                    data_wr_en <= 1'b1;
                    data_wr_ptr <= fl_alloc_ptr;
                    data_wr_line <= m_is_atomic[r_id] ? apply_atomic(r_data, m_addr[r_id], m_atomic_op[r_id], m_atomic_size[r_id], m_atomic_arg[r_id], m_atomic_cmp[r_id]) :
                                                     apply_wstrb(r_data, m_wdata[r_id], m_wstrb[r_id]);
                    data_wr_strb <= {LINE_W/8{1'b1}};
                    dir_wr_en <= 1'b1;
                    dir_wr_set <= hash_set_from_addr(m_addr[r_id]);
                    dir_wr_way <= dir_victim_way;
                    dir_wr_entry <= {1'b1,1'b1,m_calg[r_id][CALG_LOCK],2'b00,{EPOCH_W{1'b0}},m_addr[r_id][ADDR_W-1 -: TAG_W],fl_alloc_ptr,4'd0};
                    dir_access_en <= 1'b1;
                    dir_access_set <= hash_set_from_addr(m_addr[r_id]);
                    dir_access_way <= dir_victim_way;
                end
            end else begin
                m_done[r_id] <= 1'b1;
                m_rdata[r_id] <= r_data;
                // read-miss refill
                if (!m_no_fill[r_id] && dir_victim_found && fl_alloc_gnt) begin
                    fl_alloc_req <= 1'b1;
                    if (dir_rd_entry[VALID_BIT]) begin
                        fl_free_req <= 1'b1;
                        fl_free_ptr <= dir_rd_entry[PTR_LSB +: PTR_W];
                    end
                    data_wr_en <= 1'b1;
                    data_wr_ptr <= fl_alloc_ptr;
                    data_wr_line <= r_data;
                    data_wr_strb <= {LINE_W/8{1'b1}};
                    dir_wr_en <= 1'b1;
                    dir_wr_set <= hash_set_from_addr(m_addr[r_id]);
                    dir_wr_way <= dir_victim_way;
                    dir_wr_entry <= {1'b1,1'b0,m_calg[r_id][CALG_LOCK],2'b00,{EPOCH_W{1'b0}},m_addr[r_id][ADDR_W-1 -: TAG_W],fl_alloc_ptr,4'd0};
                    dir_access_en <= 1'b1;
                    dir_access_set <= hash_set_from_addr(m_addr[r_id]);
                    dir_access_way <= dir_victim_way;
                end
            end
        end
        if (b_valid && m_valid[b_id] && m_wait_b[b_id]) begin
            m_wait_b[b_id] <= 1'b0;
            m_done[b_id] <= 1'b1;
            m_resp[b_id] <= b_resp;
        end

        // retire response
        if ((RESP_IN_ORDER==1 && m_valid[head_ptr] && m_done[head_ptr]) ||
            (RESP_IN_ORDER!=1 && retire_found)) begin

            if (RESP_IN_ORDER==1) retire_tok_sel <= head_ptr;

            if (m_is_read[retire_tok_sel]) begin
                if (!xbwu_valid || xbwu_ready) begin
                    xbwu_valid <= 1'b1;
                    xbwu_id <= m_id[retire_tok_sel];
                    xbwu_rdata <= m_rdata[retire_tok_sel];
                    xbwu_resp <= m_resp[retire_tok_sel];

                    haz_retire_req <= 1'b1;
                    haz_retire_tok <= retire_tok_sel;
                    perf_retire <= perf_retire + 1'b1;
                    perf_retire_read <= perf_retire_read + 1'b1;

                    m_valid[retire_tok_sel] <= 1'b0;
                    if (RESP_IN_ORDER==1) head_ptr <= head_ptr + 1'b1;
                    inflight_cnt <= inflight_cnt - 1'b1;
                end
            end else begin
                if (!xbnu_valid || xbnu_ready) begin
                    xbnu_valid <= 1'b1;
                    xbnu_id <= m_id[retire_tok_sel];
                    xbnu_resp <= m_resp[retire_tok_sel];

                    haz_retire_req <= 1'b1;
                    haz_retire_tok <= retire_tok_sel;
                    perf_retire <= perf_retire + 1'b1;
                    perf_retire_write <= perf_retire_write + 1'b1;

                    m_valid[retire_tok_sel] <= 1'b0;
                    if (RESP_IN_ORDER==1) head_ptr <= head_ptr + 1'b1;
                    inflight_cnt <= inflight_cnt - 1'b1;
                end
            end
        end
        end
end
end

`ifndef SYNTHESIS
always @(posedge clk) begin
    if (rst_n) begin
        if (fl_free_count > DATA_SLOTS) begin
            $fatal(1, "freelist free_count overflow: %0d", fl_free_count);
        end
        if (req_fire && m_valid[tail_ptr]) begin
            $fatal(1, "token overwrite at tail_ptr=%0d", tail_ptr);
        end
    end
end
`endif

endmodule
