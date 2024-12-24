module test_uninitialized_registers(
    input clk,
    input rst,
    input enable,
);

    reg [3:0] reg1; // This register is uninitialized
    reg [3:0] reg2 = 4'b0000; // This register is initialized
    reg [3:0] reg3; // This register is initialized later


    initial begin
        reg3 = 4'b1111;
    end

endmodule
