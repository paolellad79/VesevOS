# Aiuto del comando `ping`

Invia pacchetti ICMP echo a un indirizzo IPv4 (o a un nome) e scrive le risposte come il ping classico: tempo, TTL, perdita.

-c pacchetti (4, al massimo 20), -i secondi tra i pacchetti (1, minimo 0.2), -W attesa della risposta (2, al massimo 5), -s byte di dati (56, da 16 a 1024), -t TTL (64).

Durata massima 30 s dalla seriale (Ctrl+C ferma) e 12 s dalla pagina web. Un solo host, solo IPv4; broadcast, multicast e 127.x non ammessi.

I nomi .local usano mDNS (se il servizio e acceso), gli altri il DNS. I tempi hanno la precisione di 1 ms. Solo Admin: genera traffico di rete.

## Esempi

```
ping 192.168.1.1
ping -c 3 -i 0.5 stanza.local
time ping -c 2 192.168.1.1
```
