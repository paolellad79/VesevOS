# Script di esempio (.vsb)

Script VesevOS senza pagina, per `/scripts` sulla scheda. Hanno le funzioni di base (`led()`, `btn()`, `time()`, `mqtt()`, `logtail()`...). Del Bluetooth hanno solo `ble_status()` e, se li lancia l'Admin, `ble_off()` e `ble_passkey()`. Solo ASCII.

- `ble.vsb` - stato del Bluetooth e codice di abbinamento (`script run ble`, solo Admin).
- `ble-off.vsb` - spegne il Bluetooth (`script run ble-off`, solo Admin).
- `torcia.vsb` - un click del tasto BOOT accende il LED bianco, il click dopo lo spegne.

Come si caricano e si provano: vedi `../LEGGIMI.md`.
