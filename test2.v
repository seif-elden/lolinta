module test_uninitialized_and_loop;
    input i1, i2, i3;
    output reg [3:0] uninit_reg; // Uninitialized register
    output wire loop_output;

    wire a, b, c;

    // Combinational loop
    assign a = c & i1;
    assign b = ~a;
    assign c = (i2 | i3) & b;

    initial begin
        // Uninitialized register left without assignment
        uninit_reg[3:0] = 4'bxxxx;
    end
endmodule
