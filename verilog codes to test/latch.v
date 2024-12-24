module latch_inference(input wire a, b, output reg q);
    always @* begin
        if (a)
            q = b; // Missing else branch causes a latch
    end
endmodule
