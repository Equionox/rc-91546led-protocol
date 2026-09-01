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

    Bit 1        immer 1                           siehe unten, KEINE Startmarke
    Bit (2+a)    gesetzt
    Bit (2+b)    gesetzt, falls b <= 7
    Abschluss-H  3 statt 1 Einheit, falls 6 oder 7 in (a,b)

Geprüft gegen alle 36 gemessenen Rahmen: **0 Abweichungen.** Eine Referenzumsetzung
liegt in [`example/translate.c`](example/translate.c).

### Bit 1 ist keine Startmarke — die `-A` ignoriert es

Es sieht wie eine Rahmenmarke aus, weil die `-B` es ausnahmslos setzt. Es ist aber
wirkungslos. Nachgewiesen am 2026-09-01 an zwei Codes aus verschiedenen Familien:

| Rohcode | Wirkung |
|---|---|
| `10001100` H1 | roter Ring Fade, weiß an (Rahmen `3,4`) |
| `00001100` H1 | **identisch** — derselbe Fade |
| `10000110` H1 | roter Ring dauerhaft, weiß aus (Rahmen `4,5`) |
| `00000110` H1 | **identisch** — dasselbe Dauerleuchten |

Das tatsächliche Format ist also:

    Bit 1        ohne Bedeutung
    Bit 2..8     Positionen 0..6
    Abschluss    Position 7      H3 = gesetzt, H1 = nicht gesetzt
    Position 8   nicht übertragbar

**Der Abschlussimpuls ist die achte Positionsstelle.** Er verhält sich nicht wie ein
Modifikator, sondern wie ein weiteres Maskenbit — es fehlte nur der Platz im Bitfeld, also
wird es als langer Impuls angehängt. Mit dieser Lesart bilden die 36 Rahmen **35 Masken
ohne einen einzigen Widerspruch** ab, und die Kollision `(6,7)`/`(6,8)` erklärt sich von
selbst: beide senden die Maske `{6,7}`, weil die `-B` die 8 fallen lässt und zu einer 6
ohnehin H3 setzt.

**Grenze des Formats:** Für die Positionen bleiben nur 7 Bit. `(6,7)` und `(6,8)` erzeugen
deshalb denselben Code — die `-A` kann sie prinzipiell nicht unterscheiden. Die 36
Eingangsrahmen erreichen nur **35** verschiedene Codes.

**Grenze des Suchraums:** 7 Positionsbits mal 2 Abschlusslängen = **256 unterscheidbare
Codes**, und genau die sind alle gesendet worden. Die 256 Codes mit Bit 1 = 0 sind exakte
Duplikate, kein unerforschtes Gebiet. Es gibt in diesem Protokoll nichts mehr zu finden.

### Keine versteckte Feinstruktur

Getriggerte Aufnahme mit **10 MS/s** (0,1 µs Auflösung): im 1,64-ms-Fenster nur drei
Lauflängen, kein Impuls unter 50 µs außer dem Triggerpunkt. Die Flanken sind glatt, es
gibt keine versteckten Synchronisationsimpulse. Das Signalmodell ist vollständig.

---

## Der AUS-Code: `000000000`

Acht Nullbits, jedes als `H1 L3`, schalten die `-A` **sofort** dunkel, nicht erst nach
einer Zeitüberwachung.

Der Grund ist **nicht** das fehlende Bit 1 — das ignoriert die `-A` ohnehin — sondern dass
**keine einzige Position gesetzt** ist. Dieselbe Wirkung, andere Ursache; praktisch ändert
das nichts.

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
Lauflicht links und rotes Dauerlicht rechts. Die `-B` konnte links und rechts zwar
getrennt blinken lassen (`(3,6)` / `(3,7)`), aber nur in den vorgesehenen Rollen.

**Keine versteckten Modi — und der Suchraum ist vollständig.** Alle **221** Codes mit
Bit 1, die die `-B` nie erzeugt, wurden gesendet, je 4 s. Kein einziger neuer Effekt.
Zusammen mit den 35 Codes der `-B` sind das alle 256 unterscheidbaren Codes, denn wirksam
sind nur 7 Positionsbits und die Abschlusslänge; die 256 Codes mit Bit 1 = 0 sind exakte
Duplikate. Das Weglassen der `-B` spart eine Platine, bringt aber keinen Funktionsgewinn.

**Zur Versorgung:** Hängen beide `-A` an der 3,3-V-Schiene eines Controller-Boards, kann
dessen Regler an die Grenze kommen. Flackern oder Neustarts beim Anschließen der zweiten
Platine deuten darauf — dann externe Versorgung nutzen.

---

## Effekt-Tabelle — alle 36 Rahmen

Gemessen am 2026-09-01 im Direktbetrieb: zwei `-A`-Platinen mit vollständigem
Lichtsatz, jede an einem eigenen GPIO, beide im selben Rahmen. Alle 36 Rahmen
einzeln durchgesteppt, danach die gleich aussehenden Beschreibungen im direkten
A/B-Vergleich geprüft.

**36 Eingangsrahmen, 35 verschiedene Codes — aber nur 11 verschiedene Effekte.**

### Kein links/rechts, kein Pace-Car

Beide `-A` bekommen denselben Code und zeigen dasselbe. Eine Seitenzuordnung gibt
es im `-A`-Protokoll nicht.

Frühere Fassungen dieser Tabelle nannten Effekte wie „Blinker, eine Seite" und
„Pace-Car dauerhaft". Beides waren Eigenschaften der **`-B`**: sie hatte
kanalgebundene Ausgänge und trieb das Pace-Car-Licht aus eigener Kraft. Wer die
`-A` direkt ansteuert, hat weder das eine noch das andere. Die Tabelle beschreibt
deshalb ausschließlich, was **eine `-A`-Platine** tut.

| lang bei | -B-Rahmen | -A-Code | Effekt |
|---|---|---|---|
| 0,1 | `00110101010` | `111000000` | kurzes Aufblinken aller LEDs |
| 0,2 | `00100101010` | `110100000` | Doppelblinken aller LEDs |
| 0,3 | `00101101010` | `110010000` | schnelles Blinken aller LEDs |
| 0,4 | `00101001010` | `110001000` | schnelles Blinken aller LEDs |
| 0,5 | `00101011010` | `110000100` | schnelles Blinken aller LEDs |
| 0,6 | `00101010010` | `110000011` | schnelles Blinken aller LEDs |
| 0,7 | `00101010110` | `110000001` | schnelles Blinken aller LEDs |
| 0,8 | `00101010100` | `110000000` | schnelles Blinken aller LEDs |
| 1,2 | `01100101010` | `101100000` | Doppelblinken aller LEDs |
| 1,3 | `01101101010` | `101010000` | kurzes Aufblinken aller LEDs |
| 1,4 | `01101001010` | `101001000` | kurzes Blinken, dann lange Pause |
| 1,5 | `01101011010` | `101000100` | kurzes Blinken, dann lange Pause |
| 1,6 | `01101010010` | `101000011` | kurzes Blinken, dann lange Pause |
| 1,7 | `01101010110` | `101000001` | kurzes Blinken, dann lange Pause |
| 1,8 | `01101010100` | `101000000` | kurzes Blinken, dann lange Pause |
| 2,3 | `01001101010` | `100110000` | Doppelblinken aller LEDs |
| 2,4 | `01001001010` | `100101000` | Doppelblinken aller LEDs |
| 2,5 | `01001011010` | `100100100` | Doppelblinken aller LEDs |
| 2,6 | `01001010010` | `100100011` | Doppelblinken aller LEDs |
| 2,7 | `01001010110` | `100100001` | Doppelblinken aller LEDs |
| 2,8 | `01001010100` | `100100000` | Doppelblinken aller LEDs |
| 3,4 | `01011001010` | `100011000` | **roter Ring Fade, weiß an** |
| 3,5 | `01011011010` | `100010100` | **roter Ring Lauflicht, weiß blinkt** |
| 3,6 | `01011010010` | `100010011` | Dauerlicht auf allen Leuchten |
| 3,7 | `01011010110` | `100010001` | Blinken aller LEDs |
| 3,8 | `01011010100` | `100010000` | **roter Ring aus, nur weiß** |
| 4,5 | `01010011010` | `100001100` | **rotes Dauerleuchten, weiß aus** |
| 4,6 | `01010010010` | `100001011` | Dauerlicht auf allen Leuchten |
| 4,7 | `01010010110` | `100001001` | Blinken aller LEDs |
| 4,8 | `01010010100` | `100001000` | Dauerlicht auf allen Leuchten |
| 5,6 | `01010110010` | `100000111` | Dauerlicht auf allen Leuchten |
| 5,7 | `01010110110` | `100000101` | Blinken aller LEDs |
| 5,8 | `01010110100` | `100000100` | **roter Ring Lauflicht, weiß an** |
| 6,7 | `01010100110` | `100000011` | Dauerlicht auf allen Leuchten |
| 6,8 | `01010100100` | `100000011` | Dauerlicht auf allen Leuchten |
| 7,8 | `01010101100` | `100000001` | Blinken aller LEDs |

Fett hervorgehoben sind die fünf Rahmen, die jeweils als **einziger** ihren Effekt
liefern. Alles andere ist mehrfach belegt.

### Die Struktur dahinter

Die erste lange Position wählt die **Familie**, die zweite den Effekt darin:

| erste Position | Familie |
|---|---|
| 0 | schnelles Blinken aller LEDs |
| 1 | kurzes Blinken, dann lange Pause |
| 2 | Doppelblinken aller LEDs |
| 3, 4, 5 | roter Ring und weiße LED **getrennt** |
| 6 | Dauerlicht |
| 7 | Blinken aller LEDs |

Bei erster Position 0, 1 oder 2 wirkt die zweite nur, wenn sie unmittelbar
benachbart ist — `0,1` und `1,3` geben ein kurzes Aufblinken, `0,2` und `1,2` ein
Doppelblinken, alles Weitere bleibt bei der Familie.

Sind **beide** Positionen ≥ 3, wird es interessant, und die Positionen 6 und 7
überschreiben die Familie:

| zweite Position | Wirkung, wenn erste ≥ 3 |
|---|---|
| 6 | Dauerlicht auf allen Leuchten — immer |
| 7 | Blinken aller LEDs — immer |
| 4, 5, 8 | der spezifische Effekt aus roter Ring + weiße LED |

Damit bleiben genau sechs Rahmen, die den roten Ring und die weiße LED
unabhängig behandeln:

| lang bei | roter Ring | weiße LED |
|---|---|---|
| 3,4 | Fade / Atemlicht | an |
| 3,5 | Lauflicht | **blinkt** |
| 3,8 | aus | an |
| 4,5 | dauerhaft an | **aus** |
| 4,8 | dauerhaft an | an |
| 5,8 | Lauflicht | an |

`4,8` fällt dabei mit dem Dauerlicht zusammen, obwohl es weder 6 noch 7 enthält.

### Die Familienregel gilt auch jenseits der 36 Rahmen

Die `-B` kann nie mehr als zwei Positionen setzen. Ein selbst erzeugter Code kann es —
und dann zeigt sich, dass die Blink-Familien 0, 1 und 2 sich gegen den differenzierten
Bereich 3–6 durchsetzen und die höheren Positionen einfach schlucken.

Drei Vorhersagen, vorab angesagt und alle eingetroffen (2026-09-01):

| Positionen | maßgeblich | vorhergesagt und beobachtet |
|---|---|---|
| 2, 3, 4 | 2 | Doppelblinken aller LEDs |
| 1, 3, 4 | 1 | kurzes Blinken, dann lange Pause |
| 0, 3, 4 | 0 | schnelles Blinken aller LEDs |

**Es ist aber keine Ordnung nach Zahlenwert.** „Die niedrigste Position gewinnt" wäre die
naheliegende Lesart und ist falsch — zwei Rahmen der `-B` widersprechen:

| Positionen | nach „niedrigste gewinnt" | tatsächlich |
|---|---|---|
| 0, 2 | schnelles Blinken | **Doppelblinken** — Familie 2 |
| 1, 2 | kurzes Blinken + Pause | **Doppelblinken** — Familie 2 |

Die Rangfolge ist also `2` vor `0` und `1`, und alle drei vor dem Bereich 3–6. Ob 0 vor 1
steht, ist offen: die einzige Kombination `0,1` liefert ein drittes Ergebnis („kurzes
Aufblinken"), ebenso `1,3`. Die drei Vorhersagen oben gingen auf, weil die hinzugefügte
Position dort jeweils *zugleich* die niedrigere und aus dem starken Bereich 0–2 war — sie
konnten den Unterschied gar nicht aufdecken.

**Der Bereich „Ring und Weiß getrennt" verhält sich anders — er braucht ein exaktes
Paar.** Eine dritte Position oder die falsche Abschlusslänge wirft den Code aus der
Tabelle:

| Positionen | Abschluss | Ergebnis |
|---|---|---|
| 3, 4 | H1 | roter Ring Fade, weiß an |
| 3, 4 | **H3** | Blinken aller LEDs |
| 3, 4, **6** | H1 | Blinken aller LEDs — *nicht* Dauerlicht, was eine 6 sonst bedeutet |
| 3, 4, **5** | H1 | **verworfen**, die `-A` bleibt dunkel |

Einen pauschalen Rückfall auf Blinken gibt es also nicht: Positionen 3, 4, 5 gehen dunkel.

**Praktische Folge:** Kombinationen wie „Fade ohne die weiße LED" lassen sich nicht
zusammenbauen. Der Fade hängt an genau einem Code, und alle neun Ein-Bit-Nachbarn führen
woanders hin. Das ist kein „nicht gefunden", sondern ein **existiert nicht** — siehe
[Der differenzierte Bereich ist abgeschlossen](#der-differenzierte-bereich-ist-abgeschlossen).

### Die Familien im Überblick

Nach der niedrigsten gesetzten Position — Position 7 und 8 haben kein Bit:

| Maske enthält | Verhalten |
|---|---|
| 0, 1 oder 2 | Blink-Familie, setzt sich gegen alles Höhere durch — `2` sticht `0` und `1` |
| 3, 4, 5 (ohne 6, ohne 7) | roter Ring und weiße LED getrennt — Nachschlagetabelle |
| **6 und 7 zusammen** | Dauerlicht auf allen Leuchten |
| 6 allein, oder 7 allein | Blinken aller LEDs |
| gar nichts | dunkel |

**Es gibt keine „Familie 6".** `{6,7}` ergibt Dauerlicht, aber `{6}` allein und `{7}` allein
ergeben beide Blinken. Die `-B` kann `{6}` nie senden, weil sie zu einer 6 immer H3 setzt —
deshalb sah es aus, als bedeute die 6 Dauerlicht. Gemessen wurde `{6}` am 2026-09-01 mit
einem selbst gebauten Code: **Blinken**.

Ebenso ist `{}` gegen `{7}` der Unterschied zwischen dem AUS-Code und `7,8` — dieselbe
leere Bitmaske, und allein die achte Positionsstelle entscheidet zwischen dunkel und
Blinken.

**Eine Warnung zur Benutzung dieser Tabelle:** sie ist eine Beschreibung, keine
Mechanik. An einem einzigen Tag sind hier zwei zu allgemein formulierte Regeln gefallen
(„die niedrigste Position gewinnt", „H3 heißt Blinken"). Die Wirkung ist eine
Nachschlagetabelle über die Maske; die Regelmäßigkeiten oben sind Muster darin, keine
Herleitung.

### Der differenzierte Bereich ist abgeschlossen

Masken mit 0, 1 oder 2 landen in einer Blink-Familie; Masken mit 6 oder 7 in Dauerlicht
oder Blinken. **Alles, was roten Ring und weiße LED getrennt behandelt, muss deshalb eine
Teilmenge von `{3,4,5}` sein.** Davon gibt es genau acht, und alle acht sind gemessen:

| Maske | Effekt |
|---|---|
| `{}` | dunkel |
| `{3}` | roter Ring aus, nur weiß |
| `{4}` | Dauerlicht auf allen Leuchten |
| `{5}` | roter Ring Lauflicht, weiß an |
| `{3,4}` | **roter Ring Fade**, weiß an |
| `{3,5}` | roter Ring Lauflicht, weiß blinkt |
| `{4,5}` | rotes Dauerleuchten, **weiß aus** |
| `{3,4,5}` | dunkel |

**Es gibt keine Merkmalsbits.** „Fade" tritt in genau einer Maske auf, „weiß aus" ebenfalls
in genau einer — und es sind verschiedene. Weder `{3}` noch `{4}` allein fadet. Gäbe es ein
Fade-Bit und ein Weiß-aus-Bit, ließen sie sich kombinieren; das ist nicht der Fall.

Die Effekte sind also **eine Speisekarte fertiger Szenen, keine Kodierung aus Merkmalen**:
sieben nichtleere Masken, sechs verschiedene Bilder, von Hand ausgesucht. Wer eine
Kombination sucht, die nicht in dieser Liste steht — etwa „Fade ohne die weiße LED" —
sucht nicht nach etwas Unentdecktem, sondern nach etwas, das es nicht gibt.

### Die beiden Lauflicht-Rahmen

`3,5` und `5,8` fahren dasselbe Wandermuster auf dem roten Ring und unterscheiden
sich nur in der weißen LED:

| lang bei | roter Ring | weiße LED |
|---|---|---|
| 3,5 | Lauflicht | **blinkt** |
| 5,8 | Lauflicht | **an** |

Bei `3,5` blinkt die weiße LED, sie ist nicht aus — eine frühere Fassung dieses
Dokuments hatte das falsch, weil damals nur eine LED angeschlossen war.

**Zur Benennung:** Das wandernde Muster auf dem Ring heißt in diesem Dokument
durchgehend **Lauflicht** — in der englischen Fassung *chase*, weil „running light"
dort das Tagfahrlicht bezeichnen würde. Gelegentlich findet man dafür auch den
Namen *Knight Rider*.

### Die Abschlusslänge ist funktional

Der Impuls nach dem achten Bit ist 1 oder 3 Einheiten lang — und diese Länge ist
Teil des Codes, nicht bloß eine Rahmenmarke. Drei Paare belegen es: gleiche acht
Bits, unterschiedliche Abschlusslänge, unterschiedlicher Effekt.

| 8 Bit | Abschluss | lang bei | Effekt |
|---|---|---|---|
| `10001000` | H1 | 3,8 | roter Ring aus, nur weiß |
| `10001000` | H3 | 3,7 | Blinken aller LEDs |
| `10000100` | H1 | 4,8 | Dauerlicht |
| `10000100` | H3 | 4,7 | Blinken aller LEDs |
| `10000010` | H1 | 5,8 | roter Ring Lauflicht, weiß an |
| `10000010` | H3 | 5,7 | Blinken aller LEDs |

Bei den drei entsprechenden Paaren mit erster Position 0, 1 oder 2 macht die
Abschlusslänge **keinen** Unterschied — dort dominiert die Familie.

**Achtung, die Richtung ist nicht immer dieselbe.** In den drei Paaren oben führt H3 ins
Blinken. Bei der Maske `{6}` ist es umgekehrt: H1 blinkt, H3 gibt Dauerlicht. „H3 heißt
Blinken" ist also falsch — der Abschluss ist die achte Positionsstelle, kein
Blink-Schalter.

| Maske | H1 (Position 7 nicht gesetzt) | H3 (Position 7 gesetzt) |
|---|---|---|
| `{3}` | roter Ring aus, nur weiß | Blinken |
| `{4}` | Dauerlicht | Blinken |
| `{5}` | roter Ring Lauflicht, weiß an | Blinken |
| `{6}` | **Blinken** | **Dauerlicht** |
| `{}` | **dunkel** | **Blinken** |

Wer die `-A` selbst ansteuert, muss die Abschlusslänge also mit übertragen.

### Die Zeitgeber der -A laufen frei und unabhängig

Versuch: gemeinsamer Reset des Controllers, beide `-A` gingen dunkel und bekamen
denselben Startrahmen im selben Moment. Danach ein konstanter Blinkcode auf beide
Seiten.

**Ergebnis: sie blinken unterschiedlich schnell.** Nicht nur versetzt, sondern mit
verschiedener Frequenz.

Das Protokoll wählt den **Effekt**, nicht den **Takt**. Jede `-A` erzeugt ihre
Animation aus einem eigenen, ungenauen Zeitgeber. Für Nachbauer heißt das:

- **Synchrone Blinker sind über die Effektcodes nicht erreichbar.** Wer sie
  braucht, muss den Takt im Controller machen: einen *stehenden* Effekt wählen
  und ihn gegen den AUS-Code takten. Dann kommt die Zeit vom Controller, und
  beide Seiten schalten im selben Rahmen.
- Die Blinkraten in der Tabelle beschreiben **eine** Platine und sind keine
  zugesicherte Größe.
- Zwei blinkende Codes lassen sich per Umschalten nicht gegeneinander
  vergleichen — der Versatz der Platinen überdeckt den Unterschied. `3,7` gegen
  `4,7` ließ sich deshalb nicht trennen; beide zeigen Blinken aller LEDs, ob die
  Rate identisch ist, ist offen.

Das erklärt vermutlich, warum die Originalanlage die Blinker über getrennte
`-B`-Ausgänge gemacht hat und nicht über Effekte: dort schaltete die `-B` ihre
Ausgänge, der Takt kam also aus einer einzigen Quelle.

### Ein Codewechsel setzt die Animation zurück

Wechselt der gesendete Code, beginnt die `-A` ihre Animation von vorn. Ein
Lauflicht, das im Sekundentakt neu angestoßen wird, bricht deshalb sichtbar ab
und startet neu, statt durchzulaufen.

Zwei Konsequenzen:

- Wer einen Effekt sauber laufen sehen will, hält den Code **konstant**.
- Wer zwei Zustände gegeneinander taktet, nimmt **stehende** Bilder: `4,5`
  (roter Ring an, weiß aus), `4,8` (alles an) und der AUS-Code sind stehend.
  `3,5` bringt dagegen die Lauflicht-Animation mit, die sich nicht unterdrücken
  lässt und bei jedem Takt neu beginnt.

---
## Verworfene Thesen

Aufgeführt, damit sie niemand erneut prüft:

- **„11 unabhängige Bits, Bits 2/6/7 wirkungslos"** — widerlegt. Bit 2 und 3 gemeinsam
  erzeugt einen 4-Einheiten-Impuls und damit einen ungültigen Rahmen. Das Protokoll
  überträgt Impulsbreiten, nicht ein Bit je Einheitszeit.
- **„Erste lange Position wählt die Farbe, zweite den Effekt"** — in dieser Form falsch,
  in abgewandelter Form richtig. Die erste Position wählt keine *Farbe*, sondern die
  **Familie**, und die zweite ist nicht frei, weil 6 und 7 die Familie überschreiben.
  Die Aliase (0,6)==(4,6) und (3,7)==(4,7), mit denen die These damals verworfen wurde,
  sind genau diese Überschreibung. Siehe [Die Struktur dahinter](#die-struktur-dahinter).
- **„Blinker eine Seite / andere Seite" und „Pace-Car dauerhaft" sind Effekte der `-A`"**
  — falsch. Beides waren Eigenschaften der `-B`, die kanalgebundene Ausgänge hatte. Zwei
  `-A` am selben Rahmen zeigen immer dasselbe.
- **„Bei `3,5` ist die weiße LED aus"** — falsch, sie **blinkt**. Der Fehler entstand,
  weil bei der ersten Messreihe nur eine einzige LED angeschlossen war.
- **„Zwei `-A` blinken synchron, wenn sie denselben Code bekommen"** — widerlegt. Sie
  blinken auch nach gemeinsamem Reset unterschiedlich **schnell**.
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
- **„Bit 1 ist die Startmarke des Rahmens"** — falsch, die `-A` ignoriert es. `00001100`
  liefert denselben Fade wie `10001100`, `00000110` dasselbe Dauerleuchten wie `10000110`.
  Die `-B` setzt das Bit ausnahmslos, deshalb sah es wie eine Rahmenmarke aus.
- **„Die 256 Codes ohne Startbit sind vermutlich ungültig"** — falsch, sie sind **exakte
  Duplikate** der 256 mit Bit 1. Damit ist der Suchraum nicht halb, sondern vollständig
  durchgemessen.
- **„Die `-A` kennt versteckte Modi"** — widerlegt, und seit dem 2026-09-01 lückenlos:
  wirksam sind 7 Positionsbits mal 2 Abschlusslängen = 256 Codes, alle gesendet (35 aus
  den Rahmen der `-B`, 221 im Durchlauf). Kein neuer Effekt.
- **„Ein unbekannter Code fällt immer auf Blinken zurück"** — widerlegt. Die Positionen
  3, 4, 5 werden verworfen, die `-A` bleibt dunkel.
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

### Die zweite Messreihe: alle 36 Rahmen am vollständigen Lichtsatz

Die erste Effekt-Tabelle entstand mit **einer einzigen angeschlossenen LED** und war
deshalb an mehreren Stellen falsch. Am 2026-09-01 wurde sie neu aufgenommen, mit
vollständigem Lichtsatz an zwei `-A`-Platinen und einem Skript, das alle 36 Rahmen
einzeln durchsteppt und jeden hält, bis er bestätigt ist. Was sich dadurch geändert hat:

- „Blinker eine Seite / andere Seite" und „Pace-Car dauerhaft" waren Eigenschaften der
  `-B`, keine Effekte
- bei `3,5` **blinkt** die weiße LED, sie ist nicht aus
- sechs getrennt geführte Rahmen sind ein und dasselbe Dauerlicht
- die Abschlusslänge erwies sich als funktional
- die Blinkzeitgeber der beiden Platinen laufen gegeneinander weg

Die Lehre gilt allgemein: **eine Effekt-Tabelle mit unvollständigem Lichtsatz ist nicht
belastbar**, weil Modi, die sich nur auf den fehlenden Leuchten unterscheiden, identisch
aussehen.

---

## Lizenz

**MIT** — für Dokumentation und Code gleichermaßen. Siehe [LICENSE](LICENSE). Benutzen,
anpassen, darauf aufbauen; nur der Copyright-Hinweis muss mit.

Korrekturen und Ergänzungen willkommen — bitte als Issue. Offen ist insbesondere:

- ob `3,7`, `4,7`, `5,7` und `7,8` mit *derselben Rate* blinken, ist offen — der Eigendrift
  der Platinen macht das schwer messbar
- wozu Pin 2 dient, ist unbekannt
- die Widerstandswerte auf der `-A` und die Stellung von `SB1` wurden nie ausgelesen

English version: [README.md](README.md)
