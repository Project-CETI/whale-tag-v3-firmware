# Contributing

## Project Structure
Sensors are seperated into the following types of code. Keeping these as seperate as possible allows the project to be as flexible in the future. where a change to one layer does not greatly affect how other layers behave

1) Device Driver - this code is consists of the hardware device/sensor we wish to abstract on the tag. e.g. for the keller4ld which communicates via i2c, the specifc i2c commands sent to the sensor to read the preassure should be abstracted awway. This code should not preform executive decisions (such as how to handle errors, but should forward errors to code that does make those executive decisions)

2) Acquisition code - this code interacts with device driver to capture data (into RAM) from sensors at a specified sampling rate. This code should make decisions on how to recover from error, and provide methods to the rest of the code base for safely accessing the latest samples, and methods for accessing buffered data

3) Logging code - logging code handles the storage of gathered samples onto the SD card. This includes the reformatting data. E.g. if we want pressure sensor to be saved as a csv, this is the code that implements that.

4) Mission Control Code - this is high level logic that controls when the tag should perform specific actions  

5) Misc. - this can include things such as common error definitions, fw/hw version control, etc.

## ToDo's
"ToDo" items not listed in any particular order

### Tag Firmware
1) Implement MSC USB device via tinyUSB to forward SD card to HS USB
2) Implement CDC USB device via tinyUSB to controlling tag from host PC
3) Integrate Pressure sensor capture, logging, and use (for state transistion)
4) Integrate IMU sensor cpature, logging, and use (for state transistion)

### Tag Firmware Build System and Tools
1) setup openOCD config
2) set up `debug` make command that launches gdb on available STLink device
3) set up unit tests
4) migrate away from stm32cubeIDE/MX, directly use stm32u5core?

### Questions to Answer
1) Investigate wireless data offload options. Optical SPI link?
2) Is wireless charging a viable option?
3) Burnwire detection