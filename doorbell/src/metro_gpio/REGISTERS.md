# Registers used by `metro_gpio`

Every memory-mapped register [`main.c`](main.c) and [`startup.c`](startup.c) touch,
what each one is for, and where the number came from.

This env builds with **no framework** — no Arduino core, no CMSIS, no vendor
headers. Nothing here is `PORT->Group[0].DIRSET.reg`; it is all a `volatile`
store to an address computed from a document. That is the point of the
exercise, and it is also why this file exists: with no headers there is no
place else the names live.

**Three sources, and none is sufficient alone.**

| Source | Covers | Why you need it |
|---|---|---|
| SAM D5x/E5x Family Data Sheet, **DS60001507** | PORT, MCLK | Microchip's chip. Every peripheral address. |
| **ARMv7-M Architecture Reference Manual** | SysTick, SCB | ARM's core, not Microchip's. Absent from the data sheet entirely. |
| Adafruit **`variants/metro_m4/variant.cpp`** | D-pin → chip pin | A *board* fact. Appears in no datasheet, of either vendor. |

The third one catches people out: `D7` is a silkscreen label, not a chip pin,
and the mapping is a property of how Adafruit routed this PCB.

> The repo's `Adafruit Metro M4 Express datasheet.pdf` is **not** a datasheet —
> it is a 242-page CircuitPython learn guide with no register content at all.

---

## 1. Address map at a glance

Everything the program reads or writes, in address order.

| Address | Register | Used for |
|---|---|---|
| `0x41008008` | `PORT.DIRSET` group A | make PA16/18/20/21 outputs |
| `0x41008014` | `PORT.OUTCLR` group A | LEDs off / channels dark |
| `0x41008018` | `PORT.OUTSET` group A | LED on |
| `0x4100801C` | `PORT.OUTTGL` group A | D13 flashing |
| `0x41008050` | `PORT.PINCFG16` group A | PA16 — D13 |
| `0x41008052` | `PORT.PINCFG18` group A | PA18 — D10, blue |
| `0x41008054` | `PORT.PINCFG20` group A | PA20 — D9, green |
| `0x41008055` | `PORT.PINCFG21` group A | PA21 — D8, red |
| `0x41008084` | `PORT.DIRCLR` group B | make PB12/PB15 inputs |
| `0x41008094` | `PORT.OUTCLR` group B | select pull-**down** |
| `0x410080A0` | `PORT.IN` group B | read both buttons |
| `0x410080CC` | `PORT.PINCFG12` group B | PB12 — D7, button 2 |
| `0x410080CF` | `PORT.PINCFG15` group B | PB15 — D6, button 1 |
| `0xE000E010` | `SYST_CSR` | enable SysTick, poll `COUNTFLAG` |
| `0xE000E014` | `SYST_RVR` | reload = 47999 → 1 ms |
| `0xE000E018` | `SYST_CVR` | clear counter and flag |
| `0xE000ED08` | `SCB.VTOR` | point at our vector table |
| `0xE000ED88` | `SCB.CPACR` | enable the FPU |

---

## 2. PORT — I/O Pin Controller

Data sheet §32.

### Where the base address comes from

`0x41008000`, from the Product Mapping figure (Figure 8-1, p.48): PORT is the
fifth peripheral on **bridge B**, which starts at `0x41000000` and gives each
peripheral a `0x2000` slot.

Groups are identical banks **`0x80` apart** (§32.6.1 p.822):

```
PORTA  (group 0, the PAxx pins)  =  0x41008000
PORTB  (group 1, the PBxx pins)  =  0x41008080
```

That regular stride is the whole reason the code can treat "which port" as an
ordinary function argument:

```c
#define PORT_GROUP(g)   (PORT_BASE + (g) * 0x80u)
```

### Register map

Offsets from §32.7, p.828. Both group columns are given because this program
uses both.

| Off | Group A | Group B | Name | Access | Function |
|---|---|---|---|---|---|
| `0x00` | `41008000` | `41008080` | `DIR` | R/W | Data Direction. 1 = output, 0 = input |
| `0x04` | `41008004` | `41008084` | `DIRCLR` | **W1** | write 1 → pin becomes **input** |
| `0x08` | `41008008` | `41008088` | `DIRSET` | **W1** | write 1 → pin becomes **output** |
| `0x0C` | `4100800C` | `4100808C` | `DIRTGL` | **W1** | write 1 → flip direction |
| `0x10` | `41008010` | `41008090` | `OUT` | R/W | Data Output Value — *dual purpose, see below* |
| `0x14` | `41008014` | `41008094` | `OUTCLR` | **W1** | write 1 → drive **low** / pull **down** |
| `0x18` | `41008018` | `41008098` | `OUTSET` | **W1** | write 1 → drive **high** / pull **up** |
| `0x1C` | `4100801C` | `4100809C` | `OUTTGL` | **W1** | write 1 → **invert** |
| `0x20` | `41008020` | `410080A0` | `IN` | **R** | Data Input Value — the actual pad level |
| `0x24` | `41008024` | `410080A4` | `CTRL` | R/W | input sampling control *(unused)* |
| `0x28` | `41008028` | `410080A8` | `WRCONFIG` | **W** | configure many pins in one write *(unused)* |
| `0x2C` | `4100802C` | `410080AC` | `EVCTRL` | R/W | event system input/output *(unused)* |
| `0x30` | `41008030` | `410080B0` | `PMUX0..15` | R/W | peripheral mux, **one nibble per pin** *(unused)* |
| `0x40` | `41008040` | `410080C0` | `PINCFG0..31` | R/W | **one byte per pin** — see §2.3 |

`DIR`/`OUT` are readable; the `SET`/`CLR`/`TGL` aliases read back as their
parent register. The program declares `PORT_DIR` and `PORT_OUT` for
completeness but never uses either.

### 2.1 The `SET` / `CLR` / `TGL` idiom

**W1** above means *write-1-to-act*: writing a `1` to bit *y* acts on pin *y*,
and writing a `0` does **nothing**. These are not normal registers you assign
a whole value to.

```c
PORT_OUTSET(GROUP_A) = 1u << 21;    /* PA21 high. PA20, PA18, PA16 untouched. */
```

versus the read-modify-write you would otherwise need:

```c
PORT_OUT(GROUP_A) |= (1u << 21);    /* loads all 32 bits, ors, stores all 32 */
```

Both light the LED. The difference is that the second one is three
instructions with a window in the middle, and it *writes all 32 pins of the
group*. §32.1 p.819 guarantees the first is a single atomic store.

**This is load-bearing in this program, not stylistic.** The heartbeat (PA16)
and all three colour channels (PA21/PA20/PA18) live in the same group. A
read-modify-write on the colour would periodically capture a stale copy of the
heartbeat bit and stamp it back — producing an LED that mostly works and
occasionally glitches, which is about the worst failure mode available.

### 2.2 `OUT` means two different things

Same bit, two jobs, decided by `DIR`:

| `DIR` bit | What `OUT` means |
|---|---|
| 1 (output) | the level to **drive** — high or low |
| 0 (input) | which way the internal resistor **pulls**, if `PULLEN` is set |

There is **no pull-direction register**. `PULLEN=1` with `OUT=1` is a pull-up;
`PULLEN=1` with `OUT=0` is a pull-down (Table 32-2 p.824, and the note under
Figure 32-5 p.825).

This is why `main.c` uses `OUTCLR` on the buttons. They source 3.3 V, so the
pull must oppose them and hold the pin low while the contact is open. Select a
pull-*up* by mistake and the pin reads high pressed **and** released — the
buttons become invisible, with no visible fault anywhere near the real cause.

### 2.3 `PINCFG` — one byte per pin

`PINCFGn` sits at `0x40 + n` **within the group** (§32.8.14, p.846). It is a
byte array indexed by pin, *not* a bitfield across pins like every register
above. Hence `uint8_t`:

```c
#define PORT_PINCFG(g, n)  (*(volatile uint8_t *)(PORT_GROUP(g) + 0x40u + (n)))
```

| Bit | Name | Meaning | The trap |
|---|---|---|---|
| 0 | `PMUXEN` | Peripheral Multiplexer Enable | **1 = a SERCOM/TC/TCC owns the pad, not PORT.** Your `OUT` writes go nowhere and the code looks perfect |
| 1 | `INEN` | Input Enable | Defaults to **0**. Without it `IN` reads 0 forever — the pin looks stuck low |
| 2 | `PULLEN` | Pull Enable | Turns the resistor on. Direction comes from `OUT` |
| 6 | `DRVSTR` | Drive Strength | Stronger drive. Irrelevant at LED currents |

Bits 3-5 and 7 are reserved.

`main.c` writes exactly two values:

- **`0x00`** on the four LED pins — clears `PMUXEN` so PORT owns the pad. Not
  paranoia: the UF2 bootloader drives the D13 LED itself before handing over,
  and `PINCFG` survives a warm CPU reset.
- **`0x06`** on the two buttons — `INEN | PULLEN`, with `OUT` already cleared,
  so: input buffer on, resistor on, pulling down.

### 2.4 Pin assignments

Board mapping from `variant.cpp`; package check from data sheet Table 6-1
(pp. 31-32) — worth doing before trusting any pin, since e.g. PB12 has no
48-pin column at all and exists only on 64-pin parts and larger.

| Pad | Chip pin | Group | Bit | `PINCFG` address | Role |
|---|---|---|---|---|---|
| D6 | PB15 | 1 | 15 | `0x410080CF` | button 1 — one-shot |
| D7 | PB12 | 1 | 12 | `0x410080CC` | button 2 — hold |
| D8 | PA21 | 0 | 21 | `0x41008055` | RGB red |
| D9 | PA20 | 0 | 20 | `0x41008054` | RGB green |
| D10 | PA18 | 0 | 18 | `0x41008052` | RGB blue |
| D13 | PA16 | 0 | 16 | `0x41008050` | onboard LED (`LED_BUILTIN`) |

Both buttons landed in group 1 and all four LEDs in group 0, so one `IN` read
samples both buttons at the same instant and one masked write sets the whole
colour.

---

## 3. SysTick — the core's 24-bit down-counter

ARMv7-M ARM §B3.3. **Not in the SAMD51 data sheet**, because it is not
Microchip's — it belongs to the Cortex-M4 and sits at the same address on
every ARMv7-M part.

| Address | Name | Common alias | Function |
|---|---|---|---|
| `0xE000E010` | `SYST_CSR` | `STCSR`, `SysTick->CTRL` | Control and Status |
| `0xE000E014` | `SYST_RVR` | `STRVR`, `SysTick->LOAD` | Reload Value, 24-bit |
| `0xE000E018` | `SYST_CVR` | `STCVR`, `SysTick->VAL` | Current Value |
| `0xE000E01C` | `SYST_CALIB` | `STCALIB` | Calibration hint *(unused)* |

### `SYST_CSR` bits

| Bit | Name | Meaning |
|---|---|---|
| 0 | `ENABLE` | start counting |
| 1 | `TICKINT` | raise the SysTick exception on wrap — **left 0 here** |
| 2 | `CLKSOURCE` | 1 = processor clock, 0 = external reference |
| 16 | `COUNTFLAG` | set on wrap, **cleared by reading this register** |

`main.c` writes `0x05` = `ENABLE | CLKSOURCE`: run from the CPU clock, no
interrupt. The whole program is one loop, so there is nothing for an interrupt
to preempt and polling is simpler than a handler plus a `volatile` counter.

### Three things that bite

**`RVR` is period − 1.** The counter runs *N, N-1, … 1, 0* and wraps on the
tick after zero — that is N+1 ticks. For 1 ms at 48 MHz:

```c
SYST_RVR = (48000000u / 1000u) - 1u;   /* 47999 */
```

**`COUNTFLAG` is read-to-clear and cannot count to two.** Each flag observed is
exactly one elapsed millisecond, and the read consumes it. So it must be read
**once** per pass and the result reused:

```c
if (SYST_CSR & SYST_COUNTFLAG) {     /* one read; never test twice */
    now_ms++;
}
```

If the loop ever ran slower than 1 kHz, wraps would be silently lost and the
clock would run slow. This loop runs at hundreds of kHz, so there is wide
margin — but the margin is an assumption, not a guarantee.

**Writing `CVR` clears the counter *and* `COUNTFLAG`,** whatever value you
write. That is how the code guarantees its first measured millisecond is a
whole one.

### Why SysTick and not a TC/TCC

SysTick needs no GCLK routing and no MCLK unmask — it works with the clock
tree exactly as found. Any of the chip's timers would have meant configuring a
generic clock generator first, which is a far larger bite than a two-second
timer justifies.

### The 48 MHz assumption

`CPU_HZ` is the one number in the program **not** cited to a document. Nothing
here configures the clock tree, so the CPU runs at whatever it was handed.
Both paths land on 48 MHz:

- it is the SAMD51 reset value of `GCLK_MAIN` (DFLL48M, open loop), and
- the UF2 bootloader's `check_start_application()` runs *before* its
  `system_init()`, so on an ordinary boot it jumps to the app without having
  changed anything.

You do not have to take that on faith. The D13 idle blink is derived from
`CPU_HZ` at exactly 1 Hz, so **counting blinks against a watch for ten seconds
measures it.** Any error scales button 1's 2000 ms window by the same factor.

---

## 4. System Control Block — used in `startup.c`

Also ARM core, not Microchip.

| Address | Name | Full name | Function |
|---|---|---|---|
| `0xE000ED08` | `VTOR` | Vector Table Offset Register | Where the core fetches exception vectors |
| `0xE000ED88` | `CPACR` | Coprocessor Access Control Register | Bits 20-23 = full access to CP10/CP11 = **enable the FPU** |

**`VTOR`** is set to `0x4000`, the start of our image. The UF2 bootloader
normally does this before jumping, but it costs one store not to depend on it —
and if `VTOR` were left pointing at the bootloader's table, every fault and
interrupt would dispatch into *the bootloader's* handlers instead of ours.

**`CPACR`** enables the FPU. This program uses no floating point and builds
`-mfloat-abi=soft`, so it is pure insurance: the SAMD51 is a Cortex-M4**F**,
and executing a VFP instruction with the FPU still disabled is a usage fault
that surfaces at a completely unrelated line. Costs two instructions plus a
`dsb`/`isb`.

---

## 5. Referenced in comments, deliberately never written

**`MCLK.APBBMASK`** — the APB bridge-B clock gate. PORT is **bit 4**, and the
register resets with it already set, which is why a program that only uses
GPIO needs no clock setup whatsoever. Most SAMD51 peripherals *do* need an
explicit unmask before their registers respond at all; PORT is one of the few
that does not, and that exception is why this program can be this short.

*(MCLK sits on bridge A. The exact address is not used by the code and was not
re-derived for this document — see Provenance.)*

---

## 6. Checking the binary against this document

Nothing above is trusted at runtime, so it is worth confirming the compiler
emitted what you meant. Peripheral addresses land in the literal pool:

```sh
OD=C:/pio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-objdump.exe
"$OD" -d --no-show-raw-insn .pio/build/metro_gpio/firmware.elf \
  | grep -oE "0x(4100[0-9a-f]{4}|e000e[0-9a-f]{3}|e000ed[0-9a-f]{2})" | sort -u
```

On the current build that prints nine addresses:

```
0x41008014  0x41008018  0x4100801c  0x41008055  0x41008084
0x410080a0  0xe000e014  0xe000ed08  0xe000ed88
```

**Nine, not eighteen — and that is expected.** With `-O1` the compiler keeps
one address in a register and reaches the neighbours with `adds`/`subs`, since
they are a few bytes apart. `0x41008055` (PINCFG PA21) minus 1, 3 and 5 covers
PA20, PA18 and PA16; minus 0x41 lands on `OUTCLR`. So verifying the rest means
reading the instructions, not grepping literals:

```sh
"$OD" -d --no-show-raw-insn .pio/build/metro_gpio/firmware.elf \
  --start-address=0x4040 --stop-address=0x40a0
```

Two other checks worth knowing:

```sh
# .text MUST be at VMA 0x00004000 — linked at 0, flashing destroys the
# bootloader and recovery needs an SWD probe.
"$OD" -h .pio/build/metro_gpio/firmware.elf

# What is ACTUALLY on the chip, rather than what you believe you sent.
# Board must be in the bootloader (double-tap RESET).
C:/pio/packages/tool-bossac/bossac.exe -p COM12 -o 0x4000 -r1024 readback.bin
```

---

## 7. Provenance

Not every number here carries the same weight, so:

- **PORT offsets, PINCFG bits, the `0x41008000` base and the `0x80` group
  stride** were derived from DS60001507 when `main.c` was written, and the
  page citations in this file are that work. They are *consistent with* the
  compiled binary, but consistency only proves the macros compute what the
  source says — not that the source matches Microchip. They are further
  corroborated by the hardware behaving as described, which is the strongest
  evidence available without the PDF to hand.
- **SysTick and SCB** are architectural ARMv7-M and identical across vendors.
- **Pin mappings** are from `adafruit/ArduinoCore-samd`, `variants/metro_m4/`,
  cross-checked against `variant.h` (`LED_BUILTIN` → `PIN_LED_13` → 13) and
  package availability in Table 6-1.
- **`MCLK.APBBMASK`'s address** is the one thing stated without either a
  citation or a compiled artifact behind it. The code never writes it.

The data sheet is not committed to this repo (~18.7 MB, 2002 pp):
<https://ww1.microchip.com/downloads/aemDocuments/documents/MCU32/ProductDocuments/DataSheets/SAM-D5x-E5x-Family-Data-Sheet-DS60001507.pdf>
