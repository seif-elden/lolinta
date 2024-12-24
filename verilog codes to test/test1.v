module test_casel(input a, b, c, output y);

reg [3:0] r1, r2; // Declared registers
reg [4:0] result; // Potential overflow
assign y = b | 1'bx; // X propagation
initial begin
    r2 = 0;
end
always @(a) begin
    r1 = r2 + b; // Potential arithmetic overflow
end

endmodule