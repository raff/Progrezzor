# Testliste

Samlet 2026-11-07, til Henrik. Afkryds, når det er set. Alt testes på den version, der ligger i ~/.clap og ~/.vst3
(branch `test/vst3-instrument`). Genstart DAW'en eller scan plugins igen først.

## 1. VST3 som instrument (branch test/vst3-instrument)

Når du har godkendt, skriver jeg i CLAUDE.md under Teknisk fundament: "Plugin-typen er et bevidst valg ud fra
hovedopgaven, som er at generere MIDI. CLAP er note-effect, så det sidder før synthen i Bitwig. VST3 er instrument, fordi
VST3 ikke har en MIDI-effekt-kategori, og plugin'et både laver noder og preview-lyd. Ændr det ikke uden at spørge." og
retter de gamle linjer om mulighed A og B. Branchen merges ikke før du siger det.

- [ ] Bitwig (Linux), CLAP: vises stadig som Note FX, spiller synthen efter sig, og preview kan høres.
- [ ] Bitwig (Linux), VST3: vises som instrument, sender MIDI videre til en synth, og preview kan høres.
- [ ] REAPER (Linux), VST3: sender MIDI videre til en synth, og preview kan høres.
- [ ] Bitwig og REAPER (Linux): Sound = Off, så preview ikke klinger sammen med synthen. MIDI sendes stadig.
- [ ] Bitwig og REAPER (Linux): træk en rundgang ud som MIDI-fil, både fra CLAP og fra VST3.
- [ ] Ableton Live (Windows): VST3 kan sættes på et MIDI-spor, og et andet spor kan få MIDI fra den via MIDI From.
      (Kan først testes, når der er et Windows-build.)

## 2. Ny oktav-styring (2026-11-07)

- [ ] Oct- og Oct+ i bjælkens højre side: hæver og sænker alt (pads, rundgang, MIDI-fil) en oktav ad gangen, -2 til +2.
- [ ] Mærket mellem knapperne viser, hvilket leje vi står i ("Oct 0", "Oct +1", "Oct -2"). Orange, når det ikke er 0.
- [ ] Klik rundt på padsene i lang tid og i tilfældig rækkefølge: hver pad lyder HELT ens hver gang. Ingen automatisk
      stemmeføring og ingen drift. (Før: op til ca. 4 oktaver forskel mellem samme pad.)
- [ ] Settings (tandhjulet i bjælkens højre side): vælg Low, Mid eller High. Alt flyttes en oktav fra Mid. Panelet viser
      lejet ("Low", "Mid", "High") over Oct-trinnet ("Oct 0", "Oct +1"). Oct-/Oct+ tæller fra det valgte standardleje.
- [ ] Standardlejet huskes i projektet, og MIDI-filen har samme toner, som du hører.
- [ ] Hold en pad nede, tryk Oct+/Oct- og slip: ingen hængende noter.
- [ ] Spil rundgangen, tryk Suggest flere gange: akkorderne ligger altid i samme leje. De må ikke hoppe op og ned.
- [ ] Træk MIDI-filen ud med Oct +1: filen har samme toner, som du hører.
- [ ] Luk og åbn projektet: lejet huskes.
- [ ] Pilene op/ned på blokkene (Edit, musen over): små, i hjørnerne. Man rammer dem ikke ved et uheld, når man vil
      trykke på selve akkorden.

## 2b. Arp (2026-11-07)

- [ ] Arp-knappen (nederst til venstre i bjælken, under Play): tænder og slukker. Slået fra er alt som før.
- [ ] Arp tændt: pads spilles én tone ad gangen, op gennem akkorden. Slip pad'en: arpen stopper straks.
- [ ] MIDI-tastaturet spiller arp. Klik på en blok spiller arp.
- [ ] Rundgangen (Play): hver akkord spiller sin arp og starter forfra ved hver ny blok.
- [ ] Drag/Save MIDI med Arp tændt: filen har arpens toner (filnavn med "-Arp"), og den lyder som du hørte den.
- [ ] Slå Arp fra midt i afspilning: ingen hængende noter. Stop DAW'en: ingen hængende noter.
- [ ] Panelet (de tre prikker ved Arp-knappen): Pattern, Speed, rytme-ringen (træk i ringen), forvalgene, Notes/Steps, 1 eller 2 oktaver. Er det til at forstå uden tekst?
- [ ] Tempoændringer i DAW'en mens arpen spiller.
- [ ] Preview-lyden ved korte arp-noter: er lyden brugbar, eller skal Pad-lyden have hurtigere anslag?

## 3. Tidligere ting, der stadig venter

- [ ] Milepæl 9: alle 8 stemninger i Mood-menuen. Tryk Suggest flere gange pr. stemning: giver de fleste klik noget brugbart?
- [ ] Spørgsmål: skal Suggest måtte bruge farveakkorder (maj7, 9, sus2, m7), selvom de ikke findes som pads?
      (Nu bruger den kun de 7 treklange fra pads. Dreamy, Deep og Soulful bliver derfor mest forskellige i rytme.)
- [ ] De 6 preview-lyde (Pad, Strings, Keys, Organ, Stab, Pluck): brugbare og forskellige?
- [ ] Mood og Sound huskes i projektet.
- [ ] Afspilning uden DAW (Play med stoppet DAW) og overtagelsen, når DAW'en starter og stopper.
- [ ] Quick-text med oktaver: `Am+1 F-1 C G+2:2`, derefter Enter.
- [ ] Det nye udseende: er farverne, knapperne og logoet i mål?

## 4. Kan ikke testes her

- [ ] Windows og Ableton Live: kræver CI, der bygger en Windows-release (kopieres fra SubForge).
- [ ] pluginval med GUI-tests: pluginval er kørt på niveau 10 uden GUI (SUCCESS). GUI-testene kræver en skærm.
