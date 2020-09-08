# eeprom
Arduino PROM programmer

Ed's programmer for 93c56 proms - really for use in programming old Maxon radios but can be used for anything.
PROMs must be connected in 16 bit mode

Tested with Arduino UNO but should work on anything.
The Arduino board chosen determines maximum buffer size and, therefore, the maximum PROM size.
Uno is good for a 1k buffer.

The programmer uses the serial port for commands and to receive an Intel hex file.
Data buffer can be sent out of the serial port as an Intel hex file.


