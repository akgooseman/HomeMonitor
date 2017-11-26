# NodeMCU Pin Restrictions

## Sigh - on boot, the following pins must be in the following states
* GPIO15/D8 must be LOW
* GPIO2/D4 must be HIGH
* GPIO0/D3 must be HIGH (to run) or LOW (to flash)
* GPIO16/D0 should connect to RST for sleep mode
