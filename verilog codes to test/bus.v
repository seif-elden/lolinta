module test_multi_driven_bus;

    // Declare some buses
    wire bus1;
    wire bus2;

    // Intentionally assign conflicting drivers to buses
    assign bus1 = 1'b0; // First driver for bus1
    assign bus1 = 1'b1; // Conflicting driver for bus1

    assign bus2 = 1'b0; // First driver for bus2
    assign bus2 = 1'bz; // Conflicting driver for bus2 (high-impedance)



endmodule
