// Anti-Rage Doorbell — RECEIVER SIDE ("indoor chime") on an Arduino Mega 2560.
//
// PURPOSE
//   Bench rig. This proves the receiver's components on known-good 5 V hardware
//   before the design is compacted onto an ESP32. The headline item is the AUDIO
//   OUTPUT CHAIN: an LM386 driving an 8 ohm 1 W speaker, fed by a real 8 kHz PCM
//   engine. That is the bench stand-in for the MAX98357A I2S DAC on the final
//   chime node, and it exercises the same VoiceMsg payload the radio will carry.
//
// UNDER TEST
//   passive buzzer (ding-dong) · LM386 + 8 ohm speaker via Timer1 PWM at 8 kHz ·
//   2 activation-status LEDs · DoorbellMsg / VoiceMsg decode · the rage-lockout
//   presentation (LED goes solid, chime goes silent).
//
// NOT UNDER TEST
//   The radio: there isn't one. Packets arrive over serial as '#' frames from
//   Doorbell_Sender_Side, or from the built-in commands below.
//
// TIMER BUDGET (all four in use, none fighting)
//   Timer0  millis()/delay()          - untouched
//   Timer1  8-bit fast PWM on OC1A/D11 = the 62.5 kHz audio carrier
//   Timer2  tone()                    = the buzzer
//   Timer3  CTC @ 8 kHz               = the PCM sample clock
//   => never call analogWrite() on D11 or D12 here; that is Timer1's territory.
//
// RUN
//   pio run -t upload -t monitor
// PASS
//   On boot: both LEDs blink, buzzer dings, then a clean 440 Hz tone from the
//   speaker. Type 'd' -> ding-dong + green LED pulse. Type 'v' -> a second of
//   tone through the LM386. Paste a '#01 01 ..' frame from the sender -> the
//   same reaction, driven by real wire-format bytes.

#include <Arduino.h>
#include "config_mega.h"
#include "protocol.h"
#define LINK_ENABLE_RX          // this side reads the simulated link
#include "sim_link.h"
#include "synth.h"

// ============================================================================
//  PCM audio engine — Timer1 carrier + Timer3 sample clock
// ============================================================================
// Single producer (loop) / single consumer (ISR). The indices are uint8_t and
// the ring is exactly 256 bytes, so they wrap on their own and every access is
// an atomic 8-bit load/store on AVR. No cli/sei needed.
static volatile uint8_t audioBuf[AUDIO_RING_BYTES];
static volatile uint8_t audioHead = 0;   // written by loop()
static volatile uint8_t audioTail = 0;   // written by the ISR

ISR(TIMER3_COMPA_vect) {
  if (audioTail != audioHead) {
    OCR1A = audioBuf[audioTail++];
  } else {
    OCR1A = AUDIO_MID_RAIL;              // underrun / idle: sit at silence
  }
}

static void audioBegin() {
  pinMode(RX_SPEAKER_PWM_PIN, OUTPUT);

  // Timer1: 8-bit fast PWM (WGM13:0 = 0101), non-inverting on OC1A, no
  // prescaler -> 16 MHz / 256 = 62.5 kHz. Well above hearing, so the LM386's
  // input RC network averages it into an analog waveform instead of whining.
  TCCR1A = _BV(COM1A1) | _BV(WGM10);
  TCCR1B = _BV(WGM12)  | _BV(CS10);
  OCR1A  = AUDIO_MID_RAIL;

  // Timer3: CTC at exactly VOICE_SAMPLE_RATE. 16 MHz / 8 / 8000 = 250 ticks.
  TCCR3A = 0;
  TCCR3B = _BV(WGM32) | _BV(CS31);                       // CTC, prescaler 8
  OCR3A  = (uint16_t)((F_CPU / 8UL / VOICE_SAMPLE_RATE) - 1UL);
  TIMSK3 = _BV(OCIE3A);
}

// Returns false when the ring is full (255 usable bytes = ~32 ms of audio).
static bool audioPush(uint8_t sample) {
  const uint8_t next = (uint8_t)(audioHead + 1);
  if (next == audioTail) return false;
  audioBuf[audioHead] = sample;
  audioHead = next;
  return true;
}

static void audioWaitDrain() {
  while (audioHead != audioTail) { /* let the ISR finish the buffer */ }
}

// Synthesise `ms` of `freq` into the ring, blocking while it is full. The
// linear attack/release is not decoration: without it the speaker cone steps
// from silence to full amplitude and the LM386 gives you an audible pop.
static void audioPlayTone(uint16_t freq, uint16_t ms) {
  const uint32_t total = ((uint32_t)VOICE_SAMPLE_RATE * ms) / 1000UL;
  const uint32_t ramp  = (total / 4UL) < 200UL ? (total / 4UL) : 200UL;
  uint16_t phase = 0;
  const uint16_t inc = synthPhaseInc(freq, VOICE_SAMPLE_RATE);

  for (uint32_t i = 0; i < total; i++) {
    int16_t s = (int16_t)synthNext8(phase, inc) - AUDIO_MID_RAIL;
    if (ramp) {
      if (i < ramp)                    s = (int16_t)(((int32_t)s * (int32_t)i) / (int32_t)ramp);
      else if (i > (total - ramp))     s = (int16_t)(((int32_t)s * (int32_t)(total - i)) / (int32_t)ramp);
    }
    while (!audioPush((uint8_t)(s + AUDIO_MID_RAIL))) { /* spin: ISR is draining */ }
  }
}

// Feed one received VoiceMsg into the speaker. int16 -> 8-bit unsigned PWM.
// On the ESP32 this whole function becomes a single i2s_write() of m.pcm.
static void audioPlayChunk(const VoiceMsg &m) {
  for (uint8_t i = 0; i < VOICE_SAMPLES; i++) {
    const uint8_t s = (uint8_t)((m.pcm[i] >> 8) + AUDIO_MID_RAIL);
    while (!audioPush(s)) { /* spin: ISR is draining */ }
  }
}

// ============================================================================
//  Buzzer + status LEDs
// ============================================================================
static bool     chimeLocked   = false;   // mirrors the sender's rage lockout
static uint32_t doorbellLedOff = 0;      // non-blocking pulse deadline
static bool     voiceActive   = false;

static void chimeDingDong() {
  // tone() is Timer2; the PCM engine on Timer1/Timer3 keeps running underneath.
  tone(RX_BUZZER_PIN, 988); delay(180);   // ding (B5)
  tone(RX_BUZZER_PIN, 784); delay(280);   // dong (G5)
  noTone(RX_BUZZER_PIN);
}

static void ledsBegin() {
  pinMode(RX_LED_DOORBELL_PIN, OUTPUT);
  pinMode(RX_LED_VOICE_PIN, OUTPUT);
  digitalWrite(RX_LED_DOORBELL_PIN, LOW);
  digitalWrite(RX_LED_VOICE_PIN, LOW);
}

static void ledsService() {
  // Locked overrides the pulse: solid green is "the sender is muted right now".
  if (chimeLocked) {
    digitalWrite(RX_LED_DOORBELL_PIN, HIGH);
  } else if (doorbellLedOff && (int32_t)(millis() - doorbellLedOff) >= 0) {
    digitalWrite(RX_LED_DOORBELL_PIN, LOW);
    doorbellLedOff = 0;
  }
  digitalWrite(RX_LED_VOICE_PIN, voiceActive ? HIGH : LOW);
}

static void pulseDoorbellLed(uint16_t ms) {
  digitalWrite(RX_LED_DOORBELL_PIN, HIGH);
  doorbellLedOff = millis() + ms;
}

// ============================================================================
//  Packet handling — the only part that survives unchanged onto the ESP8266
// ============================================================================
static void onDoorbell(const DoorbellMsg &m) {
  chimeLocked = (m.locked != 0);
  pulseDoorbellLed(600);

  Serial.print(F("[rx] DOORBELL press#"));
  Serial.print(m.pressCount);
  Serial.print(F(" locked="));
  Serial.print(m.locked);
  Serial.print(F(" seq="));
  Serial.println(m.seq);

  if (chimeLocked) {
    Serial.println(F("[rx] rage lockout - chime stays SILENT, LED solid"));
    return;                                // muted, exactly like the sender
  }
  chimeDingDong();
}

static void onPacket(const uint8_t *data, uint8_t len) {
  if (len < 2 || data[0] != PROTO_VERSION) {
    Serial.println(F("[rx] dropped: bad version/short frame"));
    return;
  }

  switch (data[1]) {
    case MSG_DOORBELL:
      if (len < sizeof(DoorbellMsg)) { Serial.println(F("[rx] dropped: short DOORBELL")); return; }
      { DoorbellMsg m; memcpy(&m, data, sizeof m); onDoorbell(m); }
      break;

    case MSG_VOICE_BEGIN:
      voiceActive = true;
      Serial.println(F("[rx] VOICE session OPEN - speaker live"));
      break;

    case MSG_VOICE_CHUNK:
      if (len < sizeof(VoiceMsg)) { Serial.println(F("[rx] dropped: short VOICE_CHUNK")); return; }
      { VoiceMsg m; memcpy(&m, data, sizeof m); voiceActive = true; audioPlayChunk(m); }
      break;

    case MSG_VOICE_END:
      voiceActive = false;
      audioWaitDrain();
      Serial.println(F("[rx] VOICE session CLOSED"));
      break;

    default:
      Serial.print(F("[rx] dropped: unknown type "));
      Serial.println(data[1]);
      break;
  }
}

// ============================================================================
//  Bench self-test + typed commands
// ============================================================================
static void selfTest() {
  Serial.println(F("[rx] self-test: LEDs"));
  for (uint8_t i = 0; i < 3; i++) {
    digitalWrite(RX_LED_DOORBELL_PIN, HIGH); digitalWrite(RX_LED_VOICE_PIN, LOW);  delay(150);
    digitalWrite(RX_LED_DOORBELL_PIN, LOW);  digitalWrite(RX_LED_VOICE_PIN, HIGH); delay(150);
  }
  digitalWrite(RX_LED_VOICE_PIN, LOW);

  Serial.println(F("[rx] self-test: buzzer"));
  chimeDingDong();
  delay(300);

  Serial.println(F("[rx] self-test: LM386 + speaker, 440 Hz"));
  audioPlayTone(440, 700);
  audioWaitDrain();

  Serial.println(F("[rx] self-test done"));
}

static void printHelp() {
  Serial.println(F("[rx] commands:  d=doorbell  l=locked doorbell  v=voice tone"));
  Serial.println(F("[rx]            b=buzzer    t=self-test        ?=help"));
  Serial.println(F("[rx] or paste a '#....' frame straight from the sender sketch"));
}

static void onCommand(const char *line) {
  switch (line[0]) {
    case 'd': {                              // a synthetic unlocked doorbell
      DoorbellMsg m;
      m.version = PROTO_VERSION; m.type = MSG_DOORBELL;
      m.pressCount = 1; m.locked = 0; m.seq = 0;
      onDoorbell(m);
      break;
    }
    case 'l': {                              // a synthetic locked doorbell
      DoorbellMsg m;
      m.version = PROTO_VERSION; m.type = MSG_DOORBELL;
      m.pressCount = RAGE_LIMIT; m.locked = 1; m.seq = 0;
      onDoorbell(m);
      break;
    }
    case 'v':
      voiceActive = true;
      ledsService();
      Serial.println(F("[rx] voice tone -> LM386"));
      audioPlayTone(440, 1000);
      audioWaitDrain();
      voiceActive = false;
      break;
    case 'b': chimeDingDong(); break;
    case 't': selfTest();      break;
    case '?': printHelp();     break;
    default:
      Serial.print(F("[rx] unknown command: "));
      Serial.println(line);
      break;
  }
}

// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(50);

  pinMode(RX_BUZZER_PIN, OUTPUT);
  ledsBegin();
  audioBegin();

  Serial.println(F("[rx] RECEIVER bench ready (Mega 2560)"));
  printHelp();
  selfTest();
}

void loop() {
  uint8_t buf[sizeof(VoiceMsg)];
  const uint8_t len = linkPoll(buf, sizeof buf, onCommand);
  if (len) onPacket(buf, len);
  ledsService();
}
