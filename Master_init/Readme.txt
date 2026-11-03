# Usage

Writes the default parameters into the EEPROM of the Master (Arduino Mega).
Use it for a fresh Mega, or to reset all settings to the defaults.

The defaults live in libraries/NWEBTS/NWEBTS.h (NWEBTS_DEFAULTS),
currently: Einstellungen_NWEBTS_Sept_2026_Trier.xlsx

1. Upload and open the serial monitor (115200 baud): the table read back from EEPROM is printed
2. Upload the current Master version

The previous version (v0.8, a full master copy) is in archive/.
