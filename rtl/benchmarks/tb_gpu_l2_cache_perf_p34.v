`timescale 1ns/1ps
module tb_gpu_l2_cache_perf_p34;
  localparam ADDR_W=40; localparam ID_W=8; localparam LINE_W=1024; localparam RESP_W=2; localparam TOK_W=6;
  reg clk=0; always #5 clk=~clk;
  reg rst_n=0;

  reg xbnd_valid; wire xbnd_ready; reg [ID_W-1:0] xbnd_id; reg [ADDR_W-1:0] xbnd_addr; reg xbnd_bypass; reg [4:0] xbnd_calg;
  reg xbwd_valid; wire xbwd_ready; reg [ID_W-1:0] xbwd_id; reg [ADDR_W-1:0] xbwd_addr; reg [LINE_W-1:0] xbwd_wdata; reg [LINE_W/8-1:0] xbwd_wstrb;
  reg xbwd_is_atomic; reg xbwd_bypass; reg [4:0] xbwd_calg; reg [1:0] xbwd_atomic_op; reg xbwd_atomic_size; reg [63:0] xbwd_atomic_arg; reg [63:0] xbwd_atomic_cmp;
  reg flush_req; wire flush_ack; reg cache_inv;
  wire xbwu_valid; reg xbwu_ready; wire [ID_W-1:0] xbwu_id; wire [LINE_W-1:0] xbwu_rdata; wire [RESP_W-1:0] xbwu_resp;
  wire xbnu_valid; reg xbnu_ready; wire [ID_W-1:0] xbnu_id; wire [RESP_W-1:0] xbnu_resp;
  wire ar_valid; reg ar_ready; wire [TOK_W-1:0] ar_id; wire [ADDR_W-1:0] ar_addr;
  wire aw_valid; reg aw_ready; wire [TOK_W-1:0] aw_id; wire [ADDR_W-1:0] aw_addr; wire [LINE_W-1:0] aw_wdata; wire [LINE_W/8-1:0] aw_wstrb; wire aw_is_atomic;
  reg r_valid; wire r_ready; reg [TOK_W-1:0] r_id; reg [LINE_W-1:0] r_data; reg [RESP_W-1:0] r_resp;
  reg b_valid; wire b_ready; reg [TOK_W-1:0] b_id; reg [RESP_W-1:0] b_resp;

  reg pending_rv;
  reg [TOK_W-1:0] pending_rid;

  gpu_l2_cache_renamed dut(
    .clk(clk),.rst_n(rst_n),
    .xbnd_valid(xbnd_valid),.xbnd_ready(xbnd_ready),.xbnd_id(xbnd_id),.xbnd_addr(xbnd_addr),.xbnd_bypass(xbnd_bypass),.xbnd_calg(xbnd_calg),
    .xbwd_valid(xbwd_valid),.xbwd_ready(xbwd_ready),.xbwd_id(xbwd_id),.xbwd_addr(xbwd_addr),.xbwd_wdata(xbwd_wdata),.xbwd_wstrb(xbwd_wstrb),.xbwd_is_atomic(xbwd_is_atomic),.xbwd_bypass(xbwd_bypass),.xbwd_calg(xbwd_calg),
    .xbwd_atomic_op(xbwd_atomic_op),.xbwd_atomic_size(xbwd_atomic_size),.xbwd_atomic_arg(xbwd_atomic_arg),.xbwd_atomic_cmp(xbwd_atomic_cmp),
    .flush_req(flush_req),.flush_ack(flush_ack),.cache_inv(cache_inv),
    .xbwu_valid(xbwu_valid),.xbwu_ready(xbwu_ready),.xbwu_id(xbwu_id),.xbwu_rdata(xbwu_rdata),.xbwu_resp(xbwu_resp),
    .xbnu_valid(xbnu_valid),.xbnu_ready(xbnu_ready),.xbnu_id(xbnu_id),.xbnu_resp(xbnu_resp),
    .ar_valid(ar_valid),.ar_ready(ar_ready),.ar_id(ar_id),.ar_addr(ar_addr),
    .aw_valid(aw_valid),.aw_ready(aw_ready),.aw_id(aw_id),.aw_addr(aw_addr),.aw_wdata(aw_wdata),.aw_wstrb(aw_wstrb),.aw_is_atomic(aw_is_atomic),
    .r_valid(r_valid),.r_ready(r_ready),.r_id(r_id),.r_data(r_data),.r_resp(r_resp),
    .b_valid(b_valid),.b_ready(b_ready),.b_id(b_id),.b_resp(b_resp)
  );

  always @(posedge clk) begin
    b_valid <= 0;
    r_valid <= pending_rv;
    if (pending_rv) begin
      r_id <= pending_rid;
      r_resp <= 0;
      r_data <= {16{64'hDDDD_0000_0000_0000 | pending_rid}};
      pending_rv <= 0;
    end

    if (ar_valid && ar_ready) begin
      pending_rv <= 1;
      pending_rid <= ar_id;
    end

    if (aw_valid && aw_ready) begin
      b_valid <= 1;
      b_id <= aw_id;
      b_resp <= 0;
    end
  end

  task send_read;
    input [7:0] id;
    input [39:0] addr;
    begin
      @(posedge clk);
      xbnd_valid<=1; xbnd_id<=id; xbnd_addr<=addr;
      while(!xbnd_ready) @(posedge clk);
      @(posedge clk); xbnd_valid<=0;
    end
  endtask

  task send_write;
    input [7:0] id;
    input [39:0] addr;
    begin
      @(posedge clk);
      xbwd_valid<=1; xbwd_id<=id; xbwd_addr<=addr;
      xbwd_wdata<={16{64'hA5A5_0000_0000_0000|id}}; xbwd_wstrb<={LINE_W/8{1'b1}};
      while(!xbwd_ready) @(posedge clk);
      @(posedge clk); xbwd_valid<=0;
    end
  endtask

  initial begin
    xbnd_valid=0;xbwd_valid=0;xbwu_ready=1;xbnu_ready=1;ar_ready=1;aw_ready=1;r_valid=0;b_valid=0;pending_rv=0;pending_rid=0;
    xbwd_is_atomic=0;xbwd_bypass=0;xbwd_calg=0;xbwd_atomic_op=0;xbwd_atomic_size=0;xbwd_atomic_arg=0;xbwd_atomic_cmp=0;
    xbnd_id=0;xbnd_addr=0;xbnd_bypass=0;xbnd_calg=0;flush_req=0;cache_inv=0;r_id=0;r_data=0;r_resp=0;b_id=0;b_resp=0;

    repeat(5) @(posedge clk); rst_n=1;

    // 1) miss then hit on same line for read hit/miss observability
    send_read(8'h10,40'h0000_9000);
    wait (xbwu_valid && xbwu_ready);
    send_read(8'h11,40'h0000_9000);
    wait (xbwu_valid && xbwu_ready);

    // 2) same-line write burst to trigger hazard blocking + wake sequencing
    send_write(8'h20,40'h0000_A000);
    send_write(8'h21,40'h0000_A000);
    send_write(8'h22,40'h0000_A000);

    repeat(30) @(posedge clk);

    if (dut.perf_req_accept < 5) begin
      $display("ERROR perf_req_accept too small: %0d", dut.perf_req_accept);
      $finish_and_return(1);
    end
    if (dut.perf_req_blocked == 0) begin
      $display("ERROR expected blocked requests");
      $finish_and_return(1);
    end
    if (dut.perf_haz_wake == 0) begin
      $display("ERROR expected hazard wake events");
      $finish_and_return(1);
    end
    if (dut.perf_read_miss_issue == 0 || dut.perf_read_hit_issue == 0) begin
      $display("ERROR expected both read miss/hit issue counters miss=%0d hit=%0d", dut.perf_read_miss_issue, dut.perf_read_hit_issue);
      $finish_and_return(1);
    end
    if (dut.perf_aw_issue == 0 || dut.perf_retire == 0) begin
      $display("ERROR expected aw/retire activity aw=%0d retire=%0d", dut.perf_aw_issue, dut.perf_retire);
      $finish_and_return(1);
    end
    if (dut.perf_sum_inflight < dut.perf_peak_inflight) begin
      $display("ERROR invalid inflight stats sum=%0d peak=%0d", dut.perf_sum_inflight, dut.perf_peak_inflight);
      $finish_and_return(1);
    end

    $display("PERF_P34_PASS cycles=%0d req=%0d blocked=%0d wake=%0d rhit=%0d rmiss=%0d aw=%0d retire=%0d",
      dut.perf_cycles, dut.perf_req_accept, dut.perf_req_blocked, dut.perf_haz_wake,
      dut.perf_read_hit_issue, dut.perf_read_miss_issue, dut.perf_aw_issue, dut.perf_retire);
    $display("PERF_D6 cycles=%0d req_acc=%0d req_r=%0d req_w=%0d stall_mshr=%0d stall_haz=%0d rhit=%0d rmiss=%0d aw=%0d ret=%0d ret_r=%0d ret_w=%0d sum_if=%0d sum_rq=%0d sum_wq=%0d peak_if=%0d peak_rq=%0d peak_wq=%0d wait_ar=%0d wait_aw=%0d wait_r=%0d wait_b=%0d",
      dut.perf_cycles, dut.perf_req_accept, dut.perf_req_read_accept, dut.perf_req_write_accept,
      dut.perf_req_stall_mshr_full, dut.perf_req_stall_hazard,
      dut.perf_read_hit_issue, dut.perf_read_miss_issue, dut.perf_aw_issue, dut.perf_retire,
      dut.perf_retire_read, dut.perf_retire_write,
      dut.perf_sum_inflight, dut.perf_sum_rq_depth, dut.perf_sum_wq_depth,
      dut.perf_peak_inflight, dut.perf_peak_rq_depth, dut.perf_peak_wq_depth,
      dut.perf_cyc_wait_ar_ready, dut.perf_cyc_wait_aw_ready, dut.perf_cyc_wait_r_valid, dut.perf_cyc_wait_b_valid);
    $finish;
  end
endmodule
