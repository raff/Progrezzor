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

## 2g. Bastone (branch feature/bass-note)
- [ ] Settings (tandhjulet): "Bass note" er slukket som standard. Tænd den og klik på pad 1 til 7: en dyb tone kommer under hver akkord (grundtonen), og akkorden er den samme som før.
- [ ] Skift til en blok med omvending (fx G/B): bastonen er B, den dybeste tone.
- [ ] Spil rundgangen med Bass note og Arp tændt: bastonen ligger som én lang tone under arpeggioen og skifter ved hver blok.
- [ ] Træk MIDI ud: bastonen er med, og lyden svarer til filen. Slå den fra: filen har kun akkorderne.
- [ ] Gem projektet med den tændt, åbn det igen: den er stadig tændt. Et nyt projekt har den slukket.
- [ ] Ingen hængende noter, når du slipper en pad, stopper transporten eller slår Bass note fra.

## 2h. Blokmenu (branch feature/edit-blocks)
- [ ] Højreklik på en blok: en menu åbner (ingen sletning). Den har akkordtyper, omvendinger, Octave up/down, længde, Lock, Duplicate og nederst Delete.
- [ ] Hold musen over en blok: et lille ⚙️ vises nederst til venstre og åbner den samme menu.
- [ ] Skift type, omvending, oktav og længde: kun den ene blok ændres, pad'en gør ikke. Slå Lock to scale til: kun passende typer vises.
- [ ] Fortryd (Ctrl+Z) tager hver ændring tilbage, en ad gangen.
- [ ] Delete-tasten sletter stadig en markeret blok.
- [ ] Højreklik på en pause: kun længde, Duplicate og Delete.
- [ ] I Play kan intet ændres, og der er ingen menu.
- [ ] Træk MIDI ud: filen har blokkens type, omvending, oktav og længde.

## 2i. Akkord-rytme (branch feature/chord-rhythm)
- [ ] Åbn arp-panelet (de tre prikker ved Arp). Øverst er en kontakt Hold | Arp | Rhythm. Hold: akkorden som én lang tone. Arp: som før.
- [ ] Vælg Rhythm: tolv mønstre vises som små rækker af prikker. Klik på dem og lyt: hele akkorden slås an i mønstret.
- [ ] Prøv Speed 1/8 og 1/16, og Length Short, Medium og Long.
- [ ] Euclid: Hits og Turn dukker op. Mønstret i bunden af panelet viser, hvad der spiller.
- [ ] Spil en rundgang med Rhythm: mønstret starter forfra i hver blok og holder takten, også ved andre tempi og når du hopper i tid. Første slag i takten er lidt kraftigere.
- [ ] Slå Bass note til: bastonen ligger som én lang tone under rytmen.
- [ ] Hold en pad uden loop: mønstret starter ved tryk. Slip: intet hænger.
- [ ] Træk MIDI ud: slagene er de samme som lyden (filnavn med -Rhythm). Gem og åbn projektet: valgene er der stadig.

## 2j. Tekst og tooltips (branch fix/text-padding)
- [ ] Gå over alle knapper (topbjælken, Rec, Arp, Learn, Drag, arp-panelet med Short/Medium/Long og mønsternavne, blokke): teksten har luft til kanten overalt.
- [ ] Hold musen over en knap: tooltip'en er smal, har flere linier og står tydeligt forskudt fra musen.
- [ ] Settings (tandhjulet) > Help > Show tooltips: slå fra, og ingen tooltips vises. Slå til igen. Valget huskes i alle projekter.

## 2k. Quick-text med takter (branch feature/quicktext-bars)
- [ ] Tryk på blyanten og skriv `| C | Am | F | G |`, Enter: fire blokke på 1 takt hver.
- [ ] `| C G | Am |`: to blokke på ½ takt og én på 1 takt. `| C | % | F | / |`: C, C, F, F.
- [ ] Indsæt et diagram over flere linjer (kopiér fra en tekst): linjeskift og mellemrum er lige meget.
- [ ] Den gamle notation virker stadig (`Am*2 F:2`). Et ukendt ord eller en tredje akkord i en takt er rødt, og resten virker.

## 2l. ChordPro-import (branch feature/chordpro-import)
- [ ] Settings (tandhjulet) > Import ChordPro...: en filvælger åbner (ikke i Play). Vælg en .cho-fil: akkorderne står som tekst i Quick-text-feltet, og rundgangen er uændret.
- [ ] Enter anvender (en takt pr. akkord), Esc fortryder. Fortryd (Ctrl+Z) tager det tilbage.
- [ ] En fil med afsnit (verse, chorus, bridge): en liste over afsnit, og kun det valgte kommer i feltet.
- [ ] En fil med {key: G}: beskeden "The song says it is in G major. Switch the key?" og en knap. Toneart skiftes kun, når du trykker på knappen.
- [ ] Træk en .cho-fil ind på rundgangen: det samme som via Settings.
- [ ] Ukendte akkorder (fx N.C.) er røde, og resten virker. En fil uden akkorder giver beskeden "No chords found in that file."
- [ ] En lang fil: de første 32 akkorder, og en besked om det.

## 2m. Gæt toneart (branch feature/guess-key)
- [ ] Stå i C Major, tryk på blyanten, skriv `G D C Em Am`, Enter: et kort spørger "These chords fit G Major." med "Switch to G Major" og "Keep C Major". Tonearten er uændret, til du trykker.
- [ ] Switch: tonearten skifter, og akkorderne er stadig G D C Em Am (ikke flyttet). Keep: alt er som før.
- [ ] `Am F C G` giver A Minor, `Am Dm E Am` giver A Minor, `C F G` giver C Major. En rundgang med en lånt akkord, der ikke passer, giver intet spørgsmål.
- [ ] Står du allerede i den toneart, sker der ingenting.
- [ ] ChordPro-fil med {key:}: ingen gætning. Uden {key:}: gættet kommer, når du trykker Enter.

## 2n. Eksport som tekst (branch feature/text-export)
- [ ] Settings > Copy as text: indsæt et sted (fx i en tekst): `| C | Am | F G | G |`. Åbn Quick-text, indsæt det, Enter: samme rundgang.
- [ ] Settings > Save ChordPro...: gem en .cho-fil og åbn den i en teksteditor: titel, toneart og et grid med slag. Import ChordPro igen: samme rundgang (halve takter, omvendinger og pauser).
- [ ] Begge punkter er slukket, når bjælken er tom.

## 2o. Keyboard: Chord roots (branch feature/keyboard-chord-roots)
- [ ] Settings > Keyboard: "White keys" er valgt som standard og virker som før.
- [ ] Vælg "Chord roots". I C Major spiller de hvide tangenter pad 1 til 7, og de sorte spiller ingenting.
- [ ] Skift til D Minor: D E F G A spiller pad 1 til 5, den sorte tangent Bb spiller pad 6, C spiller pad 7. C#, Eb, F#, Ab og B spiller ingenting.
- [ ] Prøv en toneart med sorte tangenter på padsene (fx Eb Major): Eb, Ab og Bb spiller hver sin pad.
- [ ] Hold en tangent og skift toneart: når du slipper, hænger der ingen note. Gem og åbn projektet: valget er husket.

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
