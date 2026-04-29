`timescale 1ns/1ps
module tb_gpu_l2_cache_renamed_p24_atomic_sem;
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

  reg saw_atomic_aw;

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
    r_valid <= 0; b_valid <= 0;
    if (ar_valid && ar_ready) begin
      r_valid <= 1; r_id <= ar_id; r_resp <= 0;
      r_data <= {LINE_W{1'b0}};
      r_data[31:0] <= 32'd5;
    end
    if (aw_valid && aw_ready) begin
      b_valid <= 1; b_id <= aw_id; b_resp <= 0;
      if (aw_is_atomic) begin
        saw_atomic_aw <= 1'b1;
        if (aw_wdata[31:0] != 32'd8) begin
          $display("ERROR atomic aw_wdata mismatch got=%0d", aw_wdata[31:0]);
          $finish_and_return(1);
        end
      end
    end
  end

  initial begin
    xbnd_valid=0;xbwd_valid=0;xbwu_ready=1;xbnu_ready=1;ar_ready=1;aw_ready=1;r_valid=0;b_valid=0;
    xbwd_is_atomic=0;xbwd_bypass=0;xbwd_calg=0;xbwd_atomic_op=0;xbwd_atomic_size=0;xbwd_atomic_arg=0;xbwd_atomic_cmp=0;
    xbnd_id=0;xbnd_addr=0;xbnd_bypass=0;xbnd_calg=0;flush_req=0;cache_inv=0;saw_atomic_aw=0;

    repeat(5) @(posedge clk); rst_n=1;
    @(posedge clk);
    xbwd_valid=1; xbwd_id=8'h61; xbwd_addr=40'h0000_9900; xbwd_wdata=0; xbwd_wstrb={LINE_W/8{1'b1}};
    xbwd_is_atomic=1; xbwd_atomic_op=2'b00; xbwd_atomic_size=0; xbwd_atomic_arg=32'd3; xbwd_atomic_cmp=0;
    while(!xbwd_ready) @(posedge clk);
    @(posedge clk); xbwd_valid=0; xbwd_is_atomic=0;

    repeat(40) @(posedge clk);
    if (!saw_atomic_aw) begin
      $display("ERROR atomic AW never observed");
      $finish_and_return(1);
    end
    $display("RENAMED_P24_ATOMIC_SEM_PASS");
    $finish;
  end
endmodule
