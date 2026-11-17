# Aiuto del comando `led_rgb1`

led_rgb1 mostra lo stato del LED. led_rgb1 trigger [nome] mostra o cambia che cosa lo accende: none, state (colore dello stato di rete), heartbeat, flash (un lampo a ogni accesso alla flash o alla microSD), wifi (un lampo a ogni pacchetto), cpu (il colore vira con il carico alto), default-on (colore fisso).

led_rgb1 color RRGGBB e led_rgb1 brightness 0-255 cambiano colore e luminosita. led_rgb1 color <sorgente> RRGGBB cambia il colore di flash-read, flash-write, wifi-rx, wifi-tx o cpu; led_rgb1 threshold 10-95 e la soglia di carico del trigger cpu. Cambiare trigger, sorgenti e soglia e solo per l'Admin.

I lampi sono al massimo 3 al secondo. Di fabbrica il trigger e state, come sempre. Le vecchie forme led_rgb1 state|heartbeat|fixed|off e i nomi led-color e led-bright restano validi (le regole li usano). Il nome vecchio led non vale piu.

vesevos-led_control e un alias di led_rgb1: stesso comando, stessi ruoli, stessi argomenti (vesevos-led_control trigger flash).

## Esempi

```
led_rgb1
led_rgb1 trigger flash
led_rgb1 color flash-write ff8800
led_rgb1 threshold 80
```
