# Balancing Robot — Initial Bill of Materials

This is a planning BOM, not a released purchasing list. Exact board/module revisions and ratings must be confirmed before connecting the battery or motors. **TBD** means the value depends on a part selection or measurement that is not yet available.

## Core parts

| Item | Qty | Status | Requirements / notes |
|---|---:|---|---|
| ESP32-S3 controller board | 1 | Selected family; exact board TBD | Confirm board schematic, available 3.3 V GPIO, PWM/timer allocation, USB programming, and power-input limits. ESP32-S3 GPIO is not 5 V tolerant. |
| GY-9250 IMU module | 1 | Selected family; revision/sensor TBD | Confirm the actual IMU fitted (often sold as an MPU-9250-based module), its supply/logic voltage, regulator/level-shifter presence, bus options, and library support. Mount rigidly near the chassis center and close to the axle plane. |
| SimpleFOC-compatible DRV8313 driver board | 2 | Selected family; exact board TBD / **not yet validated** | Verify board input-voltage rating, continuous phase current with intended cooling, current sensing, supported 3-PWM/6-PWM interface, enable/fault behavior, and transient handling. IC ratings alone do not establish the module's ratings. |
| 319 KV brushless motor | 2 | Selected; full motor model/specs TBD | Obtain rated voltage/current, phase resistance, pole-pair count, thermal limits, and shaft/mount details. KV is a speed constant, not a torque/current rating. |
| AS5048A magnetic encoder | 2 | Selected family; module/revision TBD | Confirm module voltage and logic levels, interface (SPI preferred for this plan), magnet type/diameter, mounting, and air gap. Use one diametric magnet per encoder, centered on the measured shaft. |
| 4S LiPo battery pack | 1 | Selected chemistry/configuration; capacity/C rating TBD | 14.8 V nominal, 16.8 V fully charged. Select capacity and discharge rating from measured/estimated peak motor current and desired runtime. Ensure the pack has the appropriate balance connector. |
| USB-C 4S charge/discharge module | 1 | Selected; exact model TBD | Confirm it is designed for the pack's chemistry and 4S cell count, supports the required balance charging, and has a discharge/load output rated for the robot's voltage and current. Verify USB-C input/PD requirements, charge current, cell protection, and whether it permits charging while a load is connected. Follow the module's documented battery and load terminals. |
| Buck converter | 1 | Required; exact unit TBD | Input must tolerate at least the pack's 16.8 V full-charge voltage and expected transients. Output must match the controller board's documented input (typically a regulated 5 V input, if supported); size for controller, sensors, and peripherals with margin. |

## Protection, wiring, and mechanical items

| Item | Qty | Status | Requirements / notes |
|---|---:|---|---|
| Battery-side fuse and holder | 1 | Required; rating TBD | Install close to the battery positive lead. Select from wire/connector ratings and measured or specified peak current; do not size from motor KV. |
| Battery disconnect / power switch | 1 | Required; rating TBD | DC voltage/current rating must suit the pack and load. Provide an accessible way to disconnect battery power. |
| Hardware motor-disable / emergency stop | 1 | Required | Must place both drivers in their documented disabled state independently of firmware. Define safe default level at reset and loss of MCU power. Consider a separate physical battery disconnect for complete isolation. |
| Power distribution and connectors | As needed | Required; ratings TBD | Use keyed, polarized battery connector(s), adequate copper/wire gauge, strain relief, and separate motor and logic branches. Select ratings from current and temperature requirements. |
| Driver-local DC-link capacitors | As specified by driver boards | TBD | Populate/retain the capacitance and voltage rating required by each driver-board design. Consider wiring inductance and regeneration; do not substitute arbitrary capacitors without checking the board design. |
| Chassis, two wheels, motor mounts, battery restraint | 1 set | Required; dimensions TBD | Rigidly mount the IMU and motors. Secure the battery against movement and impact. Choose wheel size/gearing after confirming motor torque and speed. |
| Encoder magnet mounts / spacers | 2 sets | Required | Maintain concentric alignment and the encoder manufacturer's specified air gap. Prevent magnet movement at motor speed. |
| Test stand / wheel-clear fixture | 1 | Strongly recommended | Allows initial powered tests with the robot secured and wheels unable to drive across a surface. |
| Optional current/voltage measurement | As needed | TBD | Add appropriate phase-current sensing if required by the chosen FOC/current-control mode; add battery voltage monitoring for undervoltage and logging. Confirm whether driver boards already provide usable sensing. |

## Sizing and purchasing gates

- **Driver gate:** identify the exact DRV8313 board/revision and review its schematic and thermal/current limits before connecting either motor. Confirm it is suitable for each motor's phase current and expected low-speed/stall conditions.
- **Battery/module gate:** confirm the charge/discharge module's exact model, 4S LiPo compatibility, balance-charging and protection features, USB-C input requirements, and rated discharge current. Choose pack capacity and discharge rating only after establishing motor current limits, acceleration/stall behavior, and expected runtime. Determine from the module documentation whether the robot load must be disconnected during charging.
- **Power gate:** check the buck converter's input range against 16.8 V plus transients, and check startup/current demand of the complete logic load. Route logic power separately from motor power.
- **Motor gate:** get actual motor datasheets. The 319 KV value alone cannot determine whether direct drive can balance the robot or what current the drivers must handle.
- **Module gate:** record board/module part numbers and schematics for the ESP32-S3 board, GY-9250, AS5048A breakouts, and both motor drivers before freezing wiring or pin assignments.
