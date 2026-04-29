`timescale 1ns/1ps
module tb_gpu_l2_freelist_p22;
  localparam DATA_SLOTS = 8;
  localparam PTR_W = $clog2(DATA_SLOTS);

  reg clk=0; always #5 clk=~clk;
  reg rst_n=0;
  reg alloc_req;
  reg reset_all;
  wire alloc_gnt;
  wire [PTR_W-1:0] alloc_ptr;
  reg free_req;
  reg [PTR_W-1:0] free_ptr;
  wire [PTR_W:0] free_count;

  gpu_l2_freelist #(.DATA_SLOTS(DATA_SLOTS)) dut (
    .clk(clk), .rst_n(rst_n),
    .reset_all(reset_all),
    .alloc_req(alloc_req), .alloc_gnt(alloc_gnt), .alloc_ptr(alloc_ptr),
    .free_req(free_req), .free_ptr(free_ptr), .free_count(free_count)
  );

  initial begin
    alloc_req = 0; free_req = 0; free_ptr = 0; reset_all = 0;
    repeat (3) @(posedge clk);
    rst_n = 1;

    // Pure alloc: count decrements
    @(posedge clk); alloc_req = 1; free_req = 0;
    @(posedge clk); alloc_req = 0;
    if (free_count != DATA_SLOTS-1) begin
      $display("ERROR alloc decrement failed count=%0d", free_count);
      $finish_and_return(1);
    end

    // alloc + free in same cycle: count should remain stable
    @(posedge clk); alloc_req = 1; free_req = 1; free_ptr = alloc_ptr;
    @(posedge clk); alloc_req = 0; free_req = 0;
    if (free_count != DATA_SLOTS-1) begin
      $display("ERROR alloc/free same-cycle imbalance count=%0d", free_count);
      $finish_and_return(1);
    end

    // Pure free: count increments
    @(posedge clk); free_req = 1; free_ptr = 3;
    @(posedge clk); free_req = 0;
    if (free_count != DATA_SLOTS) begin
      $display("ERROR free increment failed count=%0d", free_count);
      $finish_and_return(1);
    end

    $display("FREELIST_P22_PASS count=%0d", free_count);
    $finish;
  end
endmodule
