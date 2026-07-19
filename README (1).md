# Protocol Notes

These decoded fields and timing values apply to the GUANGDONG CHICO CC207S-V2.1 controller board.

Observed frames contain 14 bytes.

## Packet A

Power ON:

```text
4B 18 5C 3C 10 70 3C 7E 60 28 0A 80 00 93
```

Power OFF:

```text
4B 18 5C 3C 10 70 3C 7C 60 28 0A 80 00 91
```

## Packet B

Idle:

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

Heating is decoded from Packet B byte 10 bit `0x08`.

Consumption is estimated from the selected model's rated operating current, not directly measured from the bus.
