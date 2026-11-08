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
      (Nu: pad 1 til 7 går altid opad, fra fx C4 til B4 i C-dur.)
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

## 2c. Romertal og klaviaturet (2026-11-07)

- [ ] Padsene og blokkene viser romertal (I, ii, III ...). Er de til at læse, og er de i vejen? Prøv også en anden toneart og skala (C Major: I ii iii IV V vi vii°).
- [ ] Den lille klaviatur i højre panel: det lyse felt flytter sig, når du trykker Oct+/Oct-.
- [ ] Tonerne, der spiller, lyser grønt på klaviaturet (akkorder og arp).

## 2d. Midt-C og Settings (2026-11-07)

- [ ] Klaviaturet har en blå streg med "Mid C", som står samme sted, uanset hvad du sætter Oct til. Det orange felt flytter sig i forhold til den.
- [ ] Tandhjulet (Settings) i højre panel: vælg, hvor akkorderne som regel ligger (2 oktaver lavere til 2 højere end omkring midt-C).
- [ ] Settings huskes, når du lukker og åbner DAW'en, og er ens i alle projekter. (Filen ligger i ~/.config/Progrezzor/settings.xml.)
- [ ] Oct-knapperne tæller oven i Settings, og MIDI-filen har de samme toner, som du hører.

## 2e. Learn (branch feature/learn-mode)

Kræver et MIDI-tastatur.

- [ ] Learn-knappen (nederst til venstre, under Arp): tænd den med en rundgang i bjælken (fx fra Suggest). Den første blok pulserer, og overskriften siger "Play C on your keyboard (1 of 4)".
- [ ] Hold tonerne i akkorden nede på tastaturet (fx C, E og G): blokken får et grønt mærke, og næste pulserer. Prøv en anden oktav og en anden rækkefølge. Det skal også være rigtigt.
- [ ] Slip tangenterne før næste akkord. Hold du dem, tæller de ikke for den næste.
- [ ] Spil en forkert akkord og hold den lidt: blokken får en rød kant. Slip tangenterne uden at have ramt: også et forkert forsøg.
- [ ] Efter to fejl kommer et klaviaturudsnit med de rigtige taster lyserøde og nodenavne. Dine egne taster lyser grønne (rigtige) eller blå (forkerte).
- [ ] Settings > "Show hints from the start": udsnittet er der fra første forsøg.
- [ ] Klik på en pad i Learn: det tæller ikke, men den lyder.
- [ ] Du hører de toner, du spiller, og de sendes ud som MIDI.
- [ ] Når alle er spillet: "All done!". Tryk Learn igen for at starte forfra. Slå Learn fra: tangenterne er pads igen.
- [ ] Er klaviaturudsnittet stort og tydeligt nok? Skal det kunne spilles med musen, hvis man ikke har et tastatur?

## 2f. Omvendinger i Suggest (branch feature/suggest-inversions)
- [ ] Vælg Bright, C Major, og tryk Suggest mange gange. Nogle rundgange har en blok med skråstreg, fx G/B. Spil dem: bassen skal gå trinvis (C, B, A, F).
- [ ] Padsene er uændrede (ingen skråstreg på dem).
- [ ] Settings (tandhjulet): "Inversions in suggestions" er sat som standard. Slå den fra og tryk Suggest mange gange: aldrig en skråstreg. Luk og åbn projektet igen: valget er husket.
- [ ] Ændr en pad til en variant (fx G til G7), tryk Suggest: blokken med G bliver G7, ikke G/B.
- [ ] Træk MIDI ud: tonerne er de samme som dem, du hører, også for G/B (B er den dybeste tone).

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
