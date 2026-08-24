# Flowis — komponenty `flowis_radio` + `flowis_meter`

Obsługa modułów radiowych Flowis (METERING Anna Moder) w ESPHome, na CC1101.

## Dlaczego to nie jest driver `wmbus_common`

Flowis **nie jest wM-Bus**. Nie ma warstwy DLL (pola C/M/A/CI), nie ma rekordów
DIF/VIF, nie ma szyfrowania AES. To własny, bardzo prosty protokół:

```
preambuła 0xAA…  |  sync D391 D391  |  L  |  payload  |  CRC16
```

Sterowniki `wmbus_common` (`amiplus`, `izar`, `apator162`…) dostają na wejściu
sparsowany telegram wM-Bus. Flowis nie da się do tego wpiąć — brakuje wszystkich
warstw, na których te sterowniki operują. Dlatego to osobny komponent, celowo
bez zależności od `wmbus_common` (inaczej ciągnąłby ~100 driverów).

## Urządzenie dedykowane

Ten firmware obsługuje **wyłącznie Flowis**. `wmbus_radio` i `flowis_radio`
konfigurują CC1101 wykluczająco (sync `543D` vs `D391`, tryb sync 16/16 vs
30/32, kodowanie 3-of-6 vs brak), więc jeden układ nie obsłuży obu protokołów
naraz. W `flowis.yaml` nie ma `wmbus_radio` ani `wmbus_common` — pinout CC1101
jest identyczny jak w `cc1101rtl.yaml`, więc ten sam sprzęt (Lolin S3 Mini)
wgrywa się bez żadnych przeróbek.

## Warstwa fizyczna

| Parametr | Wartość |
|---|---|
| Modulacja | 2-FSK, PCM, bez kodowania liniowego |
| Bitrate | 100 kbit/s (nominalny okres bitu 10 µs) |
| Sync word | `D391 D391` (32-bit, tryb CC110x „sync ×2”) |
| Nośna | **do potwierdzenia** — domyślnie 868.000 MHz |
| Szyfrowanie | brak, ramki jawne |

Nośna to jedyny parametr, którego nie da się wyprowadzić z samych ramek.
Użytkownicy modułów Flowis raportują 868.0 MHz i taka jest wartość domyślna. Jeśli po wgraniu nic nie przychodzi — patrz „Szukanie nośnej”
niżej.

### Szukanie nośnej

`flowis_radio` ma metodę `retune()`, która przestraja układ i restartuje odbiór
w locie. Wystarczy dorzucić do YAML-a interwał przemiatający pasmo i zostawić
na kilkanaście minut z logiem na DEBUG:

```yaml
interval:
  - interval: 30s
    then:
      - lambda: |-
          static const float f[] = {868.000, 868.300, 868.500, 868.950, 869.525};
          static int i = 0;
          i = (i + 1) % 5;
          id(flowis).retune(f[i]);
          ESP_LOGW("scan", "Nasluch na %.3f MHz", f[i]);
```

Kanał, na którym pojawią się wpisy `FLOWIS 0x…`, wpisz na stałe w
`frequency:` i usuń interwał. Filtr kanałowy jest ustawiony na 325 kHz, więc
odbiór złapie się też przy rozjeździe rzędu ±100 kHz — krok 200–300 kHz w
skanie w zupełności wystarczy.

### Strategia odbioru

Flowis liczy `L` bez bajtów CRC, więc tryb zmiennej długości CC1101 ucinałby
ramkę o dwa bajty. Zamiast tego radio pracuje w trybie stałej długości
`PKTLEN = 62` z dołączanymi bajtami statusu — dokładnie jedno pełne FIFO (64 B).
Konsekwencje:

* po zapełnieniu FIFO układ sam schodzi do IDLE, więc **przepełnienie RX jest
  niemożliwe** niezależnie od tego, jak długo `loop()` jest zablokowany przez
  WiFi czy API — nie potrzeba osobnego tasku FreeRTOS,
* `GDO0` jest ustawiony na próg = pełne FIFO, więc pętla główna sprawdza jeden
  pin zamiast odpytywać SPI,
* RSSI jest per-ramka, z bajtu statusu, a nie z rejestru odczytanego po fakcie,
* ogon po ramce (Flowis ma max ~31 B) to szum, który odcina `L` i CRC.

Koszt: po każdej ramce radio jest głuche do następnego przebiegu `loop()`
(zwykle <20 ms). Przy nadawaniu co ~2 s to nieistotne.

## Format ramki

```
 offset  0    1     2..5      6      7..10     11..13   14   15  16 17  18   19..
        [L] [typ] [ID LE32] [flagi] [zegar]  [obj. l]  [bf] [al]  --  [A5] [archiwum]
```

CRC-16, `poly=0x8005`, `init=0xFFFF`, big-endian, liczone od bajtu `L`
włącznie. Zweryfikowane na 8/8 ramkach z Twojego logu.

### Typ 1 — odczyt bieżący

* `[11..13]` objętość w litrach, LE24
* `[14]` licznik przepływu wstecznego, `[15]` bity alarmu
* bit `0x40` w `[6]` sygnalizuje obecność bloku archiwum
* archiwum: LE32 w litrach na **czterech ostatnich bajtach payloadu**
  (`[L-3..L]`) — pozycja jest zmienna, dlatego liczona od końca

### Typ 2 — historia zużycia

Publicznie dostępne dekodery ten typ pomijają. Tutaj jest zdekodowany:
`[6..19]` to **7 × uint16 LE w litrach**.

* slot 0 = zużycie od ostatniego zapisu archiwalnego
* sloty 1..6 = sześć poprzednich okresów, od najnowszego

Potwierdzenie: dla trzech różnych liczników `slot 0 == objętość − archiwum`
co do litra (1291, 9351, 7562 l). Długość okresu nie jest potwierdzona —
rzędy wielkości (1,5–18 m³) wskazują na miesiąc, ale to wymaga próbek z dwóch
różnych miesięcy.

### Rok i czas letni — pułapka

Odczytanie roku jako 6 bitów (`b[10] >> 2`) daje na ramkach z sierpnia 2026
**2058**. Faktyczny podział 32-bitowego słowa zegara:

```
bity  0..5   sekunda
bity  6..11  minuta
bity 12..16  godzina
bity 17..21  dzień
bity 22..25  miesiąc
bity 26..30  rok (+2000)
bit  31      flaga czasu letniego
```

Bit 31 to flaga czasu letniego, nie najstarszy bit roku. Na próbkach zimowych
bit jest wyzerowany, więc błąd się nie ujawnia i 6-bitowy odczyt wygląda
poprawnie — dopiero latem wychodzi `58 − 32 = 26`. Ten komponent dekoduje
5-bitowy rok i wystawia flagę osobno.

### Nierozpoznane pola

* `[16..17]` zawsze `0000`
* `[18]` zawsze `0xA5` — znacznik końca bloku podstawowego
* `[19..21]` w dłuższych ramkach `40 00 01`
* trójka `07 XX 00` bezpośrednio przed archiwum; `XX` ∈ {CF, F6, F7} i różni się
  między licznikami — możliwy licznik dni/okresów, za mało próbek

## Dostępne pola

**`sensor`:** `total_m3`, `archive_m3`, `alarm`, `backflow`, `rssi_dbm`,
`period_0_m3` … `period_6_m3`

**`text_sensor`:** `device_time`, `telegram_type`, `telegram_hex`

Sensor nie publikuje nic, gdy dana ramka nie niesie jego pola. Licznik przeplata
telegramy typu 1 i 2, więc publikowanie zera co drugą ramkę rozwaliłoby
statystyki długoterminowe w HA.

## Instalacja

Skopiuj `components/flowis_radio/` i `components/flowis_meter/` do
`smartinhome/esphome-components`, zacommituj i wgraj `flowis.yaml`:

```bash
cp -r components/flowis_radio components/flowis_meter <repo>/components/
cd <repo>
git add components/flowis_radio components/flowis_meter
git commit -m "Sterownik Flowis"
git push
```

`flowis.yaml` wskazuje już na `github://smartinhome/esphome-components@main`
z `refresh: 0s`, więc po pushu wystarczy przebudować.

## Status

* parser: przetestowany na 8/8 ramkach z logu, wolumeny i CRC zgodne co do bitu
* odrzucanie szumu: 3 fałszywe akceptacje na 10⁶ losowych buforów (sam CRC,
  bez filtra po `meter_id` i bez 30-bitowego dopasowania sync)
* warstwa RF: **nie przetestowana na sprzęcie** — rejestry wyliczone, nie
  zmierzone. Nośna i dewiacja mogą wymagać korekty.
