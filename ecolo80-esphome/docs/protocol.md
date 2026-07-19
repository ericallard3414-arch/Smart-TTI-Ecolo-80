# Protocol Notes

## Frame format

Observed frames contain 14 bytes.

### Packet A

Example, power ON:

```text
4B 18 5C 3C 10 70 3C 7E 60 28 0A 80 00 93
```

Example, power OFF:

```text
4B 18 5C 3C 10 70 3C 7C 60 28 0A 80 00 91
```

### Keypad command

Power ON:

```text
33 18 5C 3C 10 70 3C 7E 60 28 0A 80 00 93
```

Power OFF:

```text
33 18 5C 3C 10 70 3C 7C 60 28 0A 80 00 91
```

### Packet B

Idle example:

```text
BB DC 0C 64 B4 64 00 00 C4 00 10 00 AA 26
```

Heating startup:

```text
BB DC 0C 64 B4 64 00 00 C4 00 18 00 AA 2E
```

Heating/running:

```text
BB DC 0C 64 B4 64 00 00 C4 00 58 00 AA 6E
```

## Known fields

### Packet A byte 2

Encoded target-temperature value.

### Packet A byte 7

Power/status flags:

- `0x7C`: OFF
- `0x7E`: ON

### Packet B byte 10

Heating state is currently decoded from bit `0x08`.

```cpp
bool heating = (packet_b[10] & 0x08) != 0;
```

Observed values:

- `0x10`: powered but idle
- `0x18`: heating startup
- `0x58`: heating/running

### Byte 13

Checksum byte.

## Bus cadence

Normal Packet A and Packet B traffic alternates at approximately three-second intervals.

## Transmission behavior

The working transmitter:

- Uses GPIO18
- Drives a 2N3904 open-collector stage
- Sends repeated keypad-style frames
- Checks the received transmit echo
- Waits for Packet A acknowledgement

Temperature controls group rapid clicks and send the final requested target directly.
