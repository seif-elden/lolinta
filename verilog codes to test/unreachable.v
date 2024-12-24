module test_fsm(
    input clk,
    input rst,
    output reg [1:0] state
);
    parameter S0 = 2'b00;
    parameter S1 = 2'b01;
    parameter S2 = 2'b10; // This state will be unreachable

    always @(posedge clk or posedge rst) begin
        if (rst)
            state <= S0;
        else begin
            case (state)
                S0: state <= S1;
                S1: state <= S0;
                S2: state <= S0; // Transition exists, but S2 is never entered
            endcase
        end
    end
endmodule
