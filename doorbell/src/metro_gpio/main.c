/* ---------------------------------------------------------------------------
 *  metro_gpio — two buttons drive an RGB LED, PORT registers only.
 *
 *  Adafruit Metro M4 Express, ATSAMD51J19A (64-pin TQFP).  No Arduino, no
 *  CMSIS, no framework: every line below is a write to a memory-mapped
 *  register whose address is derived here from a data sheet.
 *
 *  Sources.  Three documents, and none of them is sufficient alone:
 *
 *    SAM D5x/E5x Family Data Sheet, DS60001507N — the PORT peripheral.
 *      Every PORT register address comes from here.
 *
 *    ARMv7-M Architecture Reference Manual — SysTick.  SysTick belongs to the
 *      Cortex-M4 CORE, not to the SAMD51, so Microchip's data sheet does not
 *      document it and Microchip's headers do not declare it.  A third
 *      category of fact, alongside "chip" and "board".
 *
 *    Adafruit's variant.cpp for metro_m4 — which chip pin the header pad
 *      silkscreened "7" actually lands on.  This mapping is a board fact, not
 *      a chip fact, so it appears in NO data sheet.  It is also not in the
 *      242-page Adafruit "Metro M4 Express" learn guide, which despite the
 *      filename is a CircuitPython tutorial and contains no port names at all.
 *
 *  REGISTERS.md, next to this file, is the reference for all of it: every
 *  address the program touches, what each register does, which bits bite, and
 *  how much each number is actually worth trusting.
 *
 *  The pins.  adafruit/ArduinoCore-samd, variants/metro_m4/variant.cpp:
 *
 *      D6  ->  PB15   group 1, bit 15    button 1        (line 34)
 *      D7  ->  PB12   group 1, bit 12    button 2        (line 35)
 *      D8  ->  PA21   group 0, bit 21    RGB red         (line 38)
 *      D9  ->  PA20   group 0, bit 20    RGB green       (line 39)
 *      D10 ->  PA18   group 0, bit 18    RGB blue        (line 40)
 *      D13 ->  PA16   group 0, bit 16    onboard LED     (line 45)
 *
 *  Data sheet Table 6-1 (pp. 31-32) confirms all six are bonded out on the
 *  64-pin package, which is worth checking before trusting any pin — PB12 for
 *  instance has no 48-pin column at all and exists only on 64-pin and larger.
 *
 *  Both buttons landed in group 1 and all four LEDs in group 0.  Not a
 *  property to rely on in general, but it does mean one PORT_IN read samples
 *  both buttons at the same instant and one mask write sets the whole colour.
 *
 *  Wiring.
 *
 *      3.3V --[ button 1 ]-- D6            no ground wire: see below
 *      3.3V --[ button 2 ]-- D7            no ground wire: see below
 *
 *      D8  --[ 330R ]--|>|-- red    -.
 *      D9  --[ 100R ]--|>|-- green  -+-- common cathode -- GND
 *      D10 --[ 100R ]--|>|-- blue   -'
 *
 *  The buttons source 3.3V rather than sinking to ground, so they are ACTIVE
 *  HIGH, and the pin still needs a path to ground for "released" to have a
 *  defined level: a floating CMOS input reads neither 0 nor 1, it reads
 *  whatever charge it last picked up.  That path is the internal pull-down,
 *  which is why there is no ground wire on the button side at all.  An
 *  external 10k from each pin to GND does the identical job if you would
 *  rather see it on the breadboard.
 *
 *  Three separate resistors, not one on the shared leg: a shared resistor
 *  makes each channel's brightness depend on how many are lit at once.
 *
 *  Why 100R on green and blue but 330R on red — forward voltage.  Red drops
 *  about 2.0 V, so 330R passes (3.3-2.0)/330 = 4 mA.  Green and blue drop
 *  about 3.0 V, and against a 3.3 V rail that same 330R would pass under
 *  1 mA and look dead.  Worth getting right before writing any code, because
 *  "blue is broken" and "blue is four times dimmer than red" look the same.
 *
 *  Behaviour.  Two inputs, and BOTH LEDs report the same four states — the
 *  RGB by colour, D13 by flash rate:
 *
 *      button 1   button 2      D13                    RGB
 *      --------   --------      -----------------      -----
 *         -          -          1 Hz heartbeat         dark
 *         X          -          10 Hz, the full 2 s    red
 *         -          X          20 Hz                  green
 *         X          X          full bright, steady    blue
 *
 *  Button 1 is a ONE-SHOT: pressing it opens a 2000 ms window measured from
 *  the press, and letting go does not close it.  Button 2 is a HOLD: it is
 *  true for exactly as long as the contact is made.  So "both at once" means
 *  the button 1 WINDOW overlaps the button 2 HOLD, not that the two presses
 *  coincide — tap 1, then hold 2 within two seconds, and you get blue and
 *  full bright for whatever is left of the window.
 *
 *  Saying the same thing on two LEDs is not redundancy, it is the whole
 *  diagnostic: the RGB needs six correct connections and a part whose common
 *  leg could go either way, D13 needs none.  When they disagree, the fault is
 *  in the RGB wiring; when they agree, the logic above is doing what it says.
 *
 *  D13/PA16 needs no wiring — it is the onboard red LED.  A bare-metal image
 *  has no console: it cannot print, so with the RGB alone a dark output has
 *  several indistinguishable causes and nothing to tell them apart.  Read as
 *  an instrument rather than as an output, D13 says:
 *
 *      1 Hz blink              running, nothing pressed
 *      10 Hz blink             inside button 1's window
 *      20 Hz blink             button 2 held, outside that window
 *      full bright, steady     both at once
 *      rate never changes      running, but neither button is read -> wiring
 *      dark, or a slow fade    not running: still in the UF2 bootloader
 *
 *  The idle state is a blink rather than a steady on for the sake of that
 *  last row: the bootloader itself leaves D13 pulsing, so "solid at idle"
 *  would have been ambiguous with the one state it most needs to rule out.
 *  Full bright is safe to use for both-pressed precisely because that state
 *  cannot happen unless the program is already running and reading pins.
 *
 *  The 1 Hz doubles as the calibration check for CPU_HZ — see there.
 * ------------------------------------------------------------------------- */

#include <stdint.h>

/* ---------------------------------------------------------------------------
 *  PORT — I/O Pin Controller.  Data sheet §32.
 *
 *  Base address 0x41008000 is from the Product Mapping figure (Figure 8-1,
 *  p.48): PORT is the fifth peripheral on bridge B, which starts at
 *  0x41000000 and hands each peripheral a 0x2000 slot.
 *
 *  Each PORT group is an identical bank of registers, 0x80 apart, group 0 =
 *  the PA pins and group 1 = the PB pins (§32.6.1 p.822, restated in the
 *  register summary §32.7 p.828).  So PORTA is at 0x41008000 and PORTB at
 *  0x41008080.  That 0x80 is the entire reason this program can treat "which
 *  port" as a plain function argument.
 * ------------------------------------------------------------------------- */
#define PORT_BASE          0x41008000u
#define PORT_GROUP(g)      (PORT_BASE + (g) * 0x80u)

/* Register offsets, data sheet §32.7 register summary, p.828.
 *
 * DIRSET/DIRCLR, OUTSET/OUTCLR and OUTTGL are why this needs no read-modify-
 * write anywhere: writing a 1 to bit y sets, clears or inverts ONLY bit y and
 * leaves the other 31 pins of the group untouched, in one atomic store
 * (§32.1 p.819).  A plain `OUT |= x` would work too, but it read-modify-
 * writes all 32 pins and races anything else touching the same group.
 *
 * That is load-bearing here rather than just tidy: the heartbeat on PA16 and
 * all three colour channels share group 0, so a read-modify-write on the
 * colour would periodically stamp on the heartbeat. */
#define PORT_DIR(g)        (*(volatile uint32_t *)(PORT_GROUP(g) + 0x00u))
#define PORT_DIRCLR(g)     (*(volatile uint32_t *)(PORT_GROUP(g) + 0x04u))
#define PORT_DIRSET(g)     (*(volatile uint32_t *)(PORT_GROUP(g) + 0x08u))
#define PORT_OUT(g)        (*(volatile uint32_t *)(PORT_GROUP(g) + 0x10u))
#define PORT_OUTCLR(g)     (*(volatile uint32_t *)(PORT_GROUP(g) + 0x14u))
#define PORT_OUTSET(g)     (*(volatile uint32_t *)(PORT_GROUP(g) + 0x18u))
#define PORT_OUTTGL(g)     (*(volatile uint32_t *)(PORT_GROUP(g) + 0x1Cu))
#define PORT_IN(g)         (*(volatile uint32_t *)(PORT_GROUP(g) + 0x20u))

/* PINCFG is a byte per pin, not a bitfield across pins: PINCFGn sits at
 * 0x40 + n within the group (§32.8.14, p.846).  Hence uint8_t. */
#define PORT_PINCFG(g, n)  (*(volatile uint8_t  *)(PORT_GROUP(g) + 0x40u + (n)))

/* PINCFG bits, §32.8.14 p.846. */
#define PINCFG_PMUXEN      (1u << 0)   /* 1 = a peripheral owns the pin, not PORT */
#define PINCFG_INEN        (1u << 1)   /* 1 = input buffer on; IN is dead without it */
#define PINCFG_PULLEN      (1u << 2)   /* 1 = internal pull resistor on */
#define PINCFG_DRVSTR      (1u << 6)   /* 1 = stronger drive */

/* ---------------------------------------------------------------------------
 *  SysTick — the core's 24-bit down-counter.  ARMv7-M ARM §B3.3.
 *
 *  Used polled, with its interrupt DISABLED (no TICKINT), because the whole
 *  program is one loop and there is nothing for an interrupt to preempt.  The
 *  loop reads COUNTFLAG, which sets when the counter wraps and clears on
 *  read — so each flag seen is exactly one elapsed millisecond and the read
 *  itself consumes it.  That stays honest only while the loop runs faster
 *  than 1 kHz (it runs at hundreds of kHz): a missed wrap is a lost
 *  millisecond, because COUNTFLAG cannot count to two.
 *
 *  Chosen over any of the chip's TC/TCC timers deliberately.  SysTick needs
 *  no GCLK routing and no MCLK unmask, so it works with the clock tree
 *  exactly as found; a TC would have meant configuring a generic clock first,
 *  which is a far larger bite than a two-second timer is worth.
 * ------------------------------------------------------------------------- */
#define SYST_CSR           (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR           (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR           (*(volatile uint32_t *)0xE000E018u)

#define SYST_ENABLE        (1u << 0)    /* start counting                      */
#define SYST_TICKINT       (1u << 1)    /* fire the SysTick exception — not us */
#define SYST_CLKSOURCE     (1u << 2)    /* 1 = processor clock, 0 = external   */
#define SYST_COUNTFLAG     (1u << 16)   /* wrapped since last read; read-clear */

/* The core clock, in hertz.  This is an ASSUMPTION, and the one number in
 * this file not cited to a document: nothing here configures the clock tree,
 * so the CPU runs at whatever it was handed.  Both paths land on 48 MHz —
 * that is the SAMD51 reset value of GCLK_MAIN (DFLL48M, open loop), and the
 * UF2 bootloader's check_start_application() runs BEFORE its system_init(),
 * so on an ordinary boot it jumps here without having changed anything.
 *
 * You do not have to take that on faith.  The heartbeat below is derived from
 * this constant at exactly 1 Hz, so counting D13 against a watch for ten
 * seconds measures it — ten blinks and the number is right.  Any error here
 * scales the 2000 ms window by precisely the same factor. */
#define CPU_HZ             48000000u

/* ------------------------------------------------------------------------- */

#define GROUP_A            0u
#define GROUP_B            1u

#define BTN_GROUP          GROUP_B
#define BTN1_BIT           15u         /* D6  = PB15 */
#define BTN2_BIT           12u         /* D7  = PB12 */

#define RGB_GROUP          GROUP_A
#define RED_BIT            21u         /* D8  = PA21 */
#define GRN_BIT            20u         /* D9  = PA20 */
#define BLU_BIT            18u         /* D10 = PA18 */
#define RGB_MASK           ((1u << RED_BIT) | (1u << GRN_BIT) | (1u << BLU_BIT))

#define HB_GROUP           GROUP_A
#define HB_BIT             16u         /* D13 = PA16, onboard red LED */

/* Common cathode: the shared leg goes to GND and a HIGH pin lights that
 * channel.  Set this to 0 for a common-ANODE part, whose shared leg goes to
 * 3.3V and which lights on a LOW pin — the only difference is which way round
 * the two stores at the bottom of the loop go.  On an RGB LED the long leg is
 * the shared one; if all three colours are lit when they should be dark, this
 * is the define to flip. */
#define RGB_COMMON_CATHODE 1

#define RED_MS             2000u       /* button 1's one-shot window */

/* D13 half-periods — the gap between TOGGLES, so half the flash period: a
 * 10 Hz flash is ten complete on-off cycles per second, hence 50 ms.  Over
 * button 1's 2000 ms window that is 20 cycles.
 *
 * HB_SOLID is not a period at all.  It is the sentinel for "stop toggling and
 * stay lit", which is why the code below tests it before using it as a
 * deadline — a zero half-period would otherwise mean toggle every pass, and
 * a few hundred kHz of that reads as a dim steady glow rather than the full
 * bright the state is supposed to signal. */
#define HB_IDLE_MS          500u       /*  1 Hz: alive, nothing pressed  */
#define HB_BTN1_MS           50u       /* 10 Hz: button 1's window open  */
#define HB_BTN2_MS           25u       /* 20 Hz: button 2 held, alone    */
#define HB_SOLID              0u       /* sentinel: full bright, no flash */

/* How long a button must hold a level before it counts.  Bounce did not
 * matter while the code only ever asked "is the pad high right now"; it
 * matters now that button 1 is edge-triggered, and it matters most on
 * RELEASE, where the contact chatters and would otherwise fire a fresh rising
 * edge that restarts the 2000 ms window at the moment you let go. */
#define DEBOUNCE_MS          5u

#define BTN_COUNT            2u
#define BTN1                 0u
#define BTN2                 1u

static const uint32_t btn_mask[BTN_COUNT] = { 1u << BTN1_BIT, 1u << BTN2_BIT };

int main(void)
{
    /* -----------------------------------------------------------------------
     *  The four LEDs — totem-pole outputs.  §32.6.3.3, p.825.
     *
     *  No clock to enable first.  PORT's APB clock is already running out of
     *  reset: MCLK.APBBMASK resets to 0x00018056 and bit 4 is PORT
     *  (§15.8.8, p.175).  Most peripherals on this chip need an explicit
     *  unmask; PORT is one of the few that does not.
     *
     *  PINCFG is cleared rather than left alone because PMUXEN must be 0 for
     *  PORT — as opposed to some SERCOM or timer — to own the pad
     *  (§32.8.14 p.846).  Out of reset it already is, but the bootloader runs
     *  before this code does and PINCFG is not reset by a warm reset of the
     *  CPU alone.  That is not hypothetical for PA16: the bootloader has been
     *  driving that LED itself, so PMUXEN may well arrive set, and leaving it
     *  set would send every heartbeat write to a register the pad is no
     *  longer listening to — the one indicator meant to prove the image runs
     *  would sit dark for a reason having nothing to do with the image.
     *
     *  Drive the level BEFORE flipping DIR to output.  DIR=1 starts driving
     *  whatever OUT already held, so clearing OUT first is what keeps the
     *  LEDs from flicking on for a few cycles at every boot.
     * --------------------------------------------------------------------- */
    PORT_PINCFG(RGB_GROUP, RED_BIT) = 0u;
    PORT_PINCFG(RGB_GROUP, GRN_BIT) = 0u;
    PORT_PINCFG(RGB_GROUP, BLU_BIT) = 0u;
    PORT_PINCFG(HB_GROUP,  HB_BIT)  = 0u;

#if RGB_COMMON_CATHODE
    PORT_OUTCLR(RGB_GROUP) = RGB_MASK;
#else
    PORT_OUTSET(RGB_GROUP) = RGB_MASK;
#endif
    PORT_OUTCLR(HB_GROUP)  = 1u << HB_BIT;

    PORT_DIRSET(RGB_GROUP) = RGB_MASK;
    PORT_DIRSET(HB_GROUP)  = 1u << HB_BIT;

    /* -----------------------------------------------------------------------
     *  D6 / PB15 and D7 / PB12 — inputs with internal pull-DOWNS.
     *
     *  Two traps here, both of them silent, and both are the reason the real
     *  data sheet was needed rather than the Adafruit guide:
     *
     *  1. INEN defaults to 0.  After reset every pin is an input with its
     *     input buffer DISABLED (§32.5.2 p.821).  Skip INEN and IN simply
     *     never reflects the pad — the pin looks permanently stuck at 0 and
     *     nothing about the code looks wrong.
     *
     *  2. There is no pull-direction register.  With PULLEN=1 the pull is a
     *     pull-UP when OUT=1 and a pull-DOWN when OUT=0 (Table 32-2 p.824,
     *     and the note under Figure 32-5 p.825).  OUT means "drive level" on
     *     an output and "which way to pull" on an input — same bit, two jobs.
     *
     *     OUTCLR on the next line is therefore what makes these buttons work
     *     at all.  They source 3.3V, so the pull has to oppose them and hold
     *     the pin LOW while the contact is open.  Pull UP instead and the pin
     *     reads high pressed AND released, the buttons become invisible, and
     *     the symptom — a heartbeat that never changes rate — points at the
     *     breadboard for a fault that is entirely in this one line.
     *
     *  Order matters for the same reason as above: set OUT to pick which way
     *  the pull goes, and only then switch the resistor on with PULLEN.  The
     *  other order enables a pull-up first and briefly fights the button.
     * --------------------------------------------------------------------- */
    PORT_DIRCLR(BTN_GROUP)           = btn_mask[BTN1] | btn_mask[BTN2];
    PORT_OUTCLR(BTN_GROUP)           = btn_mask[BTN1] | btn_mask[BTN2];
    PORT_PINCFG(BTN_GROUP, BTN1_BIT) = PINCFG_INEN | PINCFG_PULLEN;
    PORT_PINCFG(BTN_GROUP, BTN2_BIT) = PINCFG_INEN | PINCFG_PULLEN;

    /* -----------------------------------------------------------------------
     *  SysTick — one wrap per millisecond.
     *
     *  RVR holds the value RELOADED on wrap, so it is the period minus one:
     *  the counter runs N, N-1, ... 1, 0 and wraps on the tick after 0, which
     *  is N+1 ticks (ARMv7-M ARM §B3.3.3).  Writing CVR clears both the
     *  counter and any COUNTFLAG left over from before, so the first
     *  millisecond measured below is a whole one.
     * --------------------------------------------------------------------- */
    SYST_RVR = (CPU_HZ / 1000u) - 1u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CLKSOURCE | SYST_ENABLE;

    uint32_t now_ms    = 0u;   /* milliseconds since reset          */
    uint32_t hb_next   = 0u;   /* when D13 next toggles             */
    uint32_t hb_half   = 0xFFFFFFFFu;  /* current rate; impossible value
                                        * so the first pass counts as a
                                        * mode change and initialises */
    uint32_t red_until = 0u;   /* when button 1's window expires    */

    uint32_t raw_prev[BTN_COUNT] = { 0u, 0u };   /* last raw sample       */
    uint32_t settled [BTN_COUNT] = { 0u, 0u };   /* ms of its last change */
    uint32_t stable  [BTN_COUNT] = { 0u, 0u };   /* the debounced answer  */
    uint32_t btn1_prev = 0u;                     /* for edge detection    */

    for (;;) {
        /* One read, and the read is what consumes the flag — so it happens
         * exactly once per pass and the result is reused, never tested a
         * second time. */
        if (SYST_CSR & SYST_COUNTFLAG) {
            now_ms++;
        }

        /* -------------------------------------------------------------------
         *  Sample and debounce.  One PORT_IN read covers both buttons because
         *  they share a group.  A pin only earns its way into `stable` once
         *  it has held the same raw level for DEBOUNCE_MS; the timestamps are
         *  per-button, so chatter on one cannot restart the other's window.
         * ----------------------------------------------------------------- */
        uint32_t in = PORT_IN(BTN_GROUP);

        for (uint32_t i = 0u; i < BTN_COUNT; i++) {
            uint32_t raw = (in & btn_mask[i]) ? 1u : 0u;

            if (raw != raw_prev[i]) {
                raw_prev[i] = raw;
                settled[i]  = now_ms;
            } else if ((now_ms - settled[i]) >= DEBOUNCE_MS) {
                stable[i] = raw;
            }
        }

        /* -------------------------------------------------------------------
         *  Button 1 is an EDGE: the rising one arms a fresh 2000 ms window,
         *  and releasing the button does not end it.  Pressing again inside
         *  a live window restarts it rather than extending it.
         *
         *  The deadline is compared as a SIGNED difference rather than with
         *  `now_ms < red_until`.  now_ms wraps to zero after 49.7 days, and a
         *  plain comparison across that wrap reads as "expired 49 days ago"
         *  or as permanently lit, depending which side wrapped.  The
         *  subtraction wraps along with it, so the sign stays right.
         * ----------------------------------------------------------------- */
        if (stable[BTN1] && !btn1_prev) {
            red_until = now_ms + RED_MS;
        }
        btn1_prev = stable[BTN1];

        uint32_t red = ((int32_t)(red_until - now_ms) > 0) ? 1u : 0u;
        uint32_t grn = stable[BTN2];

        /* -------------------------------------------------------------------
         *  Blue is not red-plus-green: it REPLACES them, so exactly one bit
         *  of RGB_MASK is ever set and the LED never has to mix.
         * ----------------------------------------------------------------- */
        uint32_t colour;

        if (red && grn) {
            colour = 1u << BLU_BIT;
        } else if (red) {
            colour = 1u << RED_BIT;
        } else if (grn) {
            colour = 1u << GRN_BIT;
        } else {
            colour = 0u;
        }

        /* Two atomic stores, and the OFF one goes first so a colour change
         * never briefly lights two channels at once. */
#if RGB_COMMON_CATHODE
        PORT_OUTCLR(RGB_GROUP) = RGB_MASK & ~colour;
        PORT_OUTSET(RGB_GROUP) = colour;
#else
        PORT_OUTSET(RGB_GROUP) = RGB_MASK & ~colour;
        PORT_OUTCLR(RGB_GROUP) = colour;
#endif

        /* -------------------------------------------------------------------
         *  D13.  Driven from `red` and `grn` — the same two values the colour
         *  came from — rather than from the raw pins, so the two LEDs cannot
         *  disagree about what the program thinks is happening.  In
         *  particular D13 follows button 1 for the whole 2000 ms window and
         *  not merely while the contact is closed: a tap is over in 80 ms,
         *  and keying the rate to the contact made a press of button 1 look
         *  like it did nothing at all.
         * ----------------------------------------------------------------- */
        uint32_t half;

        if (red && grn) {
            half = HB_SOLID;
        } else if (red) {
            half = HB_BTN1_MS;
        } else if (grn) {
            half = HB_BTN2_MS;
        } else {
            half = HB_IDLE_MS;
        }

        if (half != hb_half) {
            /* Mode change.  Restart the pattern from a known point instead of
             * inheriting the old deadline — coming from idle, hb_next can be
             * most of a second away, which would swallow a quarter of button
             * 1's window before the fast flash became visible.  Every state
             * begins lit, so the transition is always something you can see.
             */
            hb_half = half;
            PORT_OUTSET(HB_GROUP) = 1u << HB_BIT;
            hb_next = now_ms + half;
        } else if (half != HB_SOLID && (int32_t)(now_ms - hb_next) >= 0) {
            PORT_OUTTGL(HB_GROUP) = 1u << HB_BIT;
            hb_next = now_ms + half;
        }
    }
}
