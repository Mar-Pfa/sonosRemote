## Repo overview

Small Arduino/ESP (ESP32/ESP8266/Arduino WiFiNINA) project that implements a lightweight Sonos UPnP remote. Main sources live in the `sonos/` folder. Primary entrypoint for the device is `sonos/sonos.ino`. The library implementation is in `sonos/SonosUPnP.*` with a small XML "xpath" parser in `sonos/MicroXPath_P.*`.

Key ideas an agent should know:
- This is a firmware project for microcontrollers (Arduino-style). Edits should preserve memory/stack usage and avoid heavy dynamic allocation.
- The code interacts directly with Sonos devices over UPnP (port 1400) using raw HTTP/SOAP messages built manually in `SonosUPnP.cpp`.
- `MicroXPath_P` is a tiny, stream-based XML navigator used to parse SOAP responses in a memory-friendly way; many routines call `xPath.reset()` and then stream bytes via `ethClient_*` helpers.

Files of interest
- `sonos/sonos.ino` — device startup, WiFi, deep-sleep behaviour, OTA, and simple button-driven actions (volume up/down, play/pause). Good place to see the runtime flow and hardware pins.
- `sonos/SonosUPnP.h` / `sonos/SonosUPnP.cpp` — core UPnP client implementation. Look here for how SOAP messages are constructed (p_* PROGMEM constants) and how `upnpPost`, `upnpGetString`, and `ethClient_xPath` are used.
- `sonos/MicroXPath_P.h` / `sonos/MicroXPath_P.cpp` — a compact XML parser. It's stateful and stream-driven; callers expect to feed it one character at a time while reading the network response.

Architecture & data flow (short)
- Runtime: `sonos.ino` configures WiFi/mDNS/OTA and listens for button presses. When an action is required it calls `SonosUPnP` APIs (e.g. `increaseVolume`, `togglePlayback`).
- Network layer: `SonosUPnP` opens a TCP connection with `WiFiClient` to a Sonos IP on port 1400 and writes raw HTTP/SOAP requests. Responses are parsed in-stream using `MicroXPath_P` methods (via `ethClient_xPath` / `ethClient_xPath2`).
- UPnP services are grouped into constants (AVTransport, RenderingControl, DeviceProperties) — `getUpnpService` and `getUpnpEndpoint` map message types to endpoints.

Conventions & patterns specific to this repo
- PROGMEM strings: many SOAP/UPnP constants are stored in program memory via `PROGMEM` wrappers and helper `ethClient_write_P`. When adding strings, follow the existing pattern (declare PGM_P constants in the .cpp/.h files).
- Memory buffers: functions pass caller-owned char buffers (e.g. `getTrackURI(..., char *buf, size_t size)`). Avoid returning pointers to local buffers. If adding helpers, follow the same ownership pattern.
- Low-level networking: `upnpPost()` performs the connect/write/read-wait cycle; many higher-level functions call `upnpPost` then `ethClient_xPath` to stream-parse the result. Keep changes compatible with this two-step pattern.
- Write-only mode: the `SONOS_WRITE_ONLY_MODE` compile flag is used to disable read/parse logic. Respect conditional compilation when editing shared files.

Developer workflows (flash / build / debug)
- This is an Arduino-style project. Typical workflows developers use here:
  - Arduino IDE: open the folder containing `sonos.ino` and select the board (ESP32/ESP8266 or MKR/WiFiNINA board). Set the SSID/password in `sonos.ino` before first flash.
  - arduino-cli: use `arduino-cli compile --fqbn <board_fqbn> .` and `arduino-cli upload -p <port> --fqbn <board_fqbn> .` from the `sonos/` directory.
  - PlatformIO: open this folder as a PlatformIO project, set `platformio.ini` with your board and upload.

Non-obvious tips / gotchas for agents
- Deep sleep + WiFi: `sonos.ino` toggles WiFi and Bluetooth off before `esp_deep_sleep_start()`. When changing networking code, ensure the code remains friendly to deep-sleep cycles and wake-pin handling.
- OTA: `ArduinoOTA.begin()` is only called while device is awake. If you change OTA behavior be aware OTA is not available while in deep-sleep.
- Network timeouts: `upnpPost()` waits up to `UPNP_RESPONSE_TIMEOUT_MS` for available bytes. When adding blocking operations, consider this timeout and avoid long synchronous delays that block parsing.
- XML parsing: `MicroXPath_P::getValue()` expects callers to stream characters until end-tag; don't buffer whole responses unnecessarily — preserve streaming parsing approach.
- PROGMEM handling: `ethClient_write_P()` splits large PROGMEM strings into chunks. When adding large PROGMEM text, test that it is emitted correctly.

Examples to copy when implementing similar features
- Create a new getter that parses a simple SOAP field:
  - Call `upnpPost(speakerIP, UPNP_AV_TRANSPORT, p_GetPositionInfoA, ...);`
  - `xPath.reset();` then `PGM_P path[] = { p_SoapEnvelope, p_SoapBody, p_GetPositionInfoR, p_Track };`
  - Call `ethClient_xPath(path, 4, buffer, sizeof(buffer));` then `ethClient_stop();`

What a helpful agent should avoid doing
- Do not introduce heap-heavy STL usage or dynamic memory (std::string, vectors) that will bloat microcontroller binaries.
- Avoid changing the stream-based parsing model to a full-buffer model without tests on memory-constrained boards.

Useful anchors for future edits
- For UPnP SOAP templates and tags: `sonos/SonosUPnP.h` (p_* and SONOS_TAG_* constants).
- For network send/receive helpers: `SonosUPnP::upnpPost`, `ethClient_write_P`, `ethClient_stop`.
- For XML parsing: `MicroXPath_P::setPath`, `getValue`, and the wrapper `SonosUPnP::ethClient_xPath2`.

If anything in this file is unclear or you want more specifics (preferred board FQBN, CI, or test hardware available), tell me what to add and I will iterate.
