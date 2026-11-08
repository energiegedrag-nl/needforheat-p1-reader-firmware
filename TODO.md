# TODO – needforheat-p1-reader-firmware

Open work for this repository, distilled from the old NeedForHeat Trello board, including the discussions on the cards (paraphrased and regrouped) and from the 2026 maintenance review.

Items that touch several repositories are written out in full in one *lead* repository; the others list them with a link (→). Keep the details up to date in the lead repository only.

## Now: needed to run at home this winter

- [ ] → Release workflow without the Windesheim signing key — see [needforheat-generic-firmware](https://github.com/energiegedrag-nl/needforheat-generic-firmware/blob/main/TODO.md#unsigned) *(Trello #480)*

## Soon: robustness and correctness

- [ ] <a id="lg"></a>**Landis+Gyr meters.** Several Landis+Gyr meters (E360 series) do not work; one source mentions a non-standard 115200 7E1 serial setting. Find out what is needed in firmware or hardware. Pointers: experiences with the E360 and the Stegen Sensorbox on Tweakers; smart-stuff.nl sells P1 bridges and splitters and documents them openly. Also affects: needforheat-p1-base-hardware. *(Trello #971, #1092)*
- [ ] → Make OTA updates trustworthy and staged — see [needforheat-generic-firmware](https://github.com/energiegedrag-nl/needforheat-generic-firmware/blob/main/TODO.md#ota) *(Trello #347, #770)*
- [ ] → Upgrade to ESP-IDF 5.5 (WPA3, Wi-Fi 6) — see [needforheat-generic-firmware](https://github.com/energiegedrag-nl/needforheat-generic-firmware/blob/main/TODO.md#idf) *(Trello #806)*
- [ ] → Let generic and device-specific code share buttons — see [needforheat-generic-firmware](https://github.com/energiegedrag-nl/needforheat-generic-firmware/blob/main/TODO.md#buttons) *(Trello #833)*
- [ ] → Show device status on the e-ink screen — see [needforheat-generic-firmware](https://github.com/energiegedrag-nl/needforheat-generic-firmware/blob/main/TODO.md#einkstatus) *(Trello #303, #867, #1218, #499)*
- [ ] → Stable Bluetooth presence detection — see [needforheat-presence-detector-firmware](https://github.com/energiegedrag-nl/needforheat-presence-detector-firmware/blob/main/TODO.md#btstab) *(Trello #222, #769)*

## Later: features and ideas

- [ ] <a id="gateway"></a>**Review features of the old P1 gateway.** Check which features of the earlier gateway are still worth adding. *(Trello #760)*
- [ ] → All device-type settings in platformio.ini — see [needforheat-generic-firmware](https://github.com/energiegedrag-nl/needforheat-generic-firmware/blob/main/TODO.md#config) *(Trello #695, #510)*
- [ ] → Finish the rename from Twomes to NeedForHeat — see [needforheat-generic-firmware](https://github.com/energiegedrag-nl/needforheat-generic-firmware/blob/main/TODO.md#rename) *(Trello #1030)*
- [ ] → CONTRIBUTING.md with the branching workflow — see [needforheat-generic-firmware](https://github.com/energiegedrag-nl/needforheat-generic-firmware/blob/main/TODO.md#contrib) *(Trello #394, #1032)*
- [ ] → Fix links and errors in device manuals — see [needforheat-manuals](https://github.com/energiegedrag-nl/needforheat-manuals/blob/prd/TODO.md#manlinks) *(Trello #718, #995, #719)*

## Recently done

- [x] Erroneous gas readings are skipped (#400)
