# 91546LED — entschlüsseltes Lichtprotokoll für RC-Autos

Beide Datenprotokolle der **91546LED**-Lichtplatinen für RC-Autos, entschlüsselt und
verifiziert. Diese Platinen stecken im **ZLL SG216** (1/16 Drift) und vermutlich in
weiteren Modellen mit demselben Lichtsatz.

Der Siebdruck lautet `91546LED-A`, `-B` und `-F`. Der Herstellername **Runchen** steht
nur auf der `-F` — die anderen zwei tragen allein die Typnummer. `91546LED` ist damit
die verlässliche Kennung.

Wer seine Fernbedienung verloren hat, den Fahrtregler getauscht hat oder die Lichter von
einem Mikrocontroller statt vom Originalempfänger ansteuern will, findet hier alles
Nötige. Eine Tabelle braucht man nicht — die Übersetzung zwischen den beiden Protokollen
ist eine Formel.

**Stand:** entschlüsselt, nachgebaut und an arbeitender Hardware verifiziert. Beide
Protokolle werden von einem ESP32 erzeugt, alle 36 Effekte kommen korrekt heraus.

---

## Die Kette

    Empfänger ──> 91546LED-F ──> 91546LED-B ──> 91546LED-A ──> LEDs

| Platine | Funktion |
|---|---|
| `Runchen 91546LED-F V1.1` | Spannungsregler. Treibt die dauerhaft leuchtenden Rückleuchten und das Polizeilicht. **Leitet das Datensignal unverändert durch.** |
| `91546LED-B V1.1` | **Übersetzer und Verteiler.** Wandelt das Empfängerprotokoll in das `-A`-Protokoll. Ausgänge `L1 + − L2`, `R+` und neutrale. |
| `91546LED-A V1.1` | Trägt die LEDs (eine weiße plus roter Ring) und **erzeugt die Animationen selbst.** |

Auf `-F` und `-B` sitzt je ein **78M05**-Linearregler.

**`-F` und `-B` können komplett entfallen**, wenn man die `-A` direkt ansteuert — siehe
[Direktbetrieb der -A](#direktbetrieb-der--a).

### Die Ausgänge der -B sind kanalgebunden

Beschriftet mit `L1 + − L2`, `R+` und neutralen Anschlüssen ohne L/R. Was an `R+` hängt,
**blinkt beim rechten Blinker zwangsläufig mit.** Blinkt eine Leuchte unerwartet mit,
zuerst den Ausgang prüfen, nicht den Code.

---

## Protokoll 1: Empfänger → -B

| | |
|---|---|
| Ruhepegel | **HIGH** |
| Einheitszeit | **1062 µs** |
| Aufbau | 9 Symbole, jedes 1 oder 2 Einheiten, Summe **11 Einheiten** |
| Code | Positionen der beiden **langen** Symbole |
| Rahmenperiode | **116478 µs** (8,585 Hz) |
| aktiver Teil | 11,68 ms |

Dieses Protokoll überträgt **Impulsbreiten, nicht ein Bit je Einheitszeit.** Genau zwei
der neun Symbole sind lang, ihre Positionen tragen den Modus.

Ein Impuls von 3 oder mehr Einheiten ist ungültig — die Platine verwirft den Rahmen.
Daraus folgen C(9,2) = **36 gültige Rahmen**.

Kabelbelegung am Stecker: **weiß = GND, rot = Signal, schwarz = +8 V.** Das entspricht
keiner Konvention. Nachmessen, nicht nach Farbe raten.

---

## Protokoll 2: -B → -A

| | |
|---|---|
| Datenleitung | **Pin 1** (`L1` auf der `-B`, `1` auf der `-A`) — Pin 2 führt nichts |
| Ruhepegel | **LOW** |
| Einheitszeit | **509 µs** (bei 10 MS/s gemessen: 510,2 µs) |
| ein Bit | 4 Einheiten: `H3 L1` = 1, `H1 L3` = 0 |
| Rahmen | 8 Bit plus Abschluss-H von 1 oder 3 Einheiten |
| Rahmenperiode | 117 ms, wie am Eingang |
| High-Pegel | **3,657 V** — nicht 5 V |

Also invertierter Ruhepegel, halbierte Einheitszeit, andere Symbolbreiten, gleiche
Periode. Das Verhältnis der Einheitszeiten ist 1062/509 = **2,087**, also fast genau 2:1
— vermutlich derselbe Grundtakt mit anderem Teiler.

**Pin 2 ist verzichtbar.** Am arbeitenden System nachgewiesen: Pin 2 zwischen `-B` und
`-A` wurde aufgetrennt, mehrere Effekte liefen korrekt weiter.

### Die Übersetzungsformel

Aus den beiden langen Positionen `(a,b)` des Eingangsrahmens:

    Bit 1        immer 1                           Startmarke
    Bit (2+a)    gesetzt
    Bit (2+b)    gesetzt, falls b <= 7
    Abschluss-H  3 statt 1 Einheit, falls 6 oder 7 in (a,b)

Geprüft gegen alle 36 gemessenen Rahmen: **0 Abweichungen.** Eine Referenzumsetzung
liegt in [`example/translate.c`](example/translate.c).

**Grenze des Formats:** Das Datenfeld hat nur 8 Bit. `(6,7)` und `(6,8)` erzeugen deshalb
denselben Code `100000011` — die `-A` kann sie prinzipiell nicht unterscheiden. Die 36
Eingangsrahmen erreichen nur **35** verschiedene Codes.

### Keine versteckte Feinstruktur

Getriggerte Aufnahme mit **10 MS/s** (0,1 µs Auflösung): im 1,64-ms-Fenster nur drei
Lauflängen, kein Impuls unter 50 µs außer dem Triggerpunkt. Die Flanken sind glatt, es
gibt keine versteckten Synchronisationsimpulse. Das Signalmodell ist vollständig.

---

## Der AUS-Code: `000000000`

Ein Rahmen **ohne Startbit** — acht Nullbits, jedes als `H1 L3` — wird von der `-A` als
ungültig erkannt und schaltet sie **sofort** dunkel, nicht erst nach einer
Zeitüberwachung.

Das ist der Schlüssel zu beliebigem Blinken. Jeder der 35 Effekte lässt sich gegen
Schwarz takten, bis hinunter zu einem Rahmen (116,5 ms) je Phase.

**Über die `-B` gibt es kein Aus.** Getestet mit beiden Arten von ungültigem Rahmen:

| Eingang der `-B` | Verhalten |
|---|---|
| **gültiger** Rahmen | sendet den zugehörigen Code |
| **ungültiger** Rahmen, Signal vorhanden | hält den letzten Modus und **sendet weiter** — Lichter bleiben an |
| **kein Signal**, Leitung still | stellt das Senden ein → `-A` geht nach eigener Zeitüberwachung (> 349 ms) aus |

Geprüft mit je vier Wechseln à 5 s:

- **`11001011010`** — kein Rahmenanfang (beginnt mit High, alle Symbolbreiten gültig).
  Lichter blieben an.
- **`01111011010`** — enthält einen Impuls von **4 Einheiten**, also ein ungültiges
  Symbol. Lichter blieben ebenfalls an.

**Wichtige Folge für eigene Firmware:** Wer durch Stilllegen der Leitung blinkt, sieht
kurze Dunkelphasen nicht — die `-A` braucht über 349 ms Stille, bis sie ausgeht. Statt
dessen den AUS-Code senden, der wirkt innerhalb eines Rahmens.

---

## Nichts speichert den Modus

Der Modus wird nirgends gehalten. Bleibt der Eingang still, hört die `-B` auf zu senden
(Pin 1 bei 0,002 V, null Flanken, viermal gemessen) und die `-A` geht aus.

**Der Controller muss den Rahmen dauerhaft alle 117 ms wiederholen.**

---

## Direktbetrieb der -A

    GPIO ──> Pin 1 der LINKEN  -A      Daten, 3,3 V Push-Pull, Einheitszeit 509 µs
    GPIO ──> Pin 1 der RECHTEN -A
    3,3 V ──> + beider Platinen
    GND   ──> − beider Platinen, gemeinsame Masse

3,3 V Push-Pull genügt. Die `-B` treibt selbst nur **3,657 V**, und die `-A` läuft an
3,3 V Versorgung. Keine Pegelwandlung nötig.

**Zwei unabhängige Kanäle.** Beide Pins im Gleichschritt innerhalb eines Rahmens zu
treiben funktioniert, weil die Flanken bei 1 und 3 Einheiten innerhalb jedes Bits liegen
— ein Kanal kann eine 1 senden, während der andere eine 0 sendet, ohne dass das Timing
auseinanderläuft. Damit sind **beliebige Kombinationen** der 35 Codes möglich, etwa
Knight Rider links und rotes Dauerlicht rechts. Die `-B` konnte links und rechts zwar
getrennt blinken lassen (`(3,6)` / `(3,7)`), aber nur in den vorgesehenen Rollen.

**Keine versteckten Modi.** Alle **221** Codes mit gesetztem Startbit, die die `-B` nie
erzeugt, wurden gesendet, je 4 s. Kein einziger neuer Effekt. Das Weglassen der `-B`
spart eine Platine, bringt aber keinen Funktionsgewinn. Nicht geprüft: die 256 Codes
ohne Startbit, vermutlich ungültig.

**Zur Versorgung:** Hängen beide `-A` an der 3,3-V-Schiene eines Controller-Boards, kann
dessen Regler an die Grenze kommen. Flackern oder Neustarts beim Anschließen der zweiten
Platine deuten darauf — dann externe Versorgung nutzen.

---

## Effekt-Tabelle — alle 36 Rahmen

Spalte *Aufbau*: **voll** = beide Scheinwerfer und Pace-Car angeschlossen, belastbar ·
**1 LED** = nur eine LED angeschlossen, möglicherweise unvollständig. Das Polizeilicht
war in keiner Messung angeschlossen.

„eine Seite" / „andere Seite" sind Beobachtungsbeschreibungen, **nicht** die physische
Fahrzeugseite — welcher Scheinwerfer an welchem Ausgang steckte, ist nicht dokumentiert.

| lang bei | -B-Rahmen | -A-Code | Effekt | Aufbau |
|---|---|---|---|---|
| 0,1 | `00110101010` | `111000000` | kurzes Aufblinken aller LEDs | voll |
| 0,2 | `00100101010` | `110100000` | Doppelblinken aller LEDs | voll |
| 0,3 | `00101101010` | `110010000` | schnelles Blinken aller LEDs | voll |
| 0,4 | `00101001010` | `110001000` | schnelles Blinken aller LEDs | voll |
| 0,5 | `00101011010` | `110000100` | schnelles Blinken aller LEDs | voll |
| 0,6 | `00101010010` | `110000011` | schnelles Blinken aller LEDs | voll |
| 0,7 | `00101010110` | `110000001` | schnelles Blinken aller LEDs | voll |
| 0,8 | `00101010100` | `110000000` | schnelles Blinken aller LEDs | voll |
| 1,2 | `01100101010` | `101100000` | Doppelblinken, Pace-Car dauerhaft | voll |
| 1,3 | `01101101010` | `101010000` | kurzes Aufblinken aller LEDs | voll |
| 1,4 | `01101001010` | `101001000` | kurzes Blinken, dann lange Pause | voll |
| 1,5 | `01101011010` | `101000100` | wie 1,4 | voll |
| 1,6 | `01101010010` | `101000011` | alles blinkt kurz, mit Pause | voll |
| 1,7 | `01101010110` | `101000001` | **langes** Aufblinken aller LEDs | voll |
| 1,8 | `01101010100` | `101000000` | kurzes Aufblinken aller LEDs | voll |
| 2,3 | `01001101010` | `100110000` | Doppelblinken aller LEDs | voll |
| 2,4 | `01001001010` | `100101000` | Blinken | 1 LED |
| 2,5 | `01001011010` | `100100100` | **Blinken — Werkszustand / Failsafe** | 1 LED |
| 2,6 | `01001010010` | `100100011` | alles blinkt | voll |
| 2,7 | `01001010110` | `100100001` | Doppelblinken aller LEDs | voll |
| 2,8 | `01001010100` | `100100000` | Doppelblinken aller LEDs | voll |
| 3,4 | `01011001010` | `100011000` | **roter Ring Fade**, weiß + Pace-Car dauerhaft | voll |
| 3,5 | `01011011010` | `100010100` | **roter Ring Knight Rider**, weiß aus | voll |
| 3,6 | `01011010010` | `100010011` | **Blinker, eine Seite** | voll |
| 3,7 | `01011010110` | `100010001` | **Blinker, andere Seite** | voll |
| 3,8 | `01011010100` | `100010000` | roter Ring aus, nur weiß | voll |
| 4,5 | `01010011010` | `100001100` | **rotes Dauerleuchten** | voll |
| 4,6 | `01010010010` | `100001011` | Blinker, eine Seite | voll |
| 4,7 | `01010010110` | `100001001` | Blinker, andere Seite | voll |
| 4,8 | `01010010100` | `100001000` | Dauerlicht auf allen Leuchten | voll |
| 5,6 | `01010110010` | `100000111` | Blinker, eine Seite | voll |
| 5,7 | `01010110110` | `100000101` | Blinker, andere Seite | voll |
| 5,8 | `01010110100` | `100000100` | **Lauflicht beide Seiten**, Ring Knight Rider, weiß an | voll |
| 6,7 | `01010100110` | `100000011` | Dauerlicht auf allen Leuchten | voll |
| 6,8 | `01010100100` | `100000011` | identischer Code wie 6,7 | voll |
| 7,8 | `01010101100` | `100000001` | Blinker, andere Seite | voll |

### Die nützlichste Familie: erste Position 3

Dort steuert die zweite Position den **roten Ring**:

| lang bei | roter Ring |
|---|---|
| 3,4 | Fade / Atemlicht |
| 3,5 | Knight Rider |
| 3,6 | eine Seite blinkt |
| 3,7 | andere Seite blinkt |
| 3,8 | aus, nur weiß |

### Die beiden Lauflicht-Rahmen

| lang bei | Lauflicht | weiße LED |
|---|---|---|
| 3,5 | ja | **aus** |
| 5,8 | ja | **an** |

### Standbilder und Animationen unterscheiden

Wer zwei Codes gegeneinander takten will, ohne dass etwas animiert läuft, muss
**Standbilder** wählen. `4,8` ist „alles an", `4,5` ist „nur roter Ring, weiß aus" —
beides stehende Bilder. `3,5` schaltet weiß ebenfalls ab, bringt aber die
Knight-Rider-Animation des Rings mit, die die `-A` selbst erzeugt und die sich nicht
unterdrücken lässt.

---

## Verworfene Thesen

Aufgeführt, damit sie niemand erneut prüft:

- **„11 unabhängige Bits, Bits 2/6/7 wirkungslos"** — widerlegt. Bit 2 und 3 gemeinsam
  erzeugt einen 4-Einheiten-Impuls und damit einen ungültigen Rahmen. Das Protokoll
  überträgt Impulsbreiten, nicht ein Bit je Einheitszeit.
- **„Erste lange Position wählt die Farbe, zweite den Effekt"** — widerlegt durch die
  Aliase (0,6)==(4,6) und (3,7)==(4,7).
- **„(6,7) ist Lauflicht ohne Weiß"** — widerlegt, es ist Dauerleuchten.
- **„Die `-F` ist ein Protokoll-Übersetzer"** — falsch. Sie ist Spannungsregler und
  Rücklicht-Treiber und leitet das Signal durch.
- **„Die `-B` speichert den Modus"** — falsch, sie hört auf zu senden. Beobachtet worden
  war nur, dass die Lichter während eines Servo-Sweeps weiterleuchteten, wo am Eingang
  ein Signal anlag, aber kein gültiger Rahmen.
- **„Die `-A` braucht 5 V, ein 3,3-V-GPIO genügt nicht"** — widerlegt. Die `-B` treibt
  selbst nur 3,66 V, die `-A` läuft an 3,3 V. Der Open-Drain-Umweg mit Pull-up war
  unnötig und zudem kein gültiger Test (null Flanken, weil der Pull-up nicht an lebenden
  5 V hing).
- **„Die `-A` kennt versteckte Modi"** — widerlegt. 221 nie gesendete Codes, kein neuer
  Effekt.
- **„Ein Rückkanal auf der Datenleitung"** — unplausibel, es gibt niemanden, der eine
  Quittung verwerten könnte.
- **„Die vierte Ader ist ungenutztes Standard-Steckergehäuse"** — inkonsistent mit dem
  Kosten-Argument. Pin 2 ist nachweislich verzichtbar, warum er existiert, ist offen.

---

## Wie gemessen wurde

**Gerät:** Digilent Analog Discovery 2 über `libdwf`, aus Python mit `ctypes` angesteuert.
Digitalaufnahmen für das Timing, Analogaufnahmen für die echten Pegel.

Fallstricke, die man kennen sollte:

- Der Digitalpuffer hält nur **4096 Samples**. Fenster länger als eine Rahmenperiode
  wählen und den Burst zwischen zwei langen Low-Phasen ausschneiden — sonst erwischt man
  abgeschnittene Bursts und liest die Rahmenstruktur falsch.
- Der Analogpuffer hält 16384 Samples.
- Ein billiger FX2-Logikanalysator (`fx2lafw`, 8 Kanäle) taugt für das Timing ebenfalls,
  mindestens 20 kHz Abtastrate, 24 MHz nur einkanalig.

**Eine Warnung aus Erfahrung:** Stunden gingen an USB-Abbrüche verloren, die wie ein
Überspannungsproblem oder ein defekter Analysator aussahen. Die Ursache war ein
**Lade-USB-Kabel ohne Datenleitungen.** Brechen Aufnahmen ab, tausche zuerst das Kabel.

---

## Lizenz

- **Dokumentation** (diese Datei und die englische Fassung): [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)
- **Code** in `example/`: MIT, siehe [LICENSE](LICENSE)

Korrekturen und Ergänzungen willkommen — bitte als Issue. Offen ist insbesondere:

- die 256 Codes ohne Startbit wurden nie getestet
- die physische Fahrzeugseite von „eine Seite" / „andere Seite" ist nicht dokumentiert
- die Effektbeschreibungen zu `2,4` und `2,5` stammen aus einer Messung mit nur einer
  angeschlossenen LED und sind möglicherweise unvollständig
- wozu Pin 2 dient, ist unbekannt

English version: [README.md](README.md)
