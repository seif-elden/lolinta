module test_fsm_with_errors;
    input clk;
    input rst;
    output reg [1:0] state;

    parameter S0 = 2'b00;
    parameter S1 = 2'b01;
    parameter S2 = 2'b10; // Unreachable state

    always @(posedge clk or posedge rst) begin
        if (rst) 
            state <= S0;
        else begin
            case (state)
                S0: state <= S1;
                S1: state <= S0;
                S2: state <= S0; // This transition is present but S2 is never entered
            endcase
        end
    end
endmodule
