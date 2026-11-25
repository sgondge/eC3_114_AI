# Changelog

All notable eCOOK3 firmware releases are documented here. Use this file as the GitHub Release description when publishing a tag.

Firmware behaviour is specified in [eCOOK3_Firmware_Specification.md](eCOOK3_Firmware_Specification.md). Hex files are in [`RELEASE/`](../RELEASE/). This changelog covers official production releases from **v1.1.0** onward: v1.1.0, v1.1.1, v1.1.2, and v1.1.3.

## Release overview

| Release | Date | Commits | Summary |
| --- | --- | --- | --- |
| v1.1.3 | 11 Aug 2026 | - | MQTT duplicate handling, device-time timestamps, Onomondo APN, OTA AP image |
| v1.1.2 | 02 Apr 2026 | 3 | Production release |
| v1.1.1 | 24 Mar 2026 | 37 | OTA & connectivity hardening |
| v1.1.0 | 06 Jan 2026 | 120 | OTA command framework & runtime parameters |

## [1.1.3] - 2026-08-11

**Release Date:** 11 August 2026  
**Firmware version:** `1.1.3`  
**Hardware target:** HT32F54253  
**Compiler:** ARM MDK v5.06 update 7 (build 960)  
**Memory:** High-memory build (127 KB)

### Hex files

| File | Description | Use |
|---|---|---|
| [eC3_113_11082026.hex](../RELEASE/eC3_113_11082026.hex) | Combined image (IAP bootloader + AP application) | First-time flash, factory programming, or recovery. Flash using e-Link32 Pro ICP Tool. |
| [eC3_113_AP_11082026.hex](../RELEASE/eC3_113_AP_11082026.hex) | Application image only (OTA / FOTA payload) | Over-the-air update of devices already running IAP. Do not use as a full-chip first flash. |

Older builds are under [`RELEASE/Archives/`](../RELEASE/Archives/).

### Fixes

- MQTT cooking-payload duplicate handling: do not restart the send cycle while waiting for a publish ACK (`g_check_cook_publish`).
- 15-second debounce on stove-off payload so repeated OFF events do not enqueue duplicate cooking data.
- MQTT connect URC parsing: `AT+CMQTTCONNECT?` query replies are no longer treated as connect failures (only `+CMQTTCONNECT: 0,<err>` with `err != 0` re-enters network init).
- Publish retry/timeout: failed cook publishes are tracked, offset is advanced, and MQTT is torn down on on-demand/once strategies after max retries.
- I2C bus-idle / mains-off detection runs only after a stove I2C master has been seen, so factory mode and battery check in `STATE_ONOFF` are not blocked on boards without stove I2C.
- FOTA download is attempted in `STATE_ONOFF` even when stove plate config (`ecConfig`) is not yet complete.
- SIM ICCID prefix `894573` selects the Onomondo APN (`onomondo`); other SIMs use Aeris (`iot-eu.aer.net`).
- If no valid epoch timestamp is received, cooking session `onT` / `offT` (and related payload timestamps) are published in device-time format. The device does not force conversion to epoch at publish time.

### Known issues / follow-up

- Add a factory-mode firmware feature to enable or disable factory mode (currently entered by `0` + Boost from ON/OFF; no persistent enable/disable control).
- Onomondo connector feature (APN selection is present; connector/integration work remains).
- Replace I2C-based stove UI on/off (mains-loss) detection with 5V-detection hardware pin sensing for display and UI shutdown.
- Support for SMS-based AT command execution in the firmware.

### Test and documentation

Compiled release documentation (hex, notes, and related artefacts):  
[eCOOK3 1.1.3 release folder (Google Drive)](https://drive.google.com/drive/folders/1hUYRFxInqhIWPLr6Tp8LdOpAUbg-_vpy)

| Document | Link |
|---|---|
| DQE summary - 1.1.3 testing | [DQE summary (Google Doc)](https://docs.google.com/document/d/1xFZmAem_gykSr3tZfYiX-0EroP2oor1t/edit?usp=drive_link&ouid=108653612730876455616&rtpof=true&sd=true) |
| Duplication handling test result | [Duplication handling test (Google Drive)](https://drive.google.com/file/d/1U3fxP7Ll3jnefn280XQR2jejBrZYSH9s/view?usp=drive_link) |
| Field test report (1.1.3 / 1.0.7.3) | [113 and 1073 Field Test (Google Sheet)](https://docs.google.com/spreadsheets/d/1NSgtOTT-3NWi368aAQ3K2RUpaVmuc4PFSxYSpVfO9xU/edit?gid=233438698#gid=233438698) |

### Flashing

- **Full flash:** program [eC3_113_11082026.hex](../RELEASE/eC3_113_11082026.hex) using e-Link32 Pro ICP Tool.
- **OTA:** deliver [eC3_113_AP_11082026.hex](../RELEASE/eC3_113_AP_11082026.hex) through the FOTA / OTA path (AP image only).

## [1.1.2] - 2026-04-02

Production release.  (3 commits; release commit df65a53)

- AT+CLBS=0 issued to suppress background CLBS calls that disturbed connectivity.
- CLBS epoch retry added on GSM reset so time sync survives modem restarts.
- ICCID widened from 19 to 20 digits to match SIM provider numbering.

| Commit | Date | Description |
| --- | --- | --- |
| df65a53 | 2026-04-02 | Release Files 1.1.2 and AT+CLBS=0 command added to avoid background CLBS calls |
| 2140e43 | 2026-04-02 | Fix: CLBS epoch Retry on GSM reset |
| ca29a11 | 2026-03-31 | Fix: ICCID changed to 20 digit from 19 digit |

## [1.1.1] - 2026-03-24

OTA & connectivity hardening.  (37 commits; release commit 124331e)

- CLBS bounded to a maximum of 5 attempts; GSMisReadyForAT guard removes initial response delay.
- MQTT connection made persistent to ride out field disconnects; IAP programming routine improved.
- Duplicate-payload fix; power / energy fields (pwrC, Pg, avgPwrW, PgWh) changed from string to float.
- HTTP error 715 resolved; boost + error-plate combination fixed; touch response latency reduced.
- Large batch of concurrency, memory, and EEPROM hardening fixes; FOTA command path added.

| Commit | Date | Description |
| --- | --- | --- |
| 124331e | 2026-03-24 | Release: 1.1.1 and 111_AP file release |
| 1f8e8bc | 2026-03-23 | Fix: CLBS max attempt count is upto 5 times |
| db42d4d | 2026-03-23 | Fix: Add GSMisReadyForAT guard to avoid inital delay in GSM response |
| 6ca9f90 | 2026-03-19 | Fix: systick_delay_us updated |
| 2652ab0 | 2026-03-17 | Fix: duplicate payloads |
| 1b6f652 | 2026-03-17 | Fix: pwrC, pg, avgPwrW, PgWh values changed from string to float |
| b8d85fc | 2026-03-13 | Merge branch 'IAP_improvements' into Patch_1.1 |
| 2cdec9d | 2026-03-13 | Modify: MQTT connection persistent to avoid connection losses observed in the field |
| e3b0e23 | 2026-03-12 | Update: IAP_function updated |
| 1a9bc84 | 2026-03-11 | Update: Added CLBS frequent call guard |
| 4b80456 | 2026-03-06 | Fix: HTTP error 715 resolved |
| 5825d42 | 2026-03-05 | Fix: Boost + Error plate combination, Update failure running animation exit |
| 7fb93fb | 2026-03-04 | Update: delay funciton with __NOP() and added GPTM0_ProcessEvents wherever possible |
| ecb0e89 | 2026-03-02 | Fix: touch response slow issue |
| d87c77c | 2026-02-27 | Update: BatVoltage condition removed and PB2 status added |
| 896376a | 2026-02-27 | Removed compiler warnings |
| 342c4e6 | 2026-02-27 | Fix: Duplicate `epochStr` in main.c ,  checkMQTTConnection uses buffer as nonNUL-term string, RuntimeParams size vs RUNTIME_PARAMS_SIZE (32),  get_first_20_chars dest must be ? 21 bytes, PAYLOAD_ESSENTIAL erase on any +CMQTTPUB: 0,0, storeFailedEraseRest drops sessions if totalBlocks > MAX |
| 39d6e90 | 2026-02-25 | Fix: concurrency  GPTM0.c issue 70 |
| 3876312 | 2026-02-25 | Fix: concurrency  Payload.c issue 36 |
| e34e627 | 2026-02-25 | Fix: concurrency  BFTM0.c issue 71, 69 |
| 2a56303 | 2026-02-25 | Fix: memory  Payload.c issue 32, 33, 34, 35 |
| abd4dcf | 2026-02-25 | Fix: eeprom  EEPROM.c |
| 5998b13 | 2026-02-25 | Fix: eeprom  FSM.c |
| 0fcfcbc | 2026-02-25 | Fix: concurrency  FSM.c |
| 119e489 | 2026-02-25 | Fix: concurrency  TouchFunction.c |
| 40a1c43 | 2026-02-25 | fix: hardcoded serial ID removed |
| 1d313a8 | 2026-02-24 | Fix: Error delayed display Delayed Errors on Display also delayed Fixes #47 |
| bd0c566 | 2026-02-24 | Update: remove comments and batV condition for OTA |
| a2d84d7 | 2026-02-23 | Update: some more FOTA updates |
| ec0b91c | 2026-02-23 | Update: fotaUpdate moved to onoffhandler code |
| 8f5026b | 2026-02-23 | Update: FOTA commands added |
| 797301e | 2026-02-23 | Update: ExitSleepModePrepare and stateOnOffHandler condition update |
| 5995c9f | 2026-02-19 | Fix: IAP download function EOF condition updated |
| ba18ff1 | 2026-02-19 | Fix: Delayed Errors on Display also delayed Fixes #47 |
| 45d720c | 2026-02-19 | Fix: Circular Buffer implementation on 1.1 cooking blocks Fixes #52 |
| 7baedd6 | 2026-02-17 | Update: Rx_Data_Sum reset every loop for code sanity |
| 491f515 | 2026-02-17 | Update: TouchFunction.h is added for clear code |

## [1.1.0] - 2026-01-06

OTA command framework & runtime parameters.  (120 commits; release commit 988ad9a)

- Full OTA command framework: fw, fota, pu, lb, so, mr, rd, rp.
- Runtime parameters persisted to emulated EEPROM; connection-strategy FSM (always_on / on_demand / once).
- Epoch priority CLBS > CCLK > device_time with 24-hour resync; configurable cooking-block count.
- Watchdog timer, base-firmware anti-brick safeguard, IAP region grown to 12 KB.
- Numerous hardfault / null-pointer fixes and factory-mode EOL touch testing.
- Production hex files added (2026-01-06); touchkey response after deep sleep restored.

| Commit | Date | Description |
| --- | --- | --- |
| 988ad9a | 2026-01-06 | Added hex files |
| 3f1ef74 | 2026-01-06 | Merge Fixed |
| c3acb5a | 2026-01-06 | Merge branch 'issue_deepSleepMode' |
| 9f44547 | 2026-01-06 | Fix: Touchkey Response Issue Resolved |
| 906a24c | 2026-01-06 | Fix: Touchkey unresponsive after deepSleep mode |
| c318148 | 2026-01-06 | Update: factory mode touchkey Testing |
| ed7e860 | 2025-12-26 | Fix: EOL touchkey testing in Factory Mode |
| 2ca38b1 | 2025-12-24 | Added & Release: Exception for low power mode timeout to Factory Mode, |
| 1f6f1da | 2025-12-19 | Archives firmware files and prod release file added |
| 49b29ca | 2025-12-17 | Update: fw update version change 1.1.0 |
| 8e4cab2 | 2025-12-17 | Update: XAPI key for fw update |
| c9f3314 | 2025-12-17 | Fix: Multiple SYNC_State looping stuck issue |
| a94cc19 | 2025-12-17 | Update: added running animation and blocking stove operation while entered into fw Update process |
| f0f33ef | 2025-12-17 | Fix: added retry mechanism for MQTT connection |
| 52f5396 | 2025-12-17 | Merge branch 'timestamp_CLBS' |
| 6af7084 | 2025-12-17 | Moved AT+CRESET to only trigger if max retries is crossed |
| d6c342b | 2025-12-17 | Add max retry for publish response in ONOFF state |
| 29c0a30 | 2025-12-17 | Removed CLBS, validity check of both CLBS and epoch if CLBS is synced also sync epoch_time |
| cbe4c12 | 2025-12-17 | Resetting Load Balancer parameters on EEPROM on reset |
| 1bed82c | 2025-12-17 | Add: epoch now only considers CLBS |
| 8474166 | 2025-12-16 | Merge branch 'main' of https://github.com/atecglobal/eCOOK3_firmware_OTA |
| fface01 | 2025-12-16 | Added Publish Response for essential payloads and max retry of 5 for failed publish response |
| 6f695a2 | 2025-12-16 | Fix: timestamp fetching issue |
| 1316ebb | 2025-12-15 | Stack size is increased to 2KB |
| 68e419d | 2025-12-15 | Update: added stdio.h in main |
| 3d0f4a6 | 2025-12-15 | Fix: Boost Timer UI issue |
| 7c5271d | 2025-12-15 | Fix: fw_url_buf fix |
| 30d6bb7 | 2025-12-15 | Update: Pg calculation change with Ag as 0 |
| e0f9265 | 2025-12-15 | Revert: Revert back changes from cJson library to snprintf based payload implementation |
| 384dcbb | 2025-12-15 | Fix: Timestamp CCLBS and CCLK simultaneously, priority is alwats CLBS |
| 620e8d0 | 2025-12-15 | Fix: removed printf debug log, sprintf change to snprintf function, removed Event display from GPTM0, added ecConfig check for HdispSwitch and updateLEDs |
| c180833 | 2025-12-15 | Fix: Session OnTime not synced and 2hr timer falsely executing |
| 48fc1f0 | 2025-12-12 | Update: read parameters from the device OTA command |
| cba6b21 | 2025-12-12 | Update: parameter update on EEPROM |
| 0ae161f | 2025-12-12 | Update: g_params added to EEPROM |
| 0c79b1e | 2025-12-12 | Fix: Add another null pointer exception for hardfault error |
| 9dfd19c | 2025-12-12 | Update: display AtEC if number of plates are not yet synced |
| 43d82e2 | 2025-12-12 | Fix: another hardfault fix due to race condition in UART0 |
| 70f2e6e | 2025-12-12 | Fix: cooking time monitor fixes after error pan is resolved |
| 8087552 | 2025-12-12 | Fix: epoch timer validity check |
| cb0e324 | 2025-12-12 | Fix: keep track of epoch counter during deep sleep cycle, resync epoch every 24Hr |
| 32e5ac4 | 2025-12-12 | Fix: epoch prio CLBS then CCLK then device_time |
| dbe61e9 | 2025-12-11 | Fix: Null value return for parseHexLine as well |
| 40bbeaf | 2025-12-11 | Fix: resolve some hardfault issues with null pointer |
| 6214ce5 | 2025-12-11 | update: cookingTimeMonitor is now changed to deviceTime |
| 0ea6ac8 | 2025-12-11 | Update: cJon used for payload data sent |
| d59f6d5 | 2025-12-11 | Fix: AT commands interfering with Payload, added tx buffer clearing function before start of payload functions |
| 4dedae5 | 2025-12-11 | Update: added "/r/n" after the AT command sent |
| d3673e8 | 2025-12-10 | Update: getimmei function update to avoid hardfault |
| a3c6c92 | 2025-12-05 | FW Version change to 1.1.2 |
| b6a5840 | 2025-12-05 | Update: updated CLBS and CCLK diff should not be more than 86400 seconds i.e. 24 Hours |
| 1367376 | 2025-12-04 | Fix: SYNC green immediately when the device is turned On |
| fad7100 | 2025-12-02 | Update: changes OTA response from "resp" to "resp/{serial_number}" |
| f1f732e | 2025-12-01 | Update: WDT enable, eCooker_Configuration is updated |
| d8c2d17 | 2025-12-01 | Update: monitor PB2 status for dedicated 5V check |
| 8f3ea7f | 2025-12-01 | Update: Added Guard of max Power consumption 3300 for double and 3200 for single, also added guard of max cooking time as 7200 |
| 005474f | 2025-12-01 | Update: optimise code to avoid duplication of CLBS command |
| 6c16d50 | 2025-12-01 | Fix: no need for at+clbs=1 command |
| 1ad0734 | 2025-12-01 | Update: epoch timer priority based on clbs command then cclk command |
| e7b809b | 2025-12-01 | update: g_clbs_epoch and g_cclk_epoch added |
| 69772ba | 2025-11-27 | Update: Sync state handler added to reset the module and intiate MQTT connection again |
| 6ac4c7e | 2025-11-27 | Update: max session duration for payload is capped upto 7200 seconds |
| 48ca167 | 2025-11-27 | Update: if epoch is not synced, use device time as start time as well. |
| cd214d8 | 2025-11-27 | Merge branch 'main' of https://github.com/atecglobal/eCOOK3_firmware_OTA |
| 4546989 | 2025-11-27 | Update: retry max cap for CGSN check upto 5 |
| c314639 | 2025-11-27 | Include max cap in aggregated payload for essential mode |
| 48ba88f | 2025-11-27 | Update: added pointer initialization checks |
| ffdf194 | 2025-11-27 | Update: Removed WDT and commented printf statements |
| 1fc401b | 2025-11-20 | Added payload changes to OTA, now sends all payloads before disconnecting |
| 1a86759 | 2025-11-14 | Added Memory erase function files |
| cbd001f | 2025-11-13 | Fix: Power Level 1 not working after wake up from low power mode #42 |
| 73535c2 | 2025-11-13 | Fixes to prevent Hardfaults at MQTT connection |
| e4b85e8 | 2025-11-12 | Fix: I2c send 0 power level everytime on off state or power level is 0 |
| 90ad450 | 2025-11-11 | Update: Added Cooking Time Updater function |
| ddbfae8 | 2025-11-11 | Modified: sending payloads before disconnecting |
| e110370 | 2025-11-07 | update: fw download url fixed |
| 45e88d2 | 2025-11-07 | Update: GSM Reset when Turned On for First time, UART0 buffer clear before getEpochTime function |
| f8a0e26 | 2025-11-07 | Modification: Changed the payload structure to include Pg in each block and removed stat section from payload |
| 4e61551 | 2025-11-06 | Modification: Added battery charging status to aggregated payload and changed source of Pg computation to come from EEPROM data |
| 7a245d6 | 2025-11-06 | Update: Offset timezone, fixed local timezone issues |
| 77aedff | 2025-11-05 | Update: Added IMEI check before proceeding with EPOCH timer |
| 5ffaea5 | 2025-11-05 | Fixed: PgWh_chunk updated in sendCookingSessionsChunk |
| 15c7115 | 2025-11-05 | Modifications: Added Ag-Vg to EEPROM and computing Pg values from Ag-Vg computation for each chunk |
| 3860d2e | 2025-11-03 | Update: Added Watchdog timer code. (this needs more testing) |
| bc552b0 | 2025-10-30 | Update: code reorganise, battery Check issue resolved Battery Check Clear immediately Fixes #37 |
| ce79ba2 | 2025-10-29 | Update: Double stove, non-selected Power level display update from "E" to existing power level |
| 3c29577 | 2025-10-27 | Update: Change error detection and CRC calculation and cookPayload buffer is reverted back to 384 |
| d712f5a | 2025-10-27 | Update: fw URL is now changed to Dynamic URL, number of cooking blocks is not hardcoded to 2; its dependent on g_params.num_cooking_blocks |
| 7852d42 | 2025-10-27 | Li's Change for UART |
| 6e063ea | 2025-10-27 | Merge branch 'OTA_1_1' of https://github.com/atecglobal/eCOOK3_firmware_OTA into OTA_1_1 |
| bc3bdde | 2025-10-16 | Update: changed fwDownload retry logic if success do not retry |
| f202473 | 2025-10-16 | Update: bftm delay to wait for gsm response for FSDEL command, HTTPREADFILE is changed from fileswitch to fwdownload function |
| 304e7e7 | 2025-10-16 | Added: Number of Cooking Blocks as configurable OTA parameter #39 |
| 73993c1 | 2025-10-13 | Update: FW download status check on display for testing |
| 5adcc40 | 2025-10-10 | Updated EEPROM address location for cooking data |
| e00b22b | 2025-10-09 | Changed Heap size to 8KB |
| 76c430e | 2025-10-09 | Update: OTA running Animation |
| e0d0522 | 2025-10-08 | Update: cookPayload is updated to 384 array size |
| 0d9529f | 2025-10-08 | Update: Added condition to avoid bricking if no Base firmware in GSM module with name "ec3_base_110_08102025_SN.hex" |
| 88d60d8 | 2025-10-08 | Update: Base firmware functionality added |
| 4197418 | 2025-10-08 | Update: Serial ID EEPROM location changed to 11KB |
| 0e57201 | 2025-10-07 | Updated Serial ID EEPROM Address location |
| b3f9173 | 2025-10-07 | Added temporary retry count #33 |
| 8ab228e | 2025-10-06 | Reduced default time for load balancer if duration not passed fix to #30 |
| 839fa83 | 2025-10-06 | Update: Hardfault Handler Reset added |
| c938496 | 2025-10-03 | Merge branch 'OTA_1_1' of https://github.com/atecglobal/eCOOK3_firmware_OTA into OTA_1_1 |
| 286852a | 2025-10-03 | added statement to check for previous CID before execution #29 |
| b47d6e7 | 2025-10-03 | updated topic to "resp" #27 |
| 3dabed8 | 2025-10-01 | Update: from fw 1.0.7 added initGlobalPayload(), sendMultipleErrorPayload(), free session in storeFailedEraseRest() |
| c1c3601 | 2025-10-01 | Update: Closession updated for negative timestamps, removed debug printf statements, BatV in payload |
| 942efef | 2025-09-30 | Updated IAP to 12 kb |
| 6555ce1 | 2025-09-30 | Added Publish Ack handling |
| 7d1fe66 | 2025-09-26 | Update: TKL adjustments to avoid singles stove freeze |
| cc71edc | 2025-09-26 | Revert I2C1 changes |
| 93229c6 | 2025-09-09 | Added Persistent Flag to the connection string as parameter update option #25 |
| ff23313 | 2025-09-09 | Added option to send payloads when connection strategy is once during startup #20 |
| 1033210 | 2025-09-09 | Added default to load balancer timeout #18 |
| a8da91a | 2025-09-09 | Fix for Sync LED showing up in always on in onoff state #17 |
| 058693e | 2025-09-08 | Merge branch 'OTA_1_1' of https://github.com/atecglobal/eCOOK3_firmware_OTA into OTA_1_1 |
| 1ce8226 | 2025-09-08 | Fix for #22 bug removed free(globalPayload.errC) from main causing duplication of freeing memory and Hardfault |

