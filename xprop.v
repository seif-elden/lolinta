module example(input a, b, output c);
    reg x_signal;
    assign c = 4'bxxxx;             // Direct X propagation
    assign valid = 1 & 1'bx;        // X propagates due to bitwise AND
    assign result = a & b;          // No X propagation
    assign x = 0 & x_signal;        // No X propagation (0 AND anything = 0)
    assign y = x_signal | 1'bx;     // X propagates due to bitwise OR
endmodule