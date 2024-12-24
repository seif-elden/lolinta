module arithmetic_overflow_test;

    reg [3:0] a, b;          // 4-bit operands
    reg [7:0] c;             // 8-bit operand
    reg [3:0] result_small;  // 4-bit result
    reg [9:0] result_large;  // 10-bit result

    initial begin
        // Case 1: Overflow due to addition (4-bit result too small)
        result_small = a + b;

        // Case 2: No overflow (result fits in 9 bits and given 10)
        result_large = a + b;

        // Case 3: NO Overflow due to subtraction 
        result_small = a - b;

        // Case 4: Overflow due to multiplication (4-bit result too small)
        result_small = a * b;

        // Case 5: No overflow (result fits in 10 bits)
        result_large = a * b;

        // Case 6: Division by zero (runtime error)
        result_small = a / 4'b0000;

        // Case 7: No overflow (division result fits in 4 bits)
        result_small = a / 4'b0010;

        // Case 8: Overflow due to addition (4-bit result too small)
        result_small = 4'b1111 + 4'b1111 ;


    end
endmodule
