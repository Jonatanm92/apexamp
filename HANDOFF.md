# Överlämning: Apex-sviten (ApexAmp / Thallbyssal + Apex Drop)

Läs den här filen först i en ny session. Den beskriver läget efter den första
långa bygg-sessionen (oktober 2026), vad som återstår och reglerna som gäller.

## Mål

Bygga och sälja det bästa djent/thall-förstärkarpluginet på marknaden:
**ApexAmp** med editorn **Thallbyssal**, plus pitch-shiftern **Apex Drop**.
Ägaren (Jonatan) vill att Claude agerar expert och gör det den bedömer som
bäst, och siktar på något klart bättre än konkurrenterna (Neural DSP,
Thall Amp, Graphene m.fl.).

**Språk:** svara alltid på **svenska**. Ägaren dikterar med Wispr Flow, som
ibland transkriberar till norska; svara ändå på svenska. Kod, kommentarer,
commit-meddelanden och dokumentation i repot skrivs på engelska.

## Repo och gren

- Repo: `Jonatanm92/apexamp`.
- Arbetsgren: `ccr-3cb9af12-bjexa0`. Bas: `apexamp-beta`.
- **PR #3** ("Apex suite: pitch engine, Apex Drop, rig effects and the
  Thallbyssal editor") innehåller allt arbete. Läget vid överlämningen:
  - CI grönt på Windows, macOS och Linux.
  - Mergebar och utan konflikter.
  - Alla granskningstrådar är besvarade och lösta.
  - PR:en väntar bara på att ägaren mergar.
- Om PR #3 redan är mergad: starta om arbetsgrenen från senaste
  `apexamp-beta` och öppna en ny PR för nytt arbete. Återanvänd aldrig en
  mergad PR.
- Version: filen `VERSION` (nu 0.11.0) styr plugin, installerare och About.

## Vad som finns

**Struktur**

| Mapp | Innehåll |
|---|---|
| `libs/apex-dsp` | Ren C++17-DSP utan JUCE: pitchmotor, rig-effekter, The Legion |
| `libs/apex-ui` | Designsystem i JUCE: material, kontroller, presets, A/B, undo, tuner, licens-UI |
| `libs/apex-licence` | Ed25519-licensnycklar (TweetNaCl), `apex_keygen` |
| `plugins/ApexAmp` | NAM-baserat förstärkarplugin med Thallbyssal-editorn |
| `plugins/ApexDrop` | Fristående pitch-shifter (VST3/AU/Standalone) |
| `tools/snapshot` | Headless skärmdumpar av editorerna |
| `tools/demo` | Demoljud-renderare |
| `tools/keystudio` | Apex Key Studio, en webbsida för nyckelpar och licenser |
| `packaging/` | Installerare, EULA, tredjepartsnotiser |

**ApexAmp**

- Kedjan: Drop-pedal, Gate, Boost (4x översamplad), NAM-förstärkare (riggarna
  Bite/Body/Edge), Shape (Chug + Growl), amp-EQ och Depth, kabinett-IR med
  Fizz Tamer och High Cut, Echo och Abyss-reverb.
- **The Legion**: en kick på varje chug och en bas en oktav ned, live.
- **Riff capture**: de senaste 30 sekunderna DI, bas och kick. Dra RIFF DI,
  BASS eller KICK MIDI direkt in i DAW:en.
- **Noise gate**: stänger 60 dB på 25 ms. Den verkar både före och efter
  förstärkaren, nycklad från DI:n som en gate i effektloopen. Tystnad mellan
  snabba chugs mäter −74 dBFS.
- **Bypass**: latensjusterad. Echo-, reverb- och kabinettsvansar fortsätter
  att klinga ut.
- **Licens**: 14 dagars trial med allt påslaget. Därefter släpps DI:n igenom
  orörd tills en nyckel aktiveras; sessioner behåller sina inställningar.
  Nycklarna är signerade offline med Ed25519, och en nyckel låser upp alla
  DAW:ar på datorn.

**Försäljning (se `SELLING.md`)**

- Nycklar görs i Apex Key Studio eller med `apex_keygen`. En ctest kontrollerar
  att båda ger byte-identiska nycklar.
- Key Studio är också publicerad som artifact:
  https://claude.ai/artifact/Xx4aDmBJwtKisZofDPX4y3
- CI bygger installerare och laddar upp dem som artifacts `Installers-<plattform>`:
  - Windows: Inno Setup.
  - macOS: universell .pkg (arm64 + x86_64) för 10.15+, och CI kör `auval`.
  - Linux: .tar.gz.
- Signering och notarisering körs automatiskt när motsvarande secrets finns.

**Demoljud**: fem mp3-klipp gjordes med `apexamp_demo` (`tools/demo/README.md`).
De finns inte i repot, men samma verktyg kan rendera om dem.

## Bygga och testa

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release        # JUCE 8.0.4 hämtas via FetchContent
cmake --build build --parallel 4                 # obegränsat -j tar slut på minne
ctest --test-dir build --output-on-failure       # 10 sviter
```

- **Linux-beroenden:** se `README.md`. Node behövs för testet `keystudio_crosscheck`.
- **ctest-sviter:** `pitch_engine`, `rig_effects`, `licence_keys`,
  `keystudio_crosscheck`, `rig_audio`, `resample_streaming`, `riff_capture`,
  `licensing`, `noise_gate`, `bypass`.
- **pluginval:** kör med strictness 10 på båda VST3:orna (Linux, under Xvfb).
- **Skärmdumpar:** snapshot-verktyget i `tools/snapshot`, med `--unlock` för att
  hoppa över trial-skärmen.
- **Arbetssätt:** en buggfix ska ha ett test som först visas faila på den
  gamla koden.

## Säkerhetsregler (gäller alltid)

- Säljarens **hemliga** licensnyckel skapas bara lokalt hos ägaren, i Key
  Studio eller med `apex_keygen keypair`. Den skapas aldrig i en molnsession.
- Den hemliga nyckeln får aldrig klistras in i chatt, mejl eller GitHub, inte
  ens till Claude. Ägaren skickar bara den **publika** nyckeln (64 hex-tecken).
- Utvecklingsnyckelns hemlighet ligger medvetet öppet i
  `libs/apex-licence/dev/`. Byggen utan release-nyckel visar
  "DEVELOPMENT BUILD · NOT FOR SALE" och får aldrig säljas.

## Väntar på ägaren

1. **Publik nyckel**: när ägaren skickar den, gör så här:
   - Sätt den som standardvärde för `APEX_LICENCE_PUBLIC_KEY` i
     `libs/apex-licence/CMakeLists.txt`, eller som repo-variabel för CI.
   - Testerna skickar redan utvecklingsnyckeln explicit och fortsätter att
     fungera.
2. **Spärrlista**: erbjuden men inte besvarad. Det är en lista över
   läckta serienummer som följer med i uppdateringar och som pluginet vägrar.
   Föreslagen design:
   - Inkompilerad lista, t.ex. `libs/apex-licence/revoked.txt` som läses in
     av CMake till en header.
   - `verify` och `Licensing` avvisar serienummer på listan.
   - Tester i `licence_tests` och `licensing_check`.
   - En notis om spärrlistan i Key Studio och `apex_keygen`.
3. **Företagsnamn**: ska ersätta "PolychromeNext" (`COMPANY_NAME` i båda
   plugin-CMakeLists och `APEX_PUBLISHER`). Namnet ligger för nära PolyChrome DSP.
4. **Rättigheter till kabinett-IR:erna** `ir_ashen`, `ir_meshuggah` och
   `ir_pdi09`.
5. **Signering**:
   - Certifikat: Apple Developer ($99/år) och ett kodsigneringscertifikat för
     Windows.
   - Repo-secrets: se `SELLING.md`.
6. **Butik**: butiks-URL (`APEX_STORE_URL`), EULA-granskning
   (`packaging/EULA.txt`), artistkontakter, och beslut om namnet "Thallbyssal"
   ska behållas.
7. **Drop-överbländningen**: när Drop-pedalen trampas byter pluginet latens
   (8 ms bara med Drop på), så 12 ms-övergången blandar två tidslägen. Att
   ta bort det kräver 8 ms latens för alla. Detta är ägarens avvägning och står
   kvar tills vidare.

## Kända öppna saker

- pluginval kraschade (segfault vid avstängning) en gång på 9 körningar. Det
  gick inte att återskapa i gdb och behöver undersökas.
- Testa i Reaper, Logic, Ableton, Cubase och FL Studio på flera
  samplingsfrekvenser och buffertstorlekar (checklista i `SELLING.md`).
- Om knäckta kopior blir ett verkligt problem kan onlineaktivering läggas till
  senare. Se diskussionen nedan.

## Om piratkopiering (redan diskuterat)

- iLok knäcks också. Det kostar dessutom pengar, kräver PACE-avtal och
  skapar friktion för köparna.
- De signerade nycklarna gör att ingen kan göra en keygen utan den hemliga
  nyckeln. Läckta nycklar spärras med spärrlistan.
- Det som säljer är trialen, uppdateringar, nya riggar och presets, och ett
  rimligt pris.

## Konventioner

- **Commits**: conventional commits (`feat:`, `fix(ApexAmp):`, `ci:`,
  `style(...)`) på engelska. Avsluta med de attribueringsrader som sessionen
  anger. Inga modellnamn i commits eller kod.
- **Pushar**: bara till arbetsgrenen. Inga nya PR:er utan att ägaren ber om
  det.
- **Koden**: JUCE 8.0.4 och C++20 för plugins, C++17 för `apex-dsp` och
  `apex-licence`. Nya ljudfunktioner är neutrala eller avstängda som standard,
  så att befintliga sessioner och presets låter likadant.
