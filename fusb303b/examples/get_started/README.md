# FUSB303B get-started example

This example prints the safe default configuration without touching hardware.
In an application, create the driver on an existing `i2c_master` bus, configure
the role, and only then enable the autonomous Type-C state machine. The driver
does not own the enable GPIO, VBUS switch, or USB host stack.
