# Progrezzor

Jam. Capture. Drag. Done.

Et meget nemt MIDI-akkordværktøj til alle genrer: jam, find en rundgang, træk MIDI ud, færdig.
Brugeren skal ikke kunne musikteori.

## Fokus

To ting vejer tungere end alt andet:

1. Visuelt enkelt at overskue og meget let at bruge. Inspirationen må aldrig forsvinde, fordi værktøjet er besværligt.
2. Rigtig gode forslag til brugbare rundgange.

Testen for hver funktion: Kan man bruge den uden at tænke? Hvis ikke, skal den gemmes væk eller ud.

## Arbejdsform

- Henrik er produktejer. Han har begrænset C++-erfaring, så forklar valg enkelt.
- Arbejd i én milepæl ad gangen. Stop efter hver milepæl, fortæl hvad han skal teste, og vent på hans OK.
- Tilføj ikke funktioner uden for den aktuelle milepæl. Skriv ideer i BACKLOG.md i stedet.
- Søsterprojekter må læses, men aldrig ændres: ../SubForge, ../SpaceForge og ../seqverse.
- Genbrug CI, release-opsætning, JUCE-version og clap-juce-extensions-version fra SubForge. Opdater ikke versionerne på eget initiativ.
- Skriv altid til filer i dette projekt, aldrig i søsterprojekterne.

## Licens og tekster

- AGPLv3, som SubForge og SpaceForge.
- Kun lovligt licenseret kode og lyd.
- Biblioteket med rundgange skrives fra bunden. Kopier aldrig presets, akkordpakker, lister, tekster eller design fra andre plugins.
- Forslag må ikke navngives efter eller efterligne bestemte sange eller kunstnere.
- Ingen varemærker i brugerrettede tekster (GUI, tooltips, dokumentation, changelog).
- Mac-builds er usignerede, og ingen tekst må antyde, at det ændrer sig.
- Tekster i plugin'et er på engelsk og uden musikteori-sprog, hvor det kan undgås.

## Skærmen

Alt ligger på én skærm uden faner.

- Topbjælke: Toneart og skala, Lock to scale (🔒), Mood, stemme og en tydelig 🎲 Suggest-knap.
- Midten: 7 store pads og yderst til højre en smallere blank pad (Rest, en pause), der sammen fylder hele bredden. Padsene fylder cirka 49 % af højden mellem topbjælken og bjælken. De skal kunne hamres på med mus eller MIDI.
- Bund: Rundgangsbjælken, et kort for sig med en mørk bakke til blokkene: Play, blokke (ombrydes i flere rækker), Rec (til/fra), Fortryd, Gendan, ✏️ Quick-text (fast plads i højre side) og Drag.

Regler:

- Skrift (Henrik, 2026-11-08): Små tekster (under 17 px) er Inter SemiBold (SIL OFL, resources/fonts/Inter), fordi Chakra Petch går flosset på skærmen i små størrelser. Chakra Petch bruges kun til store navne (logo, akkorder, Suggest). Før det: Chakra Petch er detaljeret og ser flosset ud under cirka 13 px, så al tekst i knapper, tiles, blokke og labels er mindst 13 px (tooltips 17). Tekst krympes ikke sidelæns, hvis det kan undgås: felterne gøres bredere, eller en hel gruppe får samme, mindre størrelse (som mønsternavnene). Små tekster som "1/2 bar" kortes til "1/2" i smalle blokke.
- Tekst i knapper, felter og blokke holder altid mindst 8 px fra kanten og krymper lidt (ned til 70 %) i stedet for at røre kanten, uanset skrifttype (hjælpefunktionen theme::drawFitted). Tooltips er smalle (cirka 210 px tekst i stor skrift (17), flere linier), står tydeligt forskudt fra musen og har en farvet kant. Settings har "Show tooltips" (til som standard, gemt for alle projekter).
- Første klik på en pad giver lyd, uden routing eller opsætning.
- Gode standarder, så man kan spille med det samme. Standard er C Major og stemningen Bright (den mest almindelige og enkle). Ændret 2026-11-07 efter Henriks ønske.
- Pads og blokke viser akkordnavnet og, småt, akkordens plads i tonearten som romertal (I, ii, III, IV, v, VI, VII, ii° for formindsket). Henrik ønskede det 2026-11-07. Store bogstaver er dur, små er mol. Ingen andre teoriord.
- Pads lyser op, når de spilles, både fra mus og MIDI.
- Avancerede ting er skjulte. ⚙️ på pads og 🔒 på blokke vises først ved mouseover.
- Ingen dialogbokse i det normale flow. Fortryd og gendan virker altid.

## Teknisk fundament

- C++ med JUCE 8 og CMake.
- Formater: VST3 og CLAP. Linux og Windows først, macOS som eksperimentel build. AU er ikke med i første version.
- Plugin'ets hovedopgave er at generere MIDI, ligesom Seqverse. Preview-lyden er en hjælp.
- Faste flag: NEEDS_MIDI_INPUT TRUE, NEEDS_MIDI_OUTPUT TRUE, IS_MIDI_EFFECT FALSE. Stereo-output findes fra dag ét, så preview-lyden har en vej ud.
- Plugin-type (besluttet af Henrik 2026-11-08): VST3 er et Instrument (IS_SYNTH TRUE), og CLAP er en MIDI-effekt (CLAP-feature note-effect). Det er altså to forskellige typer, og det er med vilje. Skift dem ikke uden Henriks OK. Routingen i DAW'erne skrives i guiden, når den er testet.
- Al MIDI-output ligger på kanal 1.

## Genbrug fra Seqverse

Kopier det, der genbruges, ind i dette projekt, og tilpas det. Læs kun de filer, der er brug for i den aktuelle milepæl.

- src/Music.h: tangenter, skalaer og noteNumber(). Tag ikke arpeggio- og rytmedelene med.
- src/Take.h og filskrivningen i src/PluginProcessor.cpp: MIDI-fil, takes-mappe og træk til DAW'en.
- Tidslogikken i src/PluginProcessor.cpp: låsning til DAW'ens PPQ-position.
- tests/clap_test_host.cpp: test af noter og hængende noter uden DAW. Tilpas scenarierne til Progrezzor.
- tests/ui_test.cpp: test af GUI og PNG-billeder af vinduet uden at åbne et vindue.
- CMakeLists.txt: opsætning af VST3, CLAP og test.
- src/DarkLookAndFeel.h og resources/fonts: tema og skrifttyper. Skrifttypernes licens følger med.

Akkordlogik, stemmeføring, rundgange, Suggest og Quick-text er nyt. CI og release kommer fra SubForge.

## Arkitektur

- ChordEngine, tekstparseren og biblioteket er ren C++ uden JUCE-afhængigheder og har enhedstest.
- Audio-tråden må ikke allokere, låse eller vente. GUI og audio-tråd taler sammen via lock-free køer.
- En akkord gemmes som: afstand fra tonika i halvtoner, akkordtype, omvending. Aldrig som pad-nummer eller færdige MIDI-noter.
- Blokke skal kunne rumme alle akkorder, også dem uden for skalaen.
- En rundgang flytter med, når toneart ændres, og ændrer sig ikke, når en pad får en ny variant.
- Længder gemmes i beats. Mindstelængden er én samlet konstant, så ¼ takt kan tilføjes senere uden at bryde gamle projekter.
- Al tilstand gemmes og hentes med DAW-projektet.

## Akkordregler

- De 7 pads bygges ved at stable skalatoner, så de altid passer til skalaen.
- Skalaer i første version: Minor (standard), Major, Dorian, Phrygian.
- Akkordtyper: dur, m, 5, 7, m7, maj7, 9, m9, add9, sus2, sus4, dim og omvendinger.
- Sus4 tilbydes med låsen til kun, hvis den rene kvart findes i skalaen. Sus2 kun, hvis den store sekund findes.
- Akkordnavne staves efter tonearten (Bb frem for A#, når tonearten bruger b).
- Ingen automatisk stemmeføring og ingen magi (besluttet af Henrik 2026-11-07). En akkord har altid de samme toner, uanset hvad der blev spillet før: grundstilling, eller den omvending brugeren selv har valgt. Tonikaens grundtone placeres i området F#3 til F4 (MIDI 54 til 65). Enhver anden akkords grundtone ligger 0 til 11 halvtoner over tonikaens, ud fra akkordens afstand til tonika (også uden for skalaen), så pad 1 til 7 altid går opad (C4 D4 E4 F4 G4 A4 B4 i C-dur, A3 til G4 i A-mol). I grundstilling er grundtonen den dybeste tone.
- Bastone (Henrik, 2026-11-08): Settings har "Bass note", FRA som standard, og valget gemmes med projektet (parameteren bassNote). Er den til, spilles den dybeste tones tonenavn som en ekstra tone i området E2 til D#3 under akkorden: grundtonen i grundstilling, bassen ved en valgt omvending (E ved C/E). Akkorden ændres ikke, og samme pad giver altid samme toner. Oct-knapperne og akkordens egen oktav flytter bastonen med. Med Arp til indgår bastonen ikke i arpeggioen, men ligger som én lang tone under den, så længe akkorden holdes. Loop og MIDI-fil har den samme bastone (en lang tone pr. blok). I Learn spiller tangenterne kun deres egne toner, uden bas.
- En lille klaviatur i højre panel viser, hvor akkorderne ligger (det lyse felt, som flytter sig med Settings og Oct-knapperne) og viser midt-C som en blå streg med "Mid C", der altid står samme sted, uanset hvad lejet er. Tonerne, der spiller, lyser grønt for de toner, der spiller lige nu.
- Toneleje: Oct- og Oct+ i højre panel flytter alt (pads, rundgang og MIDI-fil) en oktav ad gangen, -2 til +2, og panelet viser trinnet (Oct 0, Oct +1). Tandhjulet (Settings) vælger, hvor akkorderne som regel ligger: normalt omkring midt-C, op til to oktaver lavere eller højere. Det er brugerens eget valg, gemt i en fil for sig (settings.xml i systemets sted for programdata, eller mappen i PROGREZZOR_SETTINGS_FOLDER), ens i alle projekter og ikke en del af projektet. Oct-knapperne tæller oven i det.

## Lock to scale

- Slået til som standard.
- Pads følger altid skalaen. 🎲 vælger kun rundgange i skalaen. Pad-varianter, brugeren selv har valgt, bruges alligevel.
- ⚙️ viser kun passende varianter, når låsen er til, og alle varianter, når den er fra. Når låsen er til, har akkordmenuen (⚙️ på pads og menuen på blokke) et sidste punkt, adskilt fra resten: "More chords outside the key...". Det slår låsen fra og åbner samme menu igen med alle akkorder. Er låsen fra, vises punktet ikke (Henrik, 2026-11-08).
- Quick-text: En akkord uden for skalaen bliver en markeret, deaktiveret blok, når låsen er til. Vis en kort besked med en Allow-knap, der slår låsen fra, så akkorden spiller med det samme.
- Plugin'et må aldrig selv rette en akkord til en anden.
- Skift af toneart eller skala ændrer ikke eksisterende blokke uden for skalaen. De bliver kun markeret.

## Mood

Én menu med 8 stemninger, navngivet efter følelser og brugbar på tværs af genrer:

- Bright: glad og åben, rene treklange.
- Warm: tryg og blød, treklange, add9, maj7.
- Dreamy: svævende, maj7, 9, sus2.
- Melancholic: trist og smuk, mol, m7, add9.
- Dark: tung og truende, mol, få toner, tæt på tonika.
- Deep: hypnotisk og rummelig, m7, m9, sus.
- Driving: fremad og energisk, treklange og powerchords.
- Soulful: rig og varm, 7, m7, 9.

Mood bestemmer bibliotekets rundgange, akkordfarven og eventuelt registeret. Mood ændrer aldrig toneart, skala eller stemme.

## Forslag

- 🎲 Suggest lægger med ét klik en hel rundgang i bjælken. Blokke med 🔒 bliver liggende, og resten skiftes ud.
- Biblioteket er håndskrevet og gemt som data, uafhængigt af toneart.
- Mål: cirka 20 rundgange pr. stemning, både til dur og mol.
- Dorian og Phrygian bruger de rundgange fra mol, der passer ind i skalaen.
- Pads, der passer godt efter den sidste akkord, lyser op. Reglerne ligger i en håndskrevet tabel. Højst 3 pads lyser ad gangen, og alle pads virker altid.
- Omvendinger (Henrik, 2026-11-08): Biblioteket kan angive en omvending på en akkord (C G/B Am F, gående bas), kun hvor den giver en hørbar forbedring, og kun i de stemninger, hvor den passer. Blokken viser navnet med skråstreg (G/B). Padsene ændres ikke, og en variant, brugeren har valgt på en pad, vinder over forslaget. Settings har "Inversions in suggestions" (til som standard, gemt for alle projekter); fra giver Suggest kun rundgange i grundstilling. Loop og MIDI-fil bruger samme toner som det, man hører.
- Kvalitetsmål: Af 10 klik på 🎲 skal mindst 8 give noget, Henrik faktisk gider bruge.

## Regler for MIDI

- Hængende noter må aldrig forekomme.
- Hver note har en tæller. Note off sendes først, når den sidste kilde slipper noten.
- Note off sendes for præcis de noter, der blev startet, også hvis toneart eller variant ændres, mens en pad holdes.
- Alle aktive noter stoppes ved transport stop, bypass og når plugin'et lukkes.
- Keyboard: Hvide tangenter i alle oktaver spiller pad 1 til 7 (C til B). Sorte tangenter ignoreres. Inputnoter sendes ikke videre. Anslagsstyrken bruges.

## Rundgangen

- Hver afspillet akkord kommer i historikken. Samme akkord to gange i træk tæller som to.
- Rec (før kaldt Capture, omdøbt 2026-11-07 efter Henriks ønske) er en TÆNDT/SLUKKET knap med en rød prik, på prøve på branchen feature/capture-toggle. Er den tændt (lyser med accentfarven), bliver hver akkord, der spilles, sat sidst i rundgangen som én blok på 1 takt, uanset om den spilles med mus på en pad eller fra MIDI-tastaturet. Er den slukket, sker der ingenting. Den kan kun tændes, når Play er slukket, og Play, Quick-text og Learn slukker den. Fortryd tager akkorderne væk igen, en ad gangen. Trækkes en pad ned i bjælken, mens Rec er tændt, tæller akkorden kun én gang (der, hvor den slippes). Den gamle Capture-knap (de sidste 8) er væk; historiestriben ligger på feature/capture-history.
- Pause (Henrik, 2026-11-07): en blok kan være stilhed. Den blanke pad yderst til højre (smallere end de andre, dæmpet grå med nodepausen for 1 takt, en hel pause, og ordet "Rest") trækkes ned i bjælken som en akkord: mellem blokke indsætter den en pause på 1 takt, over midten af en blok gør den blokken til en pause (længden bevares). En akkord, der trækkes over en pause, gør den til en akkord igen. Klik på den blanke pad giver hverken lyd eller ændring, men er Rec tændt, lægger et klik en pause bagerst (trækkes den ned, tæller den kun én gang). En pauseblok er grå og viser pausetegnet i nodeskrift efter længden (½ takt: halv pause, 1 takt: hel pause, 1½ takt: begge, 2 takter eller mere: flertaktspause med antal takter over) og længden som tekst (Henrik ønskede nodetegn, 2026-11-08, fordi et almindeligt pause-ikon giver forkerte forventninger), har ingen oktavpile, ingen romertal og kan flyttes, kopieres, låses, slettes og strækkes som andre blokke. I loopet og i MIDI-filen er der stilhed i pausens længde (filnavnet får "Rest"), den kan ikke spilles ved klik i Play, og Learn springer den over. Suggest lægger aldrig pauser ind (Henrik), men en pause uden 🔒 bliver erstattet, som alle andre blokke. Gemmes som egen markering i projektet (tilstand version 11). Quick-text: R eller Rest, med længde og antal som for en akkord (R, R:2, Rest*2). Bindestreg kan ikke bruges, fordi den skiller akkorder.
- Standardlængde er 1 takt. Mindste længde er ½ takt, største er 8 takter. Længder ændres ved at trække i blokkens højre kant med trin på ½ takt.
- Rundgangen kan have alle samlede længder, også ulige antal takter.
- Træk en blok for at flytte den. Hold Ctrl (Option på Mac) nede for at duplikere.
- Træk en pad ned i bjælken: mellem blokke eller yderst i en blok indsætter den, over midten af en blok erstatter den (blokken lyser op). Et klik på en pad erstatter aldrig noget. Der er ingen +-knap.
- Blokmenu (Henrik, 2026-11-08): Højreklik på en blok (eller det lille ⚙️, som vises ved mouseover nederst til venstre) åbner en menu: akkordtype (samme liste som ⚙️ på pads, og Lock to scale gælder på samme måde), omvending, Octave up og down, længde (½, 1, 2, 4 takter), Lock, Duplicate og til sidst Delete, adskilt fra resten. Højreklik sletter ikke direkte. Delete-tasten sletter stadig en markeret blok. En ændring gælder kun blokken, aldrig pad'en, og hver ændring er ét trin for Fortryd. En pause har kun længde, Duplicate og Delete. I Play er der ingen menu.
- Play-knappen bestemmer alt: Play slukket = rundgangen kan redigeres (der er ingen Edit-knap). Play tændt = blokkene spiller kun deres akkord, når de trykkes, og intet kan ændres.
- Kører DAW'en, følger loopet DAW'ens position (PPQ). Det skal klare DAW-loops, hop i tid og tempoændringer. Står DAW'en stille, starter Play plug-in'ens eget ur fra første akkord (DAW'ens tempo, ellers 120 BPM). Starter DAW'en, overtager den.
- Suggest spørger i et lille kort midt i rundgangsvinduet ("Replace the chords in the bar?" med en Replace-knap, ingen dialogboks), før den overskriver akkorder, brugeren selv har lavet, hvis der er blokke uden 🔒. Den spørger ikke, når bjælken er tom, kun har låste blokke, eller stadig er det, Suggest selv lagde der.
- Akkordskift skal lande præcis på taktstregen, også midt i en audio-buffer.
- Holdes en pad, mens loopet kører, har pad'en forrang, og loopet tier imens.

## Arp

Besluttet af Henrik 2026-11-07. Arp-knappen (nederst til venstre i bjælken, under Play) tænder og slukker den. Slået fra er alt som før.

- Er Arp tændt, spilles ALT som arp: pads, MIDI-tastatur, klik på blokke og rundgangen. MIDI-filen (Drag og Save MIDI) har arpen, filnavnet får "-Arp". Slås den fra, er det akkorder igen.
- En arp spiller tonerne i den aktuelle akkord (evt. over to oktaver) én ad gangen. Hver akkord starter sin arp forfra, og noderne afgøres af trinnummeret alene, også Random, så lyd og MIDI-fil er ens.
- De tre prikker ved Arp-knappen åbner et lille panel uden tal og teori: Pattern (Up, Down, Up/Down, Random som små tegninger), Speed (4 trin) og Rhythm (en ring med prikker, hvor de tændte spiller; træk i ringen for at dreje den; forvalg Even, Offbeat, Gallop, Spread; antal toner og længde).
- Rytmen er et Euklid-mønster og koden er tilpasset fra ../seqverse (Music.h), uden arp-delene fra SeqVerses skalaer.
- Ingen hængende noter: hver arp-node har sin egen note off, og alt stopper ved slip, transport stop, bypass og når Arp skiftes.

## Akkord-rytme

Besluttet af Henrik 2026-11-08. Hele akkorden slås an i et rytmisk mønster. Det bruger arp'ens rytmemotor og det PPQ-låste gitter fra arp-rettelsen.

- Øverst i arp-panelet er der en kontakt Hold | Arp | Rhythm. Hold er som når arp er slået fra. Arp-knappen i bjælken tænder og slukker stadig, og husker sidste valg (Arp eller Rhythm). Parametrene er arpOn (tændt) og arpRhythm (Rhythm i stedet for Arp), så gamle projekter åbner som før.
- Rhythm har tre valg: Pattern, Speed (1/8, 1/16) og Length (Short, Medium, Long).
- 12 faste mønstre, håndskrevet for både 1/8 og 1/16: Offbeat, Stab, Double stab, Skank, Eighths, Push, Gallop, Charleston, Tresillo, Clave, Pulse og Euclid (bruger arp'ens Hits og Rotation, vist som Hits og Turn).
- Et mønster er én takt på 4 slag: 8 pladser ved 1/8, 16 ved 1/16. Det starter forfra på hver blok og følger værtens gitter, så et slag kun hører til en blok, hvis det ligger før blokkens slutning. Ingen tilfældighed. Pads uden loop starter mønstret ved tryk.
- Let accent på første plads i takten (de andre slag har 82 % af anslagsstyrken). Short er 30 % af en plads, Medium 80 %, Long varer til næste slag (92 %).
- Bastonen, hvis slået til, ligger som én lang tone under rytmen.
- Loop og MIDI-fil giver de samme anslag, til samplen (testet ved flere tempi og buffer-størrelser over 64 takter). Filnavnet får "-Rhythm".
- Indstillingerne gemmes med projektet. I Learn er rytmen slået fra, ligesom arpen.

## Learn (på branchen feature/learn-mode)

Henrik ønskede en Learn-knap (2026-11-07), og at brugeren skal spille de RIGTIGE TANGENTER på et MIDI-tastatur, ikke bare trykke på pads. Knappen "Learn" nederst til venstre i bjælken tænder og slukker den.

- Brugeren holder tonerne i den næste akkord nede samtidig på et MIDI-tastatur. Den næste akkord er markeret i bjælken (pulserende kant og en lille pil, med "Play Am on your keyboard (3 of 3)" i overskriften).
- Rigtig betyder, at præcis tonerne i akkorden holdes nede, i en hvilken som helst oktav og rækkefølge (så en hvilken som helst omvending og fordobling er rigtig). Ekstra toner gør det forkert. Klik på pads tæller ikke.
- Rigtig akkord: blokken får et grønt mærke og målet rykker videre. Tangenterne skal først slippes, før næste akkord bedømmes. Forkert: blokken får en rød kant et øjeblik, når de holdes lidt, eller når tangenterne slippes uden at have været rigtige. Toner der kommer én ad gangen er ikke forkerte, før der er nok tangenter til en akkord.
- I Learn spiller tangenterne deres egne toner (de er ikke pads), også ud som MIDI.
- Hint: efter to forkerte forsøg, eller fra start hvis det er slået til i Settings ("Show hints from the start", gemt for alle projekter), vises et udsnit af et klaviatur nederst i rundgangsvinduet med de taster, der skal holdes nede (lyserøde, med nodenavne). De taster, brugeren holder, lyser grønt (rigtig) eller blåt (ikke en del af akkorden). De rigtige toner lyser også på det lille klaviatur i højre panel.
- Den venter på brugeren, der er intet tempo. Loopet er tavst og arpen slået fra. Ændres bjælken, starter forfra.
- Ikke med endnu (BACKLOG.md): spil med i takt, og at høre rundgangen først.

## Quick-text

✏️ gør bjælken til en tekstlinje med de aktuelle akkorder. Enter anvender, Esc fortryder. Teksten ændrer kun rundgangen, ikke de 7 pads.

- Am: 1 blok på 1 takt.
- Am*2: genvej til Am Am, altså 2 blokke på 1 takt hver.
- Am:2: 1 blok på 2 takter.
- Am:0.5: 1 blok på ½ takt.
- Am:0.5*2: 2 blokke på ½ takt hver.

Reglen er: * betyder antal, og : betyder længde. Tekstfeltet viser altid blokkene præcis, som de er, så en blok på 2 takter vises som Am:2. En stavefejl markeres med rødt og stopper ikke resten.

## Stemmer

- Preview-lyde bygges som små indbyggede synths uden samples. MIDI-filen indeholder kun noter.
- Soft Pad er standard og kommer først (hed før Pad; omdøbt 2026-11-08, så den ikke forveksles med akkord-knapperne; gemte projekter bruger tallet og er upåvirkede). Lyd-vælgeren i topbjælken har et højttaler-ikon, og med Off er ikonet streget over og teksten dæmpet. Off står under en skillelinje. Tooltip: "Preview sound. Choose Off when Progrezzor plays another instrument." Settings har under Sound "Preview sound" med flueben, der slår lyden fra og til (til vælger den sidst valgte lyd). Preview er til som standard.
- Derefter Strings, Keys (FM), Organ, Stab og Pluck.
- Klaver er droppet for nu.

## MIDI-eksport

- Gem altid i en fast, synlig mappe. Slet ikke filen efter træk, for DAW'en kan læse den senere.
- Byg en ny MidiFile for hver eksport.
- Filnavn indeholder akkorderne, fx Am-F-C-G.mid.
- Træk-og-slip bruges, hvor det virker. Knapperne Save MIDI og Open Folder findes på alle platforme.
- Filen skal bekræftes importeret korrekt i Bitwig, REAPER og Ableton Live.

## Milepæle

0. Tomt plugin bygget som mulighed A. Knap sender en testakkord, og en simpel testtone viser, om preview-lyd slipper igennem. Test i Bitwig, REAPER og Ableton Live: Kan det indsættes, når MIDI frem til en synth, og kan man høre lyden? Prøv kun mulighed B, hvis A fejler. Skriv valget i denne fil, og vent på Henriks OK.
1. ChordEngine (akkordernes toner) og enhedstest, uden GUI.
2. 7 store pads, toneart, skala, Soft Pad-stemmen, MIDI ud og keyboard-input. Ingen hængende noter.
3. Rundgangsbjælken med blokke, 🎲 Suggest og et lille bibliotek til Melancholic. Her testes kvaliteten af forslag tidligt.
4. Loop synkroniseret med DAW'en.
5. Redigering: længder, træk, duplikér, tilføj, slet, 🔒, fortryd og gendan og variant-menu. (Capture indgik, men er fjernet, se Rundgangen.)
6. MIDI-eksport.
7. Lock to scale og lysende pads.
8. Quick-text.
9. Alle 8 stemninger med fuldt bibliotek og de øvrige preview-stemmer.

## Beslutning fra milepæl 0

Mulighed A blev valgt først: IS_SYNTH FALSE og CLAP-feature note-effect. Testet på Linux i Bitwig og REAPER: Plugin'et kan indsættes på et MIDI-spor, MIDI kommer ud til en synth efter det, og testtonen kan høres. Ableton Live og Windows er ikke testet endnu (Ableton findes ikke på Linux) og testes, når CI kan bygge en Windows-release. Se BACKLOG.md. Indtil da udvikles og testes kun på Linux.

Ændret 2026-11-08 efter Henriks ønske: VST3 er nu Instrument (IS_SYNTH TRUE), mens CLAP stadig er en MIDI-effekt (note-effect). Se Teknisk fundament.

## Beslutning fra milepæl 3

Suggest bruger kun akkorder, der også findes som pads (de 7 treklange), men må give dem som omvending (se Forslag). Alt i bjælken kan dermed spilles direkte på en pad, bortset fra selve omvendingen. Biblioteket har stadig m7-, maj7- og add9-rundgange, men Suggest vælger dem ikke. De kan bruges, når variant-menuen kommer i milepæl 5. Besluttet af Henrik efter test af milepæl 3.

## Versioner

- Første udgivelse er 1.0.0, når Henrik selv har testet og er tilfreds. Der laves ingen 0.x- eller beta-udgivelser, for udgivelser må ikke afhænge af brugernes feedback.
- Før 1.0.0 testes i Bitwig og REAPER på Linux og mindst én gang på Windows.
- 1.0.x er fejlrettelser. 1.x er nye funktioner eller presets. 2.0 kun hvis gamle projekter ikke længere åbner ens.
- Plugin-ID'er og parametre må aldrig ændres, så gemte projekter altid kan åbnes.
- Lav aldrig tag, push eller release, før Henrik siger til.

## Definition på færdig

- Bygger uden nye advarsler.
- Enhedstest er grønne.
- pluginval består på højeste niveau, som SubForge-opsætningen tillader.
- Henrik har fået en kort testliste og godkendt milepælen.
