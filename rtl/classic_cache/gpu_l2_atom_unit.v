module gpu_l2_atom_unit #(
    parameter LINE_W = 1024,
    parameter ADDR_W = 40
) (
    input  [LINE_W-1:0] line_i,
    input  [ADDR_W-1:0] addr_i,
    input  [1:0]        op_i,      // 00:add,01:sub,10:swap,11:cmpswap
    input               size64_i,  // 0:b32,1:b64
    input  [63:0]       arg_i,
    input  [63:0]       cmp_i,
    output [LINE_W-1:0] line_o
);

reg [LINE_W-1:0] line_n;
reg [63:0] old64, new64;
reg [31:0] old32, new32;
reg [3:0] idx64;
reg [4:0] idx32;

always @(*) begin
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
end

assign line_o = line_n;

endmodule
