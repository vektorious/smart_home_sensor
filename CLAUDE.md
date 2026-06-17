# Using Claude Code in This Workshop

Claude Code is an AI assistant you run in your terminal. It can read the files in this repo and help you understand and debug the firmware as you build your sensor.

## Getting started

```bash
claude
```

Run that in the repo root. Then just describe what you need in plain language.

## What to ask

**Understanding the code**
- "Explain what `newDataCallback` does in `bme680.ino`."
- "What does the `SensorPacket` struct carry and who uses it?"
- "Why is there a temperature offset and how do I calibrate it?"

**Stuck on a step**
- "I got a linker error mentioning `libalgobsec` — what do I do?"
- "My upload fails even with the BOOT button trick. What else can I try?"
- "The Serial Monitor says `MQTT connecting...` but never `connected`. Help me debug."

**Wiring and hardware**
- "Which GPIO pins does the BME680 use and why should I avoid GPIO16/17?"
- "The I2C scan finds nothing at 0x76 or 0x77 — what should I check?"

**Home Assistant**
- "The device isn't showing up under Settings → Devices. Walk me through what to check."
- "What MQTT topics does this device publish to?"

## Tips

- **Paste error messages.** Claude can't see your screen or serial output — copy the exact text into the chat.
- **Paste your `config.h`.** If something isn't connecting, share the relevant lines (you can redact your Wi-Fi password).
- **Ask follow-up questions.** If an answer is unclear, just say so.

## What Claude can't do

- Flash the board for you — you need the Arduino IDE for that.
- See live sensor output — paste it from the Serial Monitor.
- Access the internet or your Home Assistant — describe what you see and it will help you interpret it.
