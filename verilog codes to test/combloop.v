module example(input i1, i2, i3, output a);
    wire b, c;

    assign a = c & i1;       // a depends on c
    assign b = ~a;           // b depends on a
    assign c = (i2 | i3) & b; // c depends on b
endmodule
