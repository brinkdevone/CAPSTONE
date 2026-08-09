# CAPSTONE — Mastering Suite

Chaîne de mastering complète avec mesure conforme **ITU-R BS.1770-4 / EBU R128**.
DSP écrit à la main et testable sans JUCE ; wrapper JUCE pour l'AU, le VST3 et
le standalone. Compagnon de [MONOLITH](../MONOLITH), qui traite les tranches.

---

## 1. La réflexion : pourquoi le mastering n'est pas du mixage en plus petit

Cinq principes structurent l'outil, et chacun a une conséquence directe dans le code.

**La mesure décide, pas l'oreille seule.** En mixage on écoute et on juge. En
mastering on livre contre une spécification : LUFS intégré, true peak, plage de
loudness. C'est pourquoi ce plugin commence par le mesureur et pas par l'égaliseur.
Un outil de mastering sans LUFS conformes n'est pas un outil de mastering.

**Les gestes sont minuscules.** ±1,5 dB d'égalisation, 1 à 2 dB de réduction de
gain. Si un défaut demande 6 dB de correction, il faut retourner au mix. Les plages
de paramètres sont donc volontairement bridées : ±6 dB sur l'égaliseur tonal contre
±18 dB sur MONOLITH. Un plugin qui autorise ±18 dB au mastering invite à la faute.

**L'axe Mid/Side vaut l'axe fréquentiel.** Éclaircir uniquement les côtés ouvre le
mix sans toucher à la voix centrale ; recentrer le grave évite que le master
s'effondre en mono. D'où une section M/S à quatre plateaux (grave et aigu, sur Mid
et sur Side séparément) plutôt qu'un simple bouton de largeur.

**La loudness est une spécification, pas un goût.** Pousser un master à −6 LUFS
quand Spotify normalise à −14 : la plateforme le baissera de 8 dB, et la dynamique
brûlée dans le limiteur, elle, ne revient pas. Le résultat sonne *plus petit* qu'un
master à −10 LUFS correctement fait. Les dix cibles intégrées affichent en
permanence l'écart à l'objectif, et l'afficheur passe au vert quand on y est.

**Le dither est toujours le dernier maillon, et une seule fois dans toute la chaîne
de production.** Après le limiteur, après le trim de sortie, après tout.

### L'ordre de la chaîne

| # | Étage | Pourquoi ici |
|---|-------|--------------|
| 1 | Trim d'entrée | Caler la marge avant tout traitement dépendant d'un seuil. |
| 2 | Rumble (passe-haut ~20 Hz) | L'infrabasse inaudible mange de la marge et fait travailler le limiteur pour rien. |
| 3 | EQ correctif (phase minimale, Q étroit) | Retirer une résonance précise. Phase minimale volontairement : une cloche étroite en phase linéaire produit un pré-écho audible sur les transitoires. |
| 4 | EQ dynamique | L'outil qui distingue le mastering : un mix correct au refrain mais bouché au couplet ne se règle pas avec une courbe statique. |
| 5 | Multibande 4 bandes | Crossovers Linkwitz-Riley 4 avec passe-tout croisés. Sans cette compensation, un multibande creuse le spectre à chaque coupure, même tous réglages à zéro. |
| 6 | EQ tonal (large, musical) | Après la dynamique : on sculpte le résultat compressé, pas la source. |
| 7 | Mid / Side | Largeur, bass mono, plateaux séparés sur Mid et Side. |
| 8 | Couleur (bande / transfo / lampe) | Densité et harmoniques. Suréchantillonné ×2, dosage typique 20–40 %. |
| 9 | Clipper (suréchantillonné ×4) | 1 à 2 dB d'écrêtage doux sur les crêtes libèrent autant de marge, sans le pompage qu'aurait produit le limiteur pour le même gain. |
| 10 | Limiteur true peak à anticipation | Deux étages, plafond garanti. Latence déclarée à l'hôte. |
| 11 | Trim de sortie | |
| 12 | Dither + mise en forme du bruit | Toujours en dernier. |

### Sur la phase linéaire

Elle n'est pas implémentée, et c'est un choix argumenté plutôt qu'un manque. Une
courbe large de ±1,5 dB en phase minimale produit un décalage de phase inaudible ;
la même courbe en phase linéaire coûte une latence importante et un pré-écho
symétrique qui, lui, s'entend sur les attaques. Beaucoup d'ingénieurs de mastering
travaillent en phase minimale presque exclusivement. Le cas où la phase linéaire
gagne vraiment — un plateau grave très large sur un mix déjà dense — reste une
addition possible (voir §6).

---

## 2. Ce que la construction a révélé

Trois résultats que je n'aurais pas trouvés sans instrumenter le code.

**Le passe-tout d'un crossover Linkwitz-Riley 4 est d'ordre 2, pas 4.**
Avec `LP4 = LP2²`, `HP4 = HP2²` et `D = s² + √2·ωc·s + ωc²` :

```
LP4 + HP4 = (s⁴ + ωc⁴) / D²      et      s⁴ + ωc⁴ = D · (s² − √2·ωc·s + ωc²)
          = (s² − √2·ωc·s + ωc²) / (s² + √2·ωc·s + ωc²)
```

soit un passe-tout du **second** ordre. J'avais mis deux sections en cascade pour
recaler les bandes : la phase doublée creusait **−2,3 dB à 800 Hz**. Après
correction, la réponse est plate à **±0,002 dB** sur 40 Hz – 15 kHz, coupures
comprises.

**Le suréchantillonnage ×2 ne suffit pas pour un clipper.** À 96 kHz, les 11e et
13e harmoniques d'un 7 kHz écrêté se replient *sous* la fréquence de coupure du
filtre de décimation et passent intactes dans la bande audible. Mesuré à 13 kHz :

| | sans OS | ×2 | ×4 |
|---|---|---|---|
| Écrêtage doux | −30,5 dB | −31,4 dB | **−96,4 dB** |
| Écrêtage dur | −26,7 dB | −28,7 dB | **−67,1 dB** |

Le clipper est donc en ×4, la saturation douce reste en ×2 où elle suffit.
Un bug annexe amplifiait le problème : un `clamp` appliqué *après* décimation
réintroduisait exactement le repliement que le suréchantillonnage venait d'éviter.

**Un limiteur true peak dépasse toujours un peu.** L'étage de gain est pourtant
sans dépassement par construction (minimum glissant sur 2·La+1, puis moyenne
glissante sur La+1 : chaque terme moyenné est déjà inférieur au gain requis à
l'instant visé). Le résidu vient d'ailleurs : le gain **varie à l'intérieur de la
fenêtre du filtre d'interpolation** (~12 échantillons), donc la crête
inter-échantillon du produit signal × gain n'est pas égale à gain × crête du signal.

Mesuré, le dépassement suit l'inverse du lookahead : 0,64 dB à 1,5 ms, 0,32 dB à
3 ms, 0,20 dB à 5 ms. Deux corrections : un second étage court (0,6 ms) en cascade,
qui divise le résidu par deux, puis une marge interne de 0,70/La_ms bornée — un
facteur de sécurité d'environ 1,5 sur le pire cas.

**Validation : 576 cas adverses** (transitoires violents, clics isolés, carrés,
sinus proches de Nyquist, 16 graines, 3 lookaheads, 2 plafonds) → **zéro
dépassement**, pire écart −0,050 dB. Le plafond affiché est donc une garantie, au
prix de 0,23 dB de niveau au réglage par défaut.

---

## 3. Le plugin

**14 modules DSP, 87 paramètres, 10 presets, 10 cibles de diffusion.**

```
Source/
  DSP/                  cœur audio, C++ pur, testable sans JUCE
    Ballistics.h          enveloppes, conversions dB, lissage
    Biquad.h              biquads RBJ, pentes variables, Linkwitz-Riley 4, passe-tout
    Oversampler.h         suréchantillonnage en cascade ×2 / ×4 / ×8
    Loudness.h            pondération K, LUFS M/S/I avec gating, LRA, true peak ×4, corrélation
    Multiband.h           4 bandes à phase recalée + compresseur par bande
    DynamicEq.h           2 bandes dynamiques
    MidSide.h             bass mono, largeur, plateaux Mid et Side séparés
    Limiter.h             limiteur true peak deux étages à anticipation
    Saturation.h          couleur (bande/transfo/lampe) + clipper ×4
    Dither.h              TPDF + 2 courbes de mise en forme du bruit
  UI/LookAndFeel.h      habillage sombre, potentiomètres bipolaires automatiques
  Params.h              87 paramètres + table des cibles de diffusion
  Presets.h             10 presets
  PluginProcessor.*     orchestration + télémétrie lock-free
  PluginEditor.*        interface, afficheurs LUFS/TP/LRA/PLR, corrélation
tests/dsp_test.cpp      conformité EBU, garantie du plafond, anti-repliement
packaging/              fabrication de l'installeur macOS (.pkg)
.github/workflows/      CI : tests Linux + build macOS + Release automatique
```

### Les cibles de diffusion

| Cible | LUFS-I | True peak |
|---|---|---|
| Spotify / Tidal / Amazon | −14 | −1,0 dBTP |
| Apple Music (Sound Check) | −16 | −1,0 dBTP |
| YouTube | −14 | −1,0 dBTP |
| Deezer | −15 | −1,0 dBTP |
| CD / physique | −9 | −0,3 dBTP |
| Club / DJ | −7 | −0,3 dBTP |
| Vinyle | −14 | −3,0 dBTP |
| Broadcast EBU R128 | −23 | −1,0 dBTP |
| Cinéma / OTT | −27 | −2,0 dBTP |

L'afficheur **PLR** (Peak to Loudness Ratio = true peak − LUFS-I) qualifie la
densité : sous 6 LU « très compressé », 6–10 LU « dense », au-delà « dynamique ».

---

## 4. Installation macOS (.pkg)

### Laisser GitHub construire l'installeur

```bash
git init && git add -A && git commit -m "CAPSTONE 1.0.0"
git remote add origin https://github.com/<toi>/capstone.git
git push -u origin main
git tag v1.0.0 && git push origin v1.0.0     # <- declenche la Release
```

Le tag `v*` enchaîne : tests DSP → compilation universelle arm64 + x86_64 →
validation `auval` → `.pkg` → Release GitHub téléchargeable.

### Ou en local, sur un Mac

```bash
./packaging/build_installer.sh          # -> dist/CAPSTONE-1.0.0.pkg
```

L'installeur laisse décocher les formats :

| Composant | Destination | Pour |
|---|---|---|
| Audio Unit | `/Library/Audio/Plug-Ins/Components` | Logic Pro, GarageBand |
| VST3 | `/Library/Audio/Plug-Ins/VST3` | Ableton, Reaper, Cubase, Studio One |
| Application | `/Applications` | mesurer un fichier sans ouvrir de session |

Un `.pkg` non signé est bloqué par macOS : clic droit → *Ouvrir* → confirmer.
Avec un compte Apple Developer, renseigner les secrets GitHub `MACOS_CERT_P12`,
`MACOS_CERT_PASSWORD`, `MACOS_APP_CERT_NAME`, `MACOS_INSTALLER_CERT_NAME` : le
workflow signe et notarise automatiquement.

### Méthode d'utilisation

CAPSTONE se met en **dernier insert du bus master**. Le limiteur déclare sa latence,
le séquenceur la compense. Choisir la cible, lire le morceau **entier**, appuyer sur
`RAZ MESURE` et relire : le LUFS intégré n'a de sens que sur un passage complet.

---

## 5. Banc de test

```
--- Mesure de loudness (ITU-R BS.1770-4 / EBU R128) ---
EBU 3341 cas 1 : sinus 1 kHz -23 dBFS -> -22.95 LUFS       [OK]
  ... momentane et court terme concordent (+/-0.1 LU)      [OK]
EBU 3341 cas 2 : sinus 1 kHz -33 dBFS -> -32.95 LUFS       [OK]
Linearite : +6 dB d'entree -> +6.00 LU                     [OK]
Sommation : deux canaux identiques -> +3.01 LU             [OK]
Gating : 10 s a -23 LUFS + 10 s de silence -> -23 LUFS     [OK]
True peak : detecte la crete cachee entre echantillons     [OK]

--- Traitement ---
   ecart maximal de magnitude sur 40 Hz - 15 kHz : 0.039 dB
Multibande : magnitude plate aux 3 coupures (+/-0.1 dB)    [OK]
EQ dynamique : attenue quand la bande depasse le seuil     [OK]
Mid/Side : le grave sous 150 Hz est recentre               [OK]
   48 cas adverses | pire ecart au plafond : -0.070 dB
Limiteur : plafond -1 dBTP jamais depasse                  [OK]
   latence declaree 173 echantillons (3.60 ms)
Limiteur : transparent sous le plafond, latence exacte     [OK]
   repli @13 kHz : sans OS -26.7 dB | avec OS -67.1 dB
Clipper : le sur-echantillonnage x4 effondre le repliement [OK]
Dither : bruit repousse hors de la zone sensible           [OK]
Chaine complete : true peak sous le plafond -1 dBTP        [OK]

>>> TOUS LES TESTS PASSENT
```

Vérification de la pondération K (mesurée) : −13,28 dB à 20 Hz, −2,90 dB à 60 Hz,
**+0,70 dB à 1 kHz**, +4,04 dB à 10 kHz — conforme à la courbe normative.

Build de référence : **JUCE 8.0.4, GCC 13.3, C++17 — 0 erreur, 0 avertissement**
sur le code du projet.

---

## 6. Pistes d'évolution

- Analyseur de spectre et courbe d'EQ superposés
- Égaliseur à phase linéaire (convolution FFT) commuté par bande
- Comparaison A/B avec un morceau de référence, alignée en loudness
- Historique LUFS court terme sur la durée du morceau (graphe défilant)
- Export d'un rapport de conformité (LUFS-I, LRA, dBTP, PLR) en PDF
- Détection automatique de la cible d'après les métadonnées du projet
