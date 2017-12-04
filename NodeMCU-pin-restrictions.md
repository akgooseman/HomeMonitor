# NodeMCU Pin Restrictions

## Sigh - on boot, the following pins must be in the following states
* GPIO15/D8 must be LOW
* GPIO2/D4 must be HIGH
* GPIO0/D3 must be HIGH (to run) or LOW (to flash)
* GPIO16/D0 should connect to RST for sleep mode
* GPIO09 - no idea why it doesn't work. Prevents booting when grounded
* D10/TX - doesn't work, prevents networking from initializing (or maybe something else)

