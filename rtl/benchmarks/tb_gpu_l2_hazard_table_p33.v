`timescale 1ns/1ps
module tb_gpu_l2_hazard_table_p33;
  localparam ADDR_W = 40;
  localparam OFFSET_BITS = 7;
  localparam MSHR_NUM = 8;
  localparam HAZ_ENTRIES = 4;
  localparam HAZ_WAYS = 2;

  reg clk = 0;
  always #5 clk = ~clk;
  reg rst_n = 0;

  reg push_req;
  reg [ADDR_W-OFFSET_BITS-1:0] push_line_addr;
  reg [$clog2(MSHR_NUM)-1:0] push_tok;
  wire push_accept;
  wire push_blocked;

  reg retire_req;
  reg [$clog2(MSHR_NUM)-1:0] retire_tok;
  wire wake_valid;
  wire [$clog2(MSHR_NUM)-1:0] wake_tok;

  gpu_l2_hazard_table #(
    .ADDR_W(ADDR_W), .OFFSET_BITS(OFFSET_BITS), .MSHR_NUM(MSHR_NUM),
    .HAZ_ENTRIES(HAZ_ENTRIES), .HAZ_WAYS(HAZ_WAYS)
  ) dut (
    .clk(clk), .rst_n(rst_n),
    .push_req(push_req), .push_line_addr(push_line_addr), .push_tok(push_tok),
    .push_accept(push_accept), .push_blocked(push_blocked),
    .retire_req(retire_req), .retire_tok(retire_tok),
    .wake_valid(wake_valid), .wake_tok(wake_tok)
  );

  task do_push;
    input [2:0] tok;
    input [32:0] line_addr;
    input exp_blocked;
    begin
      @(posedge clk);
      push_req <= 1'b1;
      push_tok <= tok;
      push_line_addr <= line_addr;
      #1;
      if (!push_accept) begin
        $display("ERROR push not accepted tok=%0d", tok);
        $finish_and_return(1);
      end
      if (push_blocked !== exp_blocked) begin
        $display("ERROR push_blocked mismatch tok=%0d exp=%0d got=%0d", tok, exp_blocked, push_blocked);
        $finish_and_return(1);
      end
      @(posedge clk);
      push_req <= 1'b0;
    end
  endtask

  task do_retire;
    input [2:0] tok;
    input exp_wake_valid;
    input [2:0] exp_wake_tok;
    begin
      @(posedge clk);
      retire_req <= 1'b1;
      retire_tok <= tok;
      @(posedge clk);
      retire_req <= 1'b0;
      #1;
      if (wake_valid !== exp_wake_valid) begin
        $display("ERROR wake_valid mismatch retire=%0d exp=%0d got=%0d", tok, exp_wake_valid, wake_valid);
        $finish_and_return(1);
      end
      if (exp_wake_valid && (wake_tok !== exp_wake_tok)) begin
        $display("ERROR wake_tok mismatch retire=%0d exp=%0d got=%0d", tok, exp_wake_tok, wake_tok);
        $finish_and_return(1);
      end
    end
  endtask

  initial begin
    push_req = 0;
    push_line_addr = 0;
    push_tok = 0;
    retire_req = 0;
    retire_tok = 0;

    repeat (4) @(posedge clk);
    rst_n = 1;

    // All addresses below hash to bucket-0 with HAZ_BUCKETS=2.
    do_push(3'd0, 33'd0, 1'b0); // first in bucket
    do_push(3'd1, 33'd2, 1'b0); // second way in same bucket, still unblocked
    do_push(3'd2, 33'd4, 1'b1); // bucket saturated -> conservative blocked alias
    do_push(3'd3, 33'd6, 1'b1); // chained behind alias queue

    // Middle delete (retire tok2 while queue is tok0->tok2->tok3): no wake.
    do_retire(3'd2, 1'b0, 3'd0);

    // Head retire should wake tok3 (tok2 already removed).
    do_retire(3'd0, 1'b1, 3'd3);

    // Independent slot retires with no wake.
    do_retire(3'd1, 1'b0, 3'd0);
    do_retire(3'd3, 1'b0, 3'd0);

    $display("HAZARD_P33_PASS");
    $finish;
  end
endmodule
