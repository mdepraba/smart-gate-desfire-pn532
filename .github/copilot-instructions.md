# Copilot instructions: smart-gate-desfire-pn532

Fast-start guidance for contributing to this ESP32 + PN532 (MIFARE DESFire) project.

## Big picture
- Main app lives in `src/smart-gate-sketch.cpp` (Arduino sketch for ESP32).
- PN532 driver (`PN532.cpp/.h`) handles transport (SPI/I2C), wake/ACK, timeouts, and packet framing.
- DESFire protocol (`Desfire.cpp/.h`) implements authentication, CMAC/session keying, file ops (read/write, settings), and error/status handling.
- Crypto helpers (`AES128.*`, `DES.*`) supply block ciphers used by DESFire.
- Utilities (`Utils.*`, `Buffer.h`) provide low-level IO, logging, buffers.
- Secrets/config (recommended): put `Secrets.h` in `include/` with keys and AIDs used by the app.

## Build, flash, debug (PlatformIO)
```bash
pio run
pio run -e esp32dev -t upload
pio device monitor -e esp32dev -b 115200
pio test
```

## Project-specific patterns
- Algorithm selection is controlled by the compile-time macro `USE_AES` (true = AES-128, false = DES/3DES). Key sizes and auth flows branch on this.
- Runtime flow: init PN532 (SPI pins via `InitHardwareSPI`), `SamConfig`, detect card (`ReadPassiveTargetID`), select app (`SelectApplication`), then authenticate (`Authenticate`) and read data (`ReadFileData`).
- Robust reading example: `ReadFileWithAutoAuth(...)` attempts no-auth read, then app key 0, then file key index 3 with a zero key. Mirror this pattern when adding new file operations.
- Error handling: PN532 errors are decoded in `PN532::CheckPN532Status`; DESFire errors via `Desfire::CheckCardStatus`. Prefer these over ad-hoc checks.
- EEPROM user records: `UserManager.h` defines on-device storage layout; changing `NAME_BUF_SIZE` requires clearing EEPROM.

## Integration points
- Hardware pins are set in `smart-gate-sketch.cpp` (`PN532_SS`, `PN532_RST`, `LED_PIN`). Update here when wiring changes.
- App secrets in `include/Secrets.h` (not committed): define `SECRET_PICC_MASTER_KEY`, `SECRET_APPLICATION_KEY`, `CARD_APPLICATION_ID`, `CARD_FILE_ID`, `CARD_KEY_VERSION`. The app passes key bytes to AES/DES via `SetKeyData`.
- Extra Arduino libraries can be discovered via `lib_extra_dirs` in `platformio.ini`.

## Safety notes
- Changing master/app keys on real cards can be irreversible. The code assumes zeroed default keys in some flows—double-check `Secrets.h` before flashing.
- Do not include `Secrets.h` in library sources; only the sketch should depend on secrets.

## Files to read first
`platformio.ini`, `src/smart-gate-sketch.cpp`, `src/PN532.cpp`, `src/Desfire.cpp`, `include/Secrets.h` (local), `test/README`.

If any section is unclear or you need additional examples (e.g., creating an app/file or key rotation flow), say which part to expand and I’ll refine this doc.# Copilot instructions for smart-gate-desfire-pn532

Short, actionable guidance to get productive in this repo (ESP32 + PN532 Desfire reader).

- Purpose: ESP32 Arduino app that uses a PN532 reader to talk to DESFire/EV1 cards. Main runtime is in `src/smart-gate-sketch.cpp`.
- Build system: PlatformIO. See `platformio.ini` (env: `esp32dev`).

Key commands (run in project root):

- Build: `platformio run`
- Upload to device (esp32dev): `platformio run -e esp32dev -t upload`
- Open serial monitor (115200): `platformio device monitor -e esp32dev -b 115200`
- Run unit tests (PlatformIO test runner): `platformio test` (see `test/README`)

Important files and why they matter

- `src/smart-gate-sketch.cpp` – application entrypoint and the best single-file overview of runtime flow: reader init, main loop, card detection, authentication flow (AuthenticateDesfire, ReadFileWithAutoAuth).
- `src/PN532.cpp`, `src/PN532.h` – low-level PN532 driver (SPI/I2C). Look here for timing, ACK, and packet parsing details (timeouts and checksum checks are implemented here). Use this when debugging communication issues.
- `src/Desfire.cpp`, `src/Desfire.h` – DESFire protocol and session management (authentication, file read/write, CMAC handling). This is where session key derivation and CMAC/crypt operations live.
- `src/Secrets.h` – repository secrets/constants: `SECRET_PICC_MASTER_KEY`, `SECRET_APPLICATION_KEY`, `SECRET_STORE_VALUE_KEY`, `CARD_APPLICATION_ID`, `CARD_FILE_ID`, `CARD_KEY_VERSION`. Treat as sensitive and be careful when modifying. The code contains comments warning about irreversible card changes.
- `src/UserManager.h` – EEPROM user storage layout and constraints (struct `kUser`, `NAME_BUF_SIZE`). Changing `NAME_BUF_SIZE` alters on-disk layout and requires clearing EEPROM.
- `lib/` and `include/` – local libraries and headers. PlatformIO compiles `lib/*` as static libraries.

Repo-specific patterns and gotchas

- Compile-time AES vs DES selection: `#define USE_AES true/false` is used in `src/smart-gate-sketch.cpp`. Many auth paths branch on this macro and key sizes differ (AES uses 16 bytes, DES/3DES use 8/24 bytes).
- Default master-key behavior: the code assumes factory default PICC master key (all zeros) in many helper flows. `Desfire::Authenticate` and `ReadFileWithAutoAuth` show fallback strategies (try read without auth, then app key 0, then key index 3 default zero key). Use these functions as canonical examples for error handling.
- Card/application IDs: `CARD_APPLICATION_ID` must be non-zero. Changing this affects what application the code selects on cards.
- EEPROM layout and alphabetic insert: `UserManager::StoreNewUser` inserts users alphabetically and expects fixed-size records. When testing modifications that change struct sizes, clear the EEPROM (code comment: CLEAR command / Reset) before reuse.
- Debugging: `PN532::SetDebugLevel` and many `Utils::Print` statements are the first stop for runtime troubleshooting. The PN532 driver has its own timeout constants and readable error messages (see `PN532::CheckPN532Status`).

Integration points

- Hardware: SPI pins are defined in `src/smart-gate-sketch.cpp` (`PN532_SS`, `PN532_RST`) and passed to `gi_PN532.InitHardwareSPI(...)`. When changing wiring, update those defines.
- Crypto: `Desfire` relies on `AES`/`DES` helper classes present in `src` files. Session keys and CMACs are computed inside `Desfire` and must be kept in sync—do not reimplement the CMAC layering without reading `DataExchange` and CMAC logic.

What to change safely (low-risk)

- Small fixes and refactors inside `Utils.cpp/h` or print/log improvements.
- Unit tests under `test/` to cover small helper functions (PlatformIO test runner).

What is high-risk or irreversible

- Changing `SECRET_PICC_MASTER_KEY` or running key-change code on real cards — the repository contains warnings: you can permanently lock or brick cards if the master key is lost.
- Changing `CARD_APPLICATION_ID` or file IDs on production cards without re-provisioning cards.
- Changing `NAME_BUF_SIZE` in `UserManager.h` without clearing EEPROM.

If you need more context or want this file tweaked (shorter/longer or with extra examples), tell me which sections to expand or remove.

References (start here):

- `platformio.ini`
- `src/smart-gate-sketch.cpp`
- `src/PN532.cpp`, `src/Desfire.cpp`, `src/Secrets.h`, `src/UserManager.h`
- `lib/README`, `include/README`, `test/README`
