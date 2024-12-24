module case_statement_test(input [1:0] a, output reg y);
    // Case with missing default
    always @(*) begin
        case (a)
            2'b00: y = 1'b0;
            2'b01: y = 1'b1;
            2'b10: y = 1'b0;
            // Missing default
        endcase
    end

    // Case with duplicate conditions
    always @(*) begin
        case (a)
            2'b00: y = 1'b0;
            2'b01: y = 1'b1;
            2'b00: y = 1'b1; // Duplicate condition
            default: y = 1'b0;
        endcase
    end

    // Proper case statement
    always @(*) begin
        case (a)
            2'b00: y = 1'b0;
            2'b01: y = 1'b1;
            2'b10: y = 1'b0;
            default: y = 1'b0;
        endcase
    end
endmodule
