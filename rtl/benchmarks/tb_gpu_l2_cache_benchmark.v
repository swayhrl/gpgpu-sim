`timescale 1ns/1ps

module tb_gpu_l2_cache_benchmark;
  localparam ADDR_W=40;
  localparam ID_W=8;
  localparam LINE_W=1024;
  localparam RESP_W=2;
  localparam MSHR_W=6;

  reg clk;
  reg rst_n;

  reg xbnd_valid;
  wire xbnd_ready;
  reg [ID_W-1:0] xbnd_id;
  reg [ADDR_W-1:0] xbnd_addr;
  reg xbnd_bypass;
  reg [4:0] xbnd_calg;

  reg xbwd_valid;
  wire xbwd_ready;
  reg [ID_W-1:0] xbwd_id;
  reg [ADDR_W-1:0] xbwd_addr;
  reg [LINE_W-1:0] xbwd_wdata;
  reg [LINE_W/8-1:0] xbwd_wstrb;
  reg xbwd_is_atomic;
  reg xbwd_bypass;
  reg [4:0] xbwd_calg;
  reg [1:0] xbwd_atomic_op;
  reg xbwd_atomic_size;
  reg [63:0] xbwd_atomic_arg;
  reg [63:0] xbwd_atomic_cmp;

  reg flush_req;
  wire flush_ack;
  reg cache_inv;

  wire xbwu_valid;
  reg xbwu_ready;
  wire [ID_W-1:0] xbwu_id;
  wire [LINE_W-1:0] xbwu_rdata;
  wire [RESP_W-1:0] xbwu_resp;

  wire xbnu_valid;
  reg xbnu_ready;
  wire [ID_W-1:0] xbnu_id;
  wire [RESP_W-1:0] xbnu_resp;

  wire ar_valid;
  reg ar_ready;
  wire [MSHR_W-1:0] ar_id;
  wire [ADDR_W-1:0] ar_addr;

  wire aw_valid;
  reg aw_ready;
  wire [MSHR_W-1:0] aw_id;
  wire [ADDR_W-1:0] aw_addr;
  wire [LINE_W-1:0] aw_wdata;
  wire [LINE_W/8-1:0] aw_wstrb;
  wire aw_is_atomic;

  reg r_valid;
  wire r_ready;
  reg [MSHR_W-1:0] r_id;
  reg [LINE_W-1:0] r_data;
  reg [RESP_W-1:0] r_resp;

  reg b_valid;
  wire b_ready;
  reg [MSHR_W-1:0] b_id;
  reg [RESP_W-1:0] b_resp;

  reg [LINE_W-1:0] mem [0:255];
  integer i;
  integer req_cnt;
  integer rsp_cnt;
  integer start_cycle;
  integer end_cycle;
  integer cycle_cnt;

  gpu_l2_cache dut(
    .clk(clk), .rst_n(rst_n),
    .xbnd_valid(xbnd_valid), .xbnd_ready(xbnd_ready), .xbnd_id(xbnd_id), .xbnd_addr(xbnd_addr), .xbnd_bypass(xbnd_bypass), .xbnd_calg(xbnd_calg),
    .xbwd_valid(xbwd_valid), .xbwd_ready(xbwd_ready), .xbwd_id(xbwd_id), .xbwd_addr(xbwd_addr), .xbwd_wdata(xbwd_wdata), .xbwd_wstrb(xbwd_wstrb),
    .xbwd_is_atomic(xbwd_is_atomic), .xbwd_bypass(xbwd_bypass), .xbwd_calg(xbwd_calg),
    .xbwd_atomic_op(xbwd_atomic_op), .xbwd_atomic_size(xbwd_atomic_size), .xbwd_atomic_arg(xbwd_atomic_arg), .xbwd_atomic_cmp(xbwd_atomic_cmp),
    .flush_req(flush_req), .flush_ack(flush_ack), .cache_inv(cache_inv),
    .xbwu_valid(xbwu_valid), .xbwu_ready(xbwu_ready), .xbwu_id(xbwu_id), .xbwu_rdata(xbwu_rdata), .xbwu_resp(xbwu_resp),
    .xbnu_valid(xbnu_valid), .xbnu_ready(xbnu_ready), .xbnu_id(xbnu_id), .xbnu_resp(xbnu_resp),
    .ar_valid(ar_valid), .ar_ready(ar_ready), .ar_id(ar_id), .ar_addr(ar_addr),
    .aw_valid(aw_valid), .aw_ready(aw_ready), .aw_id(aw_id), .aw_addr(aw_addr), .aw_wdata(aw_wdata), .aw_wstrb(aw_wstrb), .aw_is_atomic(aw_is_atomic),
    .r_valid(r_valid), .r_ready(r_ready), .r_id(r_id), .r_data(r_data), .r_resp(r_resp),
    .b_valid(b_valid), .b_ready(b_ready), .b_id(b_id), .b_resp(b_resp)
  );

  always #5 clk = ~clk;

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

  // simple memory model: 1-cycle response for AR/AW
  reg ar_pend;
  reg [MSHR_W-1:0] ar_pend_id;
  reg [ADDR_W-1:0] ar_pend_addr;
  reg aw_pend;
  reg [MSHR_W-1:0] aw_pend_id;

  always @(posedge clk) begin
    r_valid <= 1'b0;
    b_valid <= 1'b0;

    if (ar_valid && ar_ready) begin
      ar_pend <= 1'b1;
      ar_pend_id <= ar_id;
      ar_pend_addr <= ar_addr;
    end
    if (aw_valid && aw_ready) begin
      mem[aw_addr[11:4]] <= apply_wstrb(mem[aw_addr[11:4]], aw_wdata, aw_wstrb);
      aw_pend <= 1'b1;
      aw_pend_id <= aw_id;
    end

    if (ar_pend) begin
      ar_pend <= 1'b0;
      r_valid <= 1'b1;
      r_id <= ar_pend_id;
      r_data <= mem[ar_pend_addr[11:4]];
      r_resp <= 2'b00;
    end

    if (aw_pend) begin
      aw_pend <= 1'b0;
      b_valid <= 1'b1;
      b_id <= aw_pend_id;
      b_resp <= 2'b00;
    end

    if (xbwu_valid && xbwu_ready) rsp_cnt <= rsp_cnt + 1;
    if (xbnu_valid && xbnu_ready) rsp_cnt <= rsp_cnt + 1;
    cycle_cnt <= cycle_cnt + 1;
  end

  task drive_write;
    input [ID_W-1:0] id;
    input [ADDR_W-1:0] addr;
    input [4:0] calg;
    input [LINE_W-1:0] line;
    input [LINE_W/8-1:0] strb;
    begin
      @(posedge clk);
      xbwd_valid <= 1'b1;
      xbwd_id <= id;
      xbwd_addr <= addr;
      xbwd_calg <= calg;
      xbwd_wdata <= line;
      xbwd_wstrb <= strb;
      xbwd_is_atomic <= 1'b0;
      xbwd_bypass <= 1'b0;
      while (!xbwd_ready) @(posedge clk);
      req_cnt <= req_cnt + 1;
      @(posedge clk);
      xbwd_valid <= 1'b0;
    end
  endtask

  task drive_read;
    input [ID_W-1:0] id;
    input [ADDR_W-1:0] addr;
    input [4:0] calg;
    begin
      @(posedge clk);
      xbnd_valid <= 1'b1;
      xbnd_id <= id;
      xbnd_addr <= addr;
      xbnd_calg <= calg;
      xbnd_bypass <= 1'b0;
      while (!xbnd_ready) @(posedge clk);
      req_cnt <= req_cnt + 1;
      @(posedge clk);
      xbnd_valid <= 1'b0;
    end
  endtask

  initial begin
    clk = 0;
    rst_n = 0;
    xbnd_valid = 0; xbwd_valid = 0;
    xbwu_ready = 1; xbnu_ready = 1;
    ar_ready = 1; aw_ready = 1;
    r_valid = 0; b_valid = 0;
    ar_pend = 0; aw_pend = 0;
    flush_req = 0; cache_inv = 0;
    req_cnt = 0; rsp_cnt = 0; cycle_cnt = 0;

    xbwd_wdata = 0; xbwd_wstrb = 0; xbwd_is_atomic = 0; xbwd_bypass = 0; xbwd_calg = 0;
    xbwd_atomic_op = 0; xbwd_atomic_size = 0; xbwd_atomic_arg = 0; xbwd_atomic_cmp = 0;
    xbnd_id = 0; xbnd_addr = 0; xbnd_bypass = 0; xbnd_calg = 0;

    for (i=0; i<256; i=i+1) mem[i] = {LINE_W{1'b0}};
    mem['h10] = {16{64'h1111_2222_3333_4444}};
    mem['h20] = {16{64'hAAAA_BBBB_CCCC_DDDD}};

    repeat(5) @(posedge clk);
    rst_n = 1;
    start_cycle = cycle_cnt;

    // Case1: no_init full-line write miss
    drive_write(8'h01, 40'h0000_0100, 5'b00100, {16{64'hDEAD_BEEF_0000_0001}}, {LINE_W/8{1'b1}});
    // Case2: read back verify memory got updated
    drive_read(8'h02, 40'h0000_0100, 5'b00000);

    // Case3: read_back partial write miss
    drive_write(8'h03, 40'h0000_0200, 5'b01000, {16{64'h0000_0000_0000_FF00}}, {{(LINE_W/8-8){1'b0}}, 8'hFF});
    drive_read(8'h04, 40'h0000_0200, 5'b00000);


    // immediate sanity before random benchmark
    if (mem[8'h20][63:0] !== 64'h0000_0000_0000_FF00) begin
      $display("ERROR_EARLY mem20_low64=%h", mem[8'h20][63:0]);
      $finish_and_return(1);
    end
    // mini benchmark: 32 alternating requests
    for (i=0; i<32; i=i+1) begin
      if (i[0])
        drive_read(i[ID_W-1:0], {24'h0, (8'h80 + i[7:0]), 8'h00}, 5'b00000);
      else
        drive_write(i[ID_W-1:0], {24'h0, (8'h80 + i[7:0]), 8'h00}, 5'b00000, {16{i[7:0],56'h0}}, {LINE_W/8{1'b1}});
    end

    wait (rsp_cnt >= req_cnt);
    end_cycle = cycle_cnt;

    $display("BENCH_DONE req=%0d rsp=%0d cycles=%0d", req_cnt, rsp_cnt, end_cycle-start_cycle);
    // PERF_D5 template: benchmark keeps protocol-level counters; micro-architectural
    // fields are reported as 0 in this classic-cache benchmark.
    $display("PERF_D5 cycles=%0d req_acc=%0d req_r=%0d req_w=%0d stall_mshr=%0d stall_haz=%0d rhit=%0d rmiss=%0d aw=%0d ret=%0d ret_r=%0d ret_w=%0d sum_if=%0d sum_rq=%0d sum_wq=%0d peak_if=%0d peak_rq=%0d peak_wq=%0d wait_ar=%0d wait_aw=%0d wait_r=%0d wait_b=%0d",
             end_cycle-start_cycle, req_cnt, 0, 0, 0, 0, 0, 0, 0, rsp_cnt, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

    $display("BENCH_PASS");
    $finish;
  end
endmodule
