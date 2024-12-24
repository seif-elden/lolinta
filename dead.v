module test;

    reg [2:0] state; // Declare a selector variable

    initial begin
        if (1'b1) state = 3'b001; // Assign a reachable value
        if (1'b0) state = 3'b010; // Unreachable condition
        
        case (state) // Start case block
            3'b000: ; // Unreachable branch
            3'b001: ; // Reachable branch
            3'b010: ; // Reachable branch
            3'b011: ; // Unreachable branch
        endcase // End case block

        if (1'b0) state = 3'b100; // Another unreachable condition
        if (state == 3'b101) ; // Valid reachable condition
    end

endmodule
