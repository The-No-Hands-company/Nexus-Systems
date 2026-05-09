# Sensor Driver Showcase

File: examples/embedded/sensor_driver.nxl

This example demonstrates embedded-style driver logic in NexusLang:

- Register abstraction for control/status/data fields
- Initialization and calibration routines
- Sampling loop with deterministic raw input simulation
- Threshold alarm behavior for calibrated output

## Capability Notes

The program models common embedded workflows:

- Boot-time device initialization
- Register writes and reads through helper functions
- Periodic sampling and transformation of raw sensor values
- Runtime checks that trigger alert conditions

## Run

python src/main.py examples/embedded/sensor_driver.nxl
