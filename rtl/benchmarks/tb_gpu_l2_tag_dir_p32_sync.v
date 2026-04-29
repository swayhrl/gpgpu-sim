`timescale 1ns/1ps
module tb_gpu_l2_tag_dir_p32_sync;
  localparam ADDR_W=40; localparam SETS=4; localparam WAYS=2; localparam OFFSET_BITS=7; localparam DATA_SLOTS=16; localparam EPOCH_W=2;
  localparam SET_W=$clog2(SETS); localparam WAY_W=$clog2(WAYS); localparam TAG_W=ADDR_W-OFFSET_BITS-SET_W; localparam PTR_W=$clog2(DATA_SLOTS);
  localparam DIR_W=TAG_W+PTR_W+EPOCH_W+9;

  reg clk=0; always #5 clk=~clk;
  reg rst_n=0;
  reg [SET_W-1:0] lookup_set; reg [TAG_W-1:0] lookup_tag;
  wire hit; wire [WAY_W-1:0] hit_way; wire victim_found; wire [WAY_W-1:0] victim_way;
  reg [SET_W-1:0] rd_set; reg [WAY_W-1:0] rd_way; wire [DIR_W-1:0] rd_entry;
  reg wr_en; reg [SET_W-1:0] wr_set; reg [WAY_W-1:0] wr_way; reg [DIR_W-1:0] wr_entry;
  reg access_en; reg [SET_W-1:0] access_set; reg [WAY_W-1:0] access_way; reg clear_all;

  gpu_l2_tag_dir #(.ADDR_W(ADDR_W),.SETS(SETS),.WAYS(WAYS),.OFFSET_BITS(OFFSET_BITS),.DATA_SLOTS(DATA_SLOTS),.EPOCH_W(EPOCH_W),.RD_SYNC(1)) dut(
    .clk(clk),.rst_n(rst_n),.lookup_set(lookup_set),.lookup_tag(lookup_tag),.hit(hit),.hit_way(hit_way),.victim_found(victim_found),.victim_way(victim_way),
    .rd_set(rd_set),.rd_way(rd_way),.rd_entry(rd_entry),.wr_en(wr_en),.wr_set(wr_set),.wr_way(wr_way),.wr_entry(wr_entry),
    .access_en(access_en),.access_set(access_set),.access_way(access_way),.clear_all(clear_all)
  );

  initial begin
    lookup_set=0;lookup_tag=0;rd_set=0;rd_way=0;wr_en=0;wr_set=0;wr_way=0;wr_entry=0;access_en=0;access_set=0;access_way=0;clear_all=0;
    repeat(3) @(posedge clk); rst_n=1;

    // write a valid entry
    @(posedge clk);
    wr_en=1; wr_set=1; wr_way=0;
    wr_entry={1'b1,1'b0,1'b0,2'b00,{EPOCH_W{1'b0}},{TAG_W{1'b1}},{PTR_W{1'b1}},4'd0};
    rd_set=1; rd_way=0;
    @(posedge clk); wr_en=0;

    // RD_SYNC=1: rd_entry reflects updated content after clocked capture
    @(posedge clk);
    if (!rd_entry[DIR_W-1]) begin
      $display("ERROR rd_sync path did not return valid entry");
      $finish_and_return(1);
    end

    $display("TAGDIR_P32_SYNC_PASS");
    $finish;
  end
endmodule
