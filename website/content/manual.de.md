---
title: Handbuch
description: So bedienst du DJ-Workstation – Player, Crossfader, Bibliothek, Playlists, Controller und Einstellungen.
layout: manual
---

DJ-Workstation ist zuerst für Touchscreens gemacht: Jedes Bedienelement funktioniert mit dem Finger, die Maus genauso.
Bleibt die Maus auf einem Element stehen, erscheint eine kurze Hilfe. Die Bedienelemente des Players behalten in jeder
Sprache die englischen DJ-Begriffe (Load, CUE, KEY, Pitch).

## Inhalt {#contents}

- [Erster Start](#first-start)
- [Der Player](#the-player)
- [Beat-Waveforms und Crossfader](#beat-waveforms-and-crossfader)
- [Die Bibliothek](#the-library)
- [Playlists](#playlists)
- [Fenster und Layouts](#windows-and-layouts)
- [Tastenkürzel](#keyboard-shortcuts)
- [MIDI-Controller](#midi-controllers)
- [Einstellungen](#settings)

## Erster Start {#first-start}

Solange die Bibliothek noch keinen Musikordner hat, fragt DJ-Workstation beim Start danach. Dann liest es alle Titel in
diesem Ordner und seinen Unterordnern (MP3, Ogg Vorbis, FLAC und WAV) samt ihren Tags ein. Weitere Ordner fügst du
später mit **Datei > Musikordner hinzufügen…** hinzu.

Ohne gespeichertes Layout öffnet das Programm zwei Player mit den Beat-Waveforms dazwischen und dem Crossfader darunter.

## Der Player {#the-player}

{{< callouts name="player" alt="Ein Player mit nummerierten Markierungen an seinen Bedienelementen" >}}

1. **Load** lädt den in der Bibliothek gewählten Titel in diesen Player. Du kannst einen Titel in der Bibliothek auch
   doppelt antippen (er kommt in einen freien Player) oder eine Datei aus dem Dateimanager auf den Player ziehen.
   Solange der Player spielt, ist Load abgeblendet: Ein laufender Titel wird nie ersetzt.
2. **− / +** bremsen den Titel oder beschleunigen ihn, solange du sie hältst, um zwei Titel wieder in den Takt zu
   schieben.
3. **KEY** schaltet Keylock ein: Das Tempo ändert sich mit dem Pitch-Fader, die Tonart bleibt.
4. **Time** schaltet die Anzeige zwischen vergangener und verbleibender Zeit um.
5. **BPM** ist das Tempo des Titels, mit dem Pitch verrechnet.
6. **Die Waveform** zeigt den ganzen Titel; der schon gespielte Teil wird gelb. Tippe darauf, um dorthin zu springen.
   Bei einem stehenden Player hörst du an der berührten Stelle eine kurze Schleife und findest so eine Stelle nach
   Gehör; CUE setzt dann dort den Cue-Punkt.
7. **Der Zustand** des Players: geladen, am Cue, Pause oder spielt.
8. **CUE** arbeitet wie am CD-Player: Steht der Player, setzt es den Cue-Punkt an die aktuelle Stelle; spielt er,
   springt es zum Cue-Punkt zurück und stoppt; gedrückt gehalten, spielt es vom Cue-Punkt, bis du loslässt.
9. **Play** startet und pausiert den Titel.
10. **Der Pitch-Fader** ändert das Tempo um bis zu ±10 % in Schritten von 0,02 %. Wie am Plattenspieler ist unten
    schneller. Doppelt antippen stellt ihn auf 0 zurück.
11. **Vol** ist die Kanallautstärke; die Anzeige in seiner Spur zeigt den Pegel.
12. **Vorhören** schickt den Player auf den Kopfhörer; sein Licht leuchtet, solange es an ist.
13. **Head** ist die Kopfhörerlautstärke.
14. **Interpret und Titel** des geladenen Stücks.
15. **Cue-Load**: Ist es an, startet ein geladener Titel an dem Cue-Punkt, den du zuletzt für ihn gesetzt hast. Ein
    Titel ohne Cue-Punkt startet an seinem ersten Ton statt in der Stille davor.

## Beat-Waveforms und Crossfader {#beat-waveforms-and-crossfader}

{{< callouts name="mixer" alt="Beat-Waveforms und Crossfader mit nummerierten Markierungen" >}}

1. **Beat-Waveforms** zeigen sechs Sekunden der Player auf Seite A und B: Bässe rot, Mitten grün, Höhen blau.
2. **Die feste Linie** ist die Stelle, die gerade spielt. Treffen die Beats beider Spuren sie gleichzeitig, laufen die
   Titel im Takt.
3. **Dieser Knopf** dreht die Spuren von senkrecht nach waagerecht und zurück.
4. **Die beiden Auswahllisten** bestimmen, welcher Player auf Seite A und welcher auf Seite B liegt.
5. **Null** stellt den Crossfader in die Mitte.
6. **Die Anzeige** zeigt den Master-Pegel.
7. **A** und **B** springen auf diese Seite, solange du sie hältst, und zurück, wenn du loslässt.
8. **Der Crossfader** blendet von Seite A nach Seite B.
9. **Jetzt faden!** blendet selbsttätig auf die andere Seite über.
10. **Überblendzeit (s)** legt fest, wie lange diese Überblendung dauert.

## Die Bibliothek {#the-library}

{{< callouts name="library" alt="Die Bibliothek mit nummerierten Markierungen an Menüleiste, Suche, Filtern und Titelliste" >}}

1. **Die Menüleiste**: Datei, Playlist, Einstellungen, Ansicht und Layout.
2. **Suche** findet Titel schon beim Tippen, in allen Feldern zugleich. Tippst du, während kein Textfeld gewählt ist,
   landet es ebenfalls hier – du kannst also einfach lostippen.
3. **Die Spaltenfilter** durchsuchen je eine Spalte: Interpret, Titel, Genre und Jahr. `*` steht für beliebigen Text,
   `?` für ein Zeichen.
4. **Ein Spaltenkopf** sortiert die Liste nach dieser Spalte.
5. **Die Titelliste**: Tippe einen Titel an, um ihn zu wählen, doppelt, um ihn in einen freien Player zu laden.
6. **Die Zahl der gefundenen Titel** und was die Bibliothek gerade tut (einlesen, analysieren).

Ein Rechtsklick auf einen Titel bietet mehr:

- **Tags bearbeiten** ändert Interpret, Titel, Genre und Jahr in der Datei und in der Bibliothek. Die Tags eines
  Titels, der gerade spielt, lassen sich nicht ändern.
- **Erneut einlesen** liest die Tags der Datei noch einmal und analysiert sie neu.

Tempo (BPM) und Waveform eines Titels werden beim ersten Laden ermittelt. **Datei > Alle Titel analysieren** erledigt
das für die ganze Bibliothek im Hintergrund, während du weiterarbeitest.

**Datei > Bibliothek neu einlesen** sucht neue, geänderte und gelöschte Dateien. Verschwundene Dateien werden gezählt;
**Fehlende entfernen** nimmt sie aus der Bibliothek.

## Playlists {#playlists}

Die Playlist öffnest du mit **Ansicht > Playlist-Fenster**. **Neu** legt eine Playlist an, **Zur Playlist** in der
Bibliothek fügt den gewählten Titel hinzu, **Hoch** und **Runter** ändern die Reihenfolge und **Entfernen** nimmt einen
Titel heraus. Playlists liegen in der Bibliothek und lassen sich als M3U-Dateien importieren und exportieren
(**Import**, **Export**).

## Fenster und Layouts {#windows-and-layouts}

Jeder Player, der Crossfader, die Beat-Waveforms und die Playlist können ein eigenes Fenster sein, frei verschiebbar
und in der Größe veränderbar, auch auf einen anderen Bildschirm. **Ansicht > Ein-Fenster-Modus** legt stattdessen alles
in ein Fenster (ab dem nächsten Start). **Ansicht > Neuer Player** öffnet einen weiteren Player; eine feste Grenze gibt
es nicht. **Ansicht > Vollbild** (Strg+F) füllt den Bildschirm, für Touch-Terminals.

Im einen Fenster kannst du mit **Layout > Layout bearbeiten** einen Teil ziehen, um ihn zu verschieben, oder an seiner
rechten unteren Ecke, um seine Größe zu ändern; die Kanten rasten an den Nachbarn ein. **Layout > Layout speichern**
behält die Anordnung für den nächsten Start, **Layout > Auf Standard zurücksetzen** kehrt zu zwei Playern mit
Crossfader zurück.

**Layout > Layout speichern unter…** schreibt die Anordnung in eine eigene Datei, zum Beispiel eine pro Club oder
Bildschirm; **Layout > Layout laden…** holt sie sofort zurück und behält sie für den nächsten Start. Ein Layout aus dem
anderen Fenstermodus erscheint ab dem nächsten Start.

**Ansicht > Tageslicht-Farben** schaltet auf helle Farben für helle Räume oder Sonnenlicht.

Solange ein Titel spielt, fragt **Datei > Beenden** erst nach, damit ein Set nicht aus Versehen endet.

## Tastenkürzel {#keyboard-shortcuts}

Sie wirken auf den Player mit dem Tastaturfokus und nutzen immer die linke Alt-Taste (Option am Mac), damit eine
getippte Suche nie einen Player erreicht.

| Tasten | Aktion |
|---|---|
| Alt+P | Play / Pause |
| Alt+C | Cue (wirkt, solange gehalten) |
| Alt+L | Gewählten Titel laden |
| Alt+← / Alt+→ | Schritt zurück / vor (mit Shift: 10 Sekunden) |
| Alt+↑ / Alt+↓ | Pitch ±0,02 % (mit Shift: ±0,1 %) |
| Strg+F | Vollbild |

## MIDI-Controller {#midi-controllers}

Controller lassen sich im laufenden Betrieb an- und abstecken. **Einstellungen > Geräte…** weist jedem Controller ein
Mapping zu. Ein Mapping ist eine kleine TOML-Datei, die Tasten, Fader und Jogwheels des Controllers mit Play, Cue,
Laden, Pitch, Lautstärke, Crossfader und mehr verbindet und seine LEDs leuchten lässt. Das Format ist in
[mappings/README.md](https://github.com/sagiadinos/dj-workstation/blob/main/mappings/README.md) beschrieben (englisch).

**Mixxx-Mapping importieren…** macht aus einem Mixxx-Mapping (.midi.xml) eines von uns. Seine direkten Zuordnungen
werden übernommen; was Mixxx per Skript erledigt, bleibt weg und wird gezählt.

Der Hercules DJControl Starlight wird am Namen erkannt, samt seiner Soundkarte: Master auf den Ausgängen 1/2,
Kopfhörer auf 3/4. Seine Jogwheels verbiegen den Pitch, wenn man sie am Rand dreht, und scratchen, wenn man sie oben
berührt.

## Einstellungen {#settings}

**Einstellungen > Geräte…**: das Ausgabegerät, Stereo oder vier Kanäle (Master auf 1/2, Kopfhörer auf 3/4), die
Latenz (kürzer reagiert schneller, länger schützt vor Aussetzern) und die Controller-Mappings. Audio-Einstellungen
wirken ab dem nächsten Start.

**Einstellungen > Sound…**: die Keylock-Qualität, Hoch (die Voreinstellung) oder Standard für langsame Rechner.

**Einstellungen > Sprache…**: Englisch, Deutsch, Französisch, Spanisch, Griechisch oder Russisch; die Änderung wirkt
sofort.
