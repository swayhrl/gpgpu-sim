module gpu_l2_cache #(
    parameter ADDR_W      = 40,
    parameter ID_W        = 8,
    parameter LINE_W      = 1024,
    parameter SETS        = 64,
    parameter WAYS        = 16,
    parameter OFFSET_BITS = 7,
    parameter RESP_W      = 2,
    parameter MSHR_NUM    = 64,
    parameter MSHR_W      = 6,
    parameter INDEX_HASH_MODE = 0 // 0:none 1:fermi-xor 2:ipoly-like
) (
    input                       clk,
    input                       rst_n,

    input                       xbnd_valid,
    output                      xbnd_ready,
    input      [ID_W-1:0]       xbnd_id,
    input      [ADDR_W-1:0]     xbnd_addr,
    input                       xbnd_bypass,
    input      [4:0]            xbnd_calg, // [0]inv [1]bypass [2]no_init [3]read_back [4]lock

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
    output reg [MSHR_W-1:0]     ar_id,
    output reg [ADDR_W-1:0]     ar_addr,

    output reg                  aw_valid,
    input                       aw_ready,
    output reg [MSHR_W-1:0]     aw_id,
    output reg [ADDR_W-1:0]     aw_addr,
    output reg [LINE_W-1:0]     aw_wdata,
    output reg [LINE_W/8-1:0]   aw_wstrb,
    output reg                  aw_is_atomic,

    input                       r_valid,
    output                      r_ready,
    input      [MSHR_W-1:0]     r_id,
    input      [LINE_W-1:0]     r_data,
    input      [RESP_W-1:0]     r_resp,

    input                       b_valid,
    output                      b_ready,
    input      [MSHR_W-1:0]     b_id,
    input      [RESP_W-1:0]     b_resp
);

localparam TAG_W = ADDR_W - OFFSET_BITS - 6;
localparam LINE_ADDR_W = ADDR_W - OFFSET_BITS;
localparam REQ_READ=2'b01, REQ_WRITE=2'b10, REQ_ATOMIC=2'b11;
localparam CTRL_IDLE=2'd0, CTRL_FLUSH=2'd1, CTRL_INV=2'd2;
localparam CALG_INV=0, CALG_BYPASS=1, CALG_NOINIT=2, CALG_READBACK=3, CALG_LOCK=4;

reg  [5:0] tag_lookup_set;
reg  [TAG_W-1:0] tag_lookup_tag;
wire tag_hit;
wire [3:0] tag_hit_way;
wire [3:0] tag_victim_way;
wire tag_victim_found;
reg  [5:0] tag_query_set;
reg  [3:0] tag_query_way;
wire tag_query_valid, tag_query_dirty, tag_query_lock;
wire [TAG_W-1:0] tag_query_tag;
reg tag_access_en;
reg [5:0] tag_access_set;
reg [3:0] tag_access_way;
reg tag_fill_en;
reg [5:0] tag_fill_set;
reg [3:0] tag_fill_way;
reg [TAG_W-1:0] tag_fill_tag;
reg tag_fill_valid, tag_fill_dirty, tag_fill_lock;
reg tag_set_dirty_en;
reg [5:0] tag_set_dirty_set;
reg [3:0] tag_set_dirty_way;
reg tag_set_dirty_value;
reg tag_set_lock_en;
reg [5:0] tag_set_lock_set;
reg [3:0] tag_set_lock_way;
reg tag_set_lock_value;
reg tag_inv_en;
reg [5:0] tag_inv_set;
reg [3:0] tag_inv_way;

reg  [5:0] data_rd_set;
reg  [3:0] data_rd_way;
wire [LINE_W-1:0] data_rd_line;
reg data_wr_en;
reg [5:0] data_wr_set;
reg [3:0] data_wr_way;
reg [LINE_W-1:0] data_wr_line;
reg [LINE_W/8-1:0] data_wr_strb;

reg [LINE_W-1:0] atom_line_i;
reg [ADDR_W-1:0] atom_addr_i;
reg [1:0] atom_op_i;
reg atom_size_i;
reg [63:0] atom_arg_i;
reg [63:0] atom_cmp_i;
wire [LINE_W-1:0] atom_line_o;

reg                 m_valid       [0:MSHR_NUM-1];
reg [1:0]           m_kind        [0:MSHR_NUM-1];
reg [4:0]           m_calg        [0:MSHR_NUM-1];
reg [ID_W-1:0]      m_up_id       [0:MSHR_NUM-1];
reg [ADDR_W-1:0]    m_addr        [0:MSHR_NUM-1];
reg [LINE_ADDR_W-1:0] m_line_addr [0:MSHR_NUM-1];
reg                 m_need_ar     [0:MSHR_NUM-1];
reg                 m_need_aw     [0:MSHR_NUM-1];
reg                 m_wait_r      [0:MSHR_NUM-1];
reg                 m_wait_b      [0:MSHR_NUM-1];
reg                 m_need_fill   [0:MSHR_NUM-1];
reg                 m_done        [0:MSHR_NUM-1];
reg [3:0]           m_way         [0:MSHR_NUM-1];
reg [LINE_W-1:0]    m_rdata       [0:MSHR_NUM-1];
reg [RESP_W-1:0]    m_resp        [0:MSHR_NUM-1];
reg [LINE_W-1:0]    m_aw_line     [0:MSHR_NUM-1];
reg [LINE_W/8-1:0]  m_wstrb       [0:MSHR_NUM-1];
reg [1:0]           m_aop         [0:MSHR_NUM-1];
reg                 m_asize       [0:MSHR_NUM-1];
reg [63:0]          m_aarg        [0:MSHR_NUM-1];
reg [63:0]          m_acmp        [0:MSHR_NUM-1];

reg [MSHR_W-1:0] head_ptr, tail_ptr;
reg [MSHR_W:0] inflight_cnt;

reg [1:0] ctrl_state;
reg flush_pending;
reg [5:0] scan_set;
reg [3:0] scan_way;
reg flush_wait_b;

integer i;
reg has_line_conflict;
reg [MSHR_W-1:0] issue_ar_idx, issue_aw_idx;
reg issue_ar_found, issue_aw_found;
reg [LINE_ADDR_W-1:0] req_line_addr;
reg [5:0] req_set;
reg [TAG_W-1:0] req_tag;
reg [1:0] req_kind;
reg req_sel_xbnd;
reg [4:0] req_calg;
reg req_bypass_eff;
wire can_accept_req;

assign r_ready = 1'b1;
assign b_ready = 1'b1;

gpu_l2_tag_array #(.ADDR_W(ADDR_W), .SETS(SETS), .WAYS(WAYS), .OFFSET_BITS(OFFSET_BITS)) u_tag (
    .clk(clk), .rst_n(rst_n),
    .lookup_set(tag_lookup_set), .lookup_tag(tag_lookup_tag), .hit(tag_hit), .hit_way(tag_hit_way), .victim_way(tag_victim_way), .victim_found(tag_victim_found),
    .query_set(tag_query_set), .query_way(tag_query_way), .query_valid(tag_query_valid), .query_dirty(tag_query_dirty), .query_lock(tag_query_lock), .query_tag(tag_query_tag),
    .access_en(tag_access_en), .access_set(tag_access_set), .access_way(tag_access_way),
    .fill_en(tag_fill_en), .fill_set(tag_fill_set), .fill_way(tag_fill_way), .fill_tag(tag_fill_tag), .fill_valid(tag_fill_valid), .fill_dirty(tag_fill_dirty), .fill_lock(tag_fill_lock),
    .set_dirty_en(tag_set_dirty_en), .set_dirty_set(tag_set_dirty_set), .set_dirty_way(tag_set_dirty_way), .set_dirty_value(tag_set_dirty_value),
    .set_lock_en(tag_set_lock_en), .set_lock_set(tag_set_lock_set), .set_lock_way(tag_set_lock_way), .set_lock_value(tag_set_lock_value),
    .inv_en(tag_inv_en), .inv_set(tag_inv_set), .inv_way(tag_inv_way)
);

gpu_l2_data_array #(.LINE_W(LINE_W), .SETS(SETS), .WAYS(WAYS)) u_data (
    .clk(clk), .rst_n(rst_n), .rd_set(data_rd_set), .rd_way(data_rd_way), .rd_line(data_rd_line),
    .wr_en(data_wr_en), .wr_set(data_wr_set), .wr_way(data_wr_way), .wr_line(data_wr_line), .wr_strb(data_wr_strb)
);

gpu_l2_atom_unit #(.LINE_W(LINE_W), .ADDR_W(ADDR_W)) u_atom (
    .line_i(atom_line_i), .addr_i(atom_addr_i), .op_i(atom_op_i), .size64_i(atom_size_i), .arg_i(atom_arg_i), .cmp_i(atom_cmp_i), .line_o(atom_line_o)
);

function line_inflight;
    input [LINE_ADDR_W-1:0] line_addr;
    integer t;
    begin
        line_inflight = 1'b0;
        for (t=0; t<MSHR_NUM; t=t+1)
            if (m_valid[t] && m_line_addr[t]==line_addr) line_inflight = 1'b1;
    end
endfunction

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

function is_full_strb;
    input [LINE_W/8-1:0] strb;
    begin
        is_full_strb = &strb;
    end
endfunction

function [5:0] hash_set_from_line;
    input [LINE_ADDR_W-1:0] line_addr;
    reg [5:0] raw_set, mix1, mix2, t;
    begin
        raw_set = line_addr[5:0];
        mix1 = line_addr[11:6];
        mix2 = line_addr[17:12];
        case (INDEX_HASH_MODE)
            1: hash_set_from_line = raw_set ^ mix1 ^ mix2; // fermi-like xor hash
            2: begin // ipoly-like lightweight mixing
                t = raw_set ^ mix1;
                hash_set_from_line = t ^ {t[4:0], t[5]} ^ {t[0], t[5:1]} ^ mix2;
            end
            default: hash_set_from_line = raw_set;
        endcase
    end
endfunction

function [5:0] hash_set_from_addr;
    input [ADDR_W-1:0] addr;
    begin
        hash_set_from_addr = hash_set_from_line(addr[ADDR_W-1:OFFSET_BITS]);
    end
endfunction

always @(*) begin
    req_sel_xbnd = xbnd_valid;
    req_kind = req_sel_xbnd ? REQ_READ : (xbwd_is_atomic ? REQ_ATOMIC : REQ_WRITE);
    req_calg = req_sel_xbnd ? xbnd_calg : xbwd_calg;
    req_line_addr = req_sel_xbnd ? xbnd_addr[ADDR_W-1:OFFSET_BITS] : xbwd_addr[ADDR_W-1:OFFSET_BITS];
    req_set = hash_set_from_line(req_line_addr);
    req_tag = req_sel_xbnd ? xbnd_addr[ADDR_W-1:OFFSET_BITS+6] : xbwd_addr[ADDR_W-1:OFFSET_BITS+6];
    req_bypass_eff = (req_sel_xbnd ? xbnd_bypass : xbwd_bypass) | req_calg[CALG_BYPASS];

    tag_lookup_set = req_set;
    tag_lookup_tag = req_tag;
    data_rd_set = req_set;
    data_rd_way = tag_hit_way;

    tag_query_set = scan_set;
    tag_query_way = scan_way;

    has_line_conflict = 1'b0;
    for (i=0; i<MSHR_NUM; i=i+1)
        if (m_valid[i] && m_line_addr[i] == req_line_addr) has_line_conflict = 1'b1;

    issue_ar_found = 1'b0; issue_ar_idx = 0;
    issue_aw_found = 1'b0; issue_aw_idx = 0;
    for (i=0; i<MSHR_NUM; i=i+1) begin
        if (!issue_ar_found && m_valid[i] && m_need_ar[i]) begin issue_ar_found = 1'b1; issue_ar_idx = i[MSHR_W-1:0]; end
        if (!issue_aw_found && m_valid[i] && m_need_aw[i]) begin issue_aw_found = 1'b1; issue_aw_idx = i[MSHR_W-1:0]; end
    end

    atom_line_i = data_rd_line;
    atom_addr_i = req_sel_xbnd ? xbnd_addr : xbwd_addr;
    atom_op_i = xbwd_atomic_op;
    atom_size_i = xbwd_atomic_size;
    atom_arg_i = xbwd_atomic_arg;
    atom_cmp_i = xbwd_atomic_cmp;
end

assign can_accept_req = (ctrl_state == CTRL_IDLE) && (inflight_cnt < MSHR_NUM) && !has_line_conflict;
assign xbnd_ready = rst_n && can_accept_req;
assign xbwd_ready = rst_n && !xbnd_valid && can_accept_req;

always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
        head_ptr <= 0; tail_ptr <= 0; inflight_cnt <= 0;
        xbwu_valid <= 0; xbnu_valid <= 0; flush_ack <= 0;
        ar_valid <= 0; aw_valid <= 0; ar_id <= 0; aw_id <= 0;
        ar_addr <= 0; aw_addr <= 0; aw_wdata <= 0; aw_wstrb <= 0; aw_is_atomic <= 0;
        ctrl_state <= CTRL_IDLE; flush_pending <= 0; scan_set <= 0; scan_way <= 0; flush_wait_b <= 0;

        for (i=0; i<MSHR_NUM; i=i+1) begin
            m_valid[i] <= 0; m_kind[i] <= 0; m_calg[i] <= 0; m_up_id[i] <= 0; m_addr[i] <= 0; m_line_addr[i] <= 0;
            m_need_ar[i] <= 0; m_need_aw[i] <= 0; m_wait_r[i] <= 0; m_wait_b[i] <= 0; m_need_fill[i] <= 0; m_done[i] <= 0;
            m_way[i] <= 0; m_rdata[i] <= 0; m_resp[i] <= 0; m_aw_line[i] <= 0; m_wstrb[i] <= 0;
            m_aop[i] <= 0; m_asize[i] <= 0; m_aarg[i] <= 0; m_acmp[i] <= 0;
        end

        tag_access_en <= 0; tag_fill_en <= 0; tag_set_dirty_en <= 0; tag_inv_en <= 0; tag_set_lock_en <= 0;
        data_wr_en <= 0;
    end else begin
        flush_ack <= 1'b0;
        ar_valid <= 1'b0; aw_valid <= 1'b0;
        tag_access_en <= 1'b0; tag_fill_en <= 1'b0; tag_set_dirty_en <= 1'b0; tag_inv_en <= 1'b0; tag_set_lock_en <= 1'b0;
        data_wr_en <= 1'b0;

        if (xbwu_valid && xbwu_ready) xbwu_valid <= 1'b0;
        if (xbnu_valid && xbnu_ready) xbnu_valid <= 1'b0;

        if (flush_req) flush_pending <= 1'b1;
        if (cache_inv && ctrl_state == CTRL_IDLE) begin ctrl_state <= CTRL_INV; scan_set <= 0; scan_way <= 0; end
        if (ctrl_state == CTRL_IDLE && flush_pending && inflight_cnt == 0) begin
            ctrl_state <= CTRL_FLUSH; scan_set <= 0; scan_way <= 0; flush_wait_b <= 0; flush_pending <= 1'b0;
        end

        if (ctrl_state == CTRL_FLUSH) begin
            if (!flush_wait_b) begin
                if (tag_query_valid && tag_query_dirty) begin
                    if (aw_ready) begin
                        aw_valid <= 1'b1; aw_id <= {MSHR_W{1'b1}};
                        aw_addr <= {tag_query_tag, scan_set, {OFFSET_BITS{1'b0}}};
                        aw_wdata <= data_rd_line; aw_wstrb <= {LINE_W/8{1'b1}}; aw_is_atomic <= 1'b0;
                        flush_wait_b <= 1'b1;
                    end
                end else begin
                    if (scan_way == 4'd15) begin scan_way <= 0; if (scan_set == 6'd63) begin ctrl_state <= CTRL_IDLE; flush_ack <= 1'b1; end else scan_set <= scan_set + 1'b1; end
                    else scan_way <= scan_way + 1'b1;
                end
            end else if (b_valid) begin
                tag_set_dirty_en <= 1'b1; tag_set_dirty_set <= scan_set; tag_set_dirty_way <= scan_way; tag_set_dirty_value <= 1'b0;
                flush_wait_b <= 1'b0;
                if (scan_way == 4'd15) begin scan_way <= 0; if (scan_set == 6'd63) begin ctrl_state <= CTRL_IDLE; flush_ack <= 1'b1; end else scan_set <= scan_set + 1'b1; end
                else scan_way <= scan_way + 1'b1;
            end
        end else if (ctrl_state == CTRL_INV) begin
            if (tag_query_valid && !line_inflight({tag_query_tag, scan_set})) begin
                tag_inv_en <= 1'b1; tag_inv_set <= scan_set; tag_inv_way <= scan_way;
            end
            if (scan_way == 4'd15) begin scan_way <= 0; if (scan_set == 6'd63) ctrl_state <= CTRL_IDLE; else scan_set <= scan_set + 1'b1; end
            else scan_way <= scan_way + 1'b1;
        end else begin
            if ((xbnd_valid && xbnd_ready) || ((!xbnd_valid) && xbwd_valid && xbwd_ready)) begin
                m_valid[tail_ptr] <= 1'b1; m_kind[tail_ptr] <= req_kind; m_calg[tail_ptr] <= req_calg;
                m_up_id[tail_ptr] <= req_sel_xbnd ? xbnd_id : xbwd_id;
                m_addr[tail_ptr] <= req_sel_xbnd ? xbnd_addr : xbwd_addr;
                m_line_addr[tail_ptr] <= req_line_addr;
                m_way[tail_ptr] <= tag_hit ? tag_hit_way : tag_victim_way;
                m_done[tail_ptr] <= 1'b0; m_resp[tail_ptr] <= 0;
                m_wstrb[tail_ptr] <= xbwd_wstrb;
                m_aop[tail_ptr] <= xbwd_atomic_op; m_asize[tail_ptr] <= xbwd_atomic_size; m_aarg[tail_ptr] <= xbwd_atomic_arg; m_acmp[tail_ptr] <= xbwd_atomic_cmp;
                m_aw_line[tail_ptr] <= xbwd_wdata;

                // calg.inv: 命中即无效（并释放lock）
                if (req_calg[CALG_INV] && tag_hit) begin
                    tag_inv_en <= 1'b1; tag_inv_set <= req_set; tag_inv_way <= tag_hit_way;
                end

                if (req_kind == REQ_READ) begin
                    if (!req_bypass_eff && tag_hit) begin
                        m_rdata[tail_ptr] <= data_rd_line;
                        m_done[tail_ptr] <= 1'b1;
                        m_need_ar[tail_ptr] <= 1'b0; m_wait_r[tail_ptr] <= 1'b0; m_need_aw[tail_ptr] <= 1'b0; m_wait_b[tail_ptr] <= 1'b0; m_need_fill[tail_ptr] <= 1'b0;
                        tag_access_en <= 1'b1; tag_access_set <= req_set; tag_access_way <= tag_hit_way;
                        if (req_calg[CALG_LOCK]) begin
                            tag_set_lock_en <= 1'b1; tag_set_lock_set <= req_set; tag_set_lock_way <= tag_hit_way; tag_set_lock_value <= 1'b1;
                        end
                    end else begin
                        m_need_ar[tail_ptr] <= 1'b1; m_wait_r[tail_ptr] <= 1'b1; m_need_aw[tail_ptr] <= 1'b0; m_wait_b[tail_ptr] <= 1'b0;
                        m_need_fill[tail_ptr] <= !req_bypass_eff;
                    end
                end else if (req_kind == REQ_WRITE) begin
                    if (req_bypass_eff || !tag_victim_found) begin
                        // 旁路写 / 无可替换路（可能都被lock）
                        m_need_ar[tail_ptr] <= 1'b0; m_wait_r[tail_ptr] <= 1'b0; m_need_aw[tail_ptr] <= 1'b1; m_wait_b[tail_ptr] <= 1'b1; m_need_fill[tail_ptr] <= 1'b0;
                    end else if (!tag_hit && req_calg[CALG_NOINIT] && is_full_strb(xbwd_wstrb)) begin
                        // no_init: miss时不重填（仅全写掩码）
                        m_need_ar[tail_ptr] <= 1'b0; m_wait_r[tail_ptr] <= 1'b0; m_need_aw[tail_ptr] <= 1'b1; m_wait_b[tail_ptr] <= 1'b1; m_need_fill[tail_ptr] <= 1'b0;
                    end else if (!tag_hit && req_calg[CALG_READBACK]) begin
                        // read_back: miss先读回再合并写
                        m_need_ar[tail_ptr] <= 1'b1; m_wait_r[tail_ptr] <= 1'b1; m_need_aw[tail_ptr] <= 1'b0; m_wait_b[tail_ptr] <= 1'b1; m_need_fill[tail_ptr] <= 1'b1;
                    end else begin
                        // 默认写分配
                        if (tag_hit) begin
                            data_wr_en <= 1'b1; data_wr_set <= req_set; data_wr_way <= tag_hit_way; data_wr_line <= xbwd_wdata; data_wr_strb <= xbwd_wstrb;
                            tag_set_dirty_en <= 1'b1; tag_set_dirty_set <= req_set; tag_set_dirty_way <= tag_hit_way; tag_set_dirty_value <= 1'b1;
                            tag_access_en <= 1'b1; tag_access_set <= req_set; tag_access_way <= tag_hit_way;
                            if (req_calg[CALG_LOCK]) begin tag_set_lock_en <= 1'b1; tag_set_lock_set <= req_set; tag_set_lock_way <= tag_hit_way; tag_set_lock_value <= 1'b1; end
                        end else begin
                            tag_fill_en <= 1'b1; tag_fill_set <= req_set; tag_fill_way <= tag_victim_way; tag_fill_tag <= req_tag; tag_fill_valid <= 1'b1; tag_fill_dirty <= 1'b1; tag_fill_lock <= req_calg[CALG_LOCK];
                            data_wr_en <= 1'b1; data_wr_set <= req_set; data_wr_way <= tag_victim_way; data_wr_line <= xbwd_wdata; data_wr_strb <= xbwd_wstrb;
                            tag_access_en <= 1'b1; tag_access_set <= req_set; tag_access_way <= tag_victim_way;
                        end
                        m_need_ar[tail_ptr] <= 1'b0; m_wait_r[tail_ptr] <= 1'b0; m_need_aw[tail_ptr] <= 1'b1; m_wait_b[tail_ptr] <= 1'b1; m_need_fill[tail_ptr] <= !tag_hit;
                    end
                end else begin
                    // atomic
                    if (req_bypass_eff || !tag_victim_found) begin
                        m_need_ar[tail_ptr] <= 1'b1; m_wait_r[tail_ptr] <= 1'b1; m_need_aw[tail_ptr] <= 1'b0; m_wait_b[tail_ptr] <= 1'b1; m_need_fill[tail_ptr] <= 1'b0;
                    end else if (tag_hit) begin
                        atom_line_i <= data_rd_line; atom_addr_i <= xbwd_addr; atom_op_i <= xbwd_atomic_op; atom_size_i <= xbwd_atomic_size; atom_arg_i <= xbwd_atomic_arg; atom_cmp_i <= xbwd_atomic_cmp;
                        m_aw_line[tail_ptr] <= atom_line_o;
                        data_wr_en <= 1'b1; data_wr_set <= req_set; data_wr_way <= tag_hit_way; data_wr_line <= atom_line_o; data_wr_strb <= {LINE_W/8{1'b1}};
                        tag_set_dirty_en <= 1'b1; tag_set_dirty_set <= req_set; tag_set_dirty_way <= tag_hit_way; tag_set_dirty_value <= 1'b1;
                        tag_access_en <= 1'b1; tag_access_set <= req_set; tag_access_way <= tag_hit_way;
                        if (req_calg[CALG_LOCK]) begin tag_set_lock_en <= 1'b1; tag_set_lock_set <= req_set; tag_set_lock_way <= tag_hit_way; tag_set_lock_value <= 1'b1; end
                        m_need_ar[tail_ptr] <= 1'b0; m_wait_r[tail_ptr] <= 1'b0; m_need_aw[tail_ptr] <= 1'b1; m_wait_b[tail_ptr] <= 1'b1; m_need_fill[tail_ptr] <= 1'b0;
                    end else begin
                        m_need_ar[tail_ptr] <= 1'b1; m_wait_r[tail_ptr] <= 1'b1; m_need_aw[tail_ptr] <= 1'b0; m_wait_b[tail_ptr] <= 1'b1; m_need_fill[tail_ptr] <= 1'b1;
                    end
                end

                tail_ptr <= tail_ptr + 1'b1;
                inflight_cnt <= inflight_cnt + 1'b1;
            end

            if (issue_ar_found && ar_ready) begin
                ar_valid <= 1'b1; ar_id <= issue_ar_idx; ar_addr <= m_addr[issue_ar_idx]; m_need_ar[issue_ar_idx] <= 1'b0;
            end
            if (issue_aw_found && aw_ready) begin
                aw_valid <= 1'b1; aw_id <= issue_aw_idx; aw_addr <= m_addr[issue_aw_idx]; aw_wdata <= m_aw_line[issue_aw_idx];
                aw_wstrb <= (m_kind[issue_aw_idx] == REQ_ATOMIC) ? {LINE_W/8{1'b1}} : m_wstrb[issue_aw_idx];
                aw_is_atomic <= (m_kind[issue_aw_idx] == REQ_ATOMIC); m_need_aw[issue_aw_idx] <= 1'b0;
            end

            if (r_valid && m_valid[r_id] && m_wait_r[r_id]) begin
                m_wait_r[r_id] <= 1'b0; m_resp[r_id] <= r_resp; m_rdata[r_id] <= r_data;

                if (m_kind[r_id] == REQ_READ) begin
                    if (m_need_fill[r_id] && tag_victim_found) begin
                        tag_fill_en <= 1'b1; tag_fill_set <= hash_set_from_addr(m_addr[r_id]); tag_fill_way <= m_way[r_id]; tag_fill_tag <= m_addr[r_id][ADDR_W-1:OFFSET_BITS+6];
                        tag_fill_valid <= 1'b1; tag_fill_dirty <= 1'b0; tag_fill_lock <= m_calg[r_id][CALG_LOCK];
                        data_wr_en <= 1'b1; data_wr_set <= hash_set_from_addr(m_addr[r_id]); data_wr_way <= m_way[r_id]; data_wr_line <= r_data; data_wr_strb <= {LINE_W/8{1'b1}};
                        tag_access_en <= 1'b1; tag_access_set <= hash_set_from_addr(m_addr[r_id]); tag_access_way <= m_way[r_id];
                    end
                    m_done[r_id] <= 1'b1;
                end else if (m_kind[r_id] == REQ_WRITE) begin
                    // read_back写：读回后merge并写下行
                    m_aw_line[r_id] <= apply_wstrb(r_data, m_aw_line[r_id], m_wstrb[r_id]);
                    if (m_need_fill[r_id]) begin
                        tag_fill_en <= 1'b1; tag_fill_set <= hash_set_from_addr(m_addr[r_id]); tag_fill_way <= m_way[r_id]; tag_fill_tag <= m_addr[r_id][ADDR_W-1:OFFSET_BITS+6];
                        tag_fill_valid <= 1'b1; tag_fill_dirty <= 1'b1; tag_fill_lock <= m_calg[r_id][CALG_LOCK];
                        data_wr_en <= 1'b1; data_wr_set <= hash_set_from_addr(m_addr[r_id]); data_wr_way <= m_way[r_id];
                        data_wr_line <= apply_wstrb(r_data, m_aw_line[r_id], m_wstrb[r_id]); data_wr_strb <= {LINE_W/8{1'b1}};
                        tag_access_en <= 1'b1; tag_access_set <= hash_set_from_addr(m_addr[r_id]); tag_access_way <= m_way[r_id];
                    end
                    m_need_aw[r_id] <= 1'b1;
                end else begin
                    atom_line_i <= r_data; atom_addr_i <= m_addr[r_id]; atom_op_i <= m_aop[r_id]; atom_size_i <= m_asize[r_id]; atom_arg_i <= m_aarg[r_id]; atom_cmp_i <= m_acmp[r_id];
                    m_aw_line[r_id] <= atom_line_o; m_need_aw[r_id] <= 1'b1;
                    if (m_need_fill[r_id]) begin
                        tag_fill_en <= 1'b1; tag_fill_set <= hash_set_from_addr(m_addr[r_id]); tag_fill_way <= m_way[r_id]; tag_fill_tag <= m_addr[r_id][ADDR_W-1:OFFSET_BITS+6];
                        tag_fill_valid <= 1'b1; tag_fill_dirty <= 1'b1; tag_fill_lock <= m_calg[r_id][CALG_LOCK];
                        data_wr_en <= 1'b1; data_wr_set <= hash_set_from_addr(m_addr[r_id]); data_wr_way <= m_way[r_id]; data_wr_line <= atom_line_o; data_wr_strb <= {LINE_W/8{1'b1}};
                        tag_access_en <= 1'b1; tag_access_set <= hash_set_from_addr(m_addr[r_id]); tag_access_way <= m_way[r_id];
                    end
                end
            end

            if (b_valid && m_valid[b_id] && m_wait_b[b_id]) begin
                m_wait_b[b_id] <= 1'b0; m_resp[b_id] <= b_resp; m_done[b_id] <= 1'b1;
            end

            if (m_valid[head_ptr] && m_done[head_ptr]) begin
                if (m_kind[head_ptr] == REQ_READ) begin
                    if (!xbwu_valid || xbwu_ready) begin
                        xbwu_valid <= 1'b1; xbwu_id <= m_up_id[head_ptr]; xbwu_rdata <= m_rdata[head_ptr]; xbwu_resp <= m_resp[head_ptr];
                        m_valid[head_ptr] <= 1'b0; head_ptr <= head_ptr + 1'b1; inflight_cnt <= inflight_cnt - 1'b1;
                    end
                end else begin
                    if (!xbnu_valid || xbnu_ready) begin
                        xbnu_valid <= 1'b1; xbnu_id <= m_up_id[head_ptr]; xbnu_resp <= m_resp[head_ptr];
                        m_valid[head_ptr] <= 1'b0; head_ptr <= head_ptr + 1'b1; inflight_cnt <= inflight_cnt - 1'b1;
                    end
                end
            end
        end
    end
end

endmodule
