#pragma once
// ============================================================================
//  CAPSTONE — Paramètres
// ============================================================================
#include <juce_audio_processors/juce_audio_processors.h>

namespace pid {
constexpr auto inTrim   = "in_trim";    constexpr auto rumbleOn = "rumble_on";
constexpr auto rumbleF  = "rumble_f";
// EQ correctif (chirurgical, phase minimale)
constexpr auto cqOn     = "cq_on";
constexpr auto cq1F     = "cq1_f";      constexpr auto cq1G = "cq1_g";   constexpr auto cq1Q = "cq1_q";
constexpr auto cq2F     = "cq2_f";      constexpr auto cq2G = "cq2_g";   constexpr auto cq2Q = "cq2_q";
// EQ dynamique
constexpr auto dq1On    = "dq1_on";     constexpr auto dq1F = "dq1_f";   constexpr auto dq1Q = "dq1_q";
constexpr auto dq1Thr   = "dq1_thr";    constexpr auto dq1R = "dq1_r";   constexpr auto dq1Rng = "dq1_rng";
constexpr auto dq2On    = "dq2_on";     constexpr auto dq2F = "dq2_f";   constexpr auto dq2Q = "dq2_q";
constexpr auto dq2Thr   = "dq2_thr";    constexpr auto dq2R = "dq2_r";   constexpr auto dq2Rng = "dq2_rng";
// Multibande
constexpr auto mbOn     = "mb_on";      constexpr auto mbSolo = "mb_solo";
constexpr auto mbX1     = "mb_x1";      constexpr auto mbX2 = "mb_x2";   constexpr auto mbX3 = "mb_x3";
constexpr auto mbAtt    = "mb_att";     constexpr auto mbRel = "mb_rel";
constexpr auto mb1Thr="mb1_thr"; constexpr auto mb1R="mb1_r"; constexpr auto mb1G="mb1_g";
constexpr auto mb2Thr="mb2_thr"; constexpr auto mb2R="mb2_r"; constexpr auto mb2G="mb2_g";
constexpr auto mb3Thr="mb3_thr"; constexpr auto mb3R="mb3_r"; constexpr auto mb3G="mb3_g";
constexpr auto mb4Thr="mb4_thr"; constexpr auto mb4R="mb4_r"; constexpr auto mb4G="mb4_g";
// EQ tonal (large, musical)
constexpr auto teOn     = "te_on";
constexpr auto teLoF="te_lo_f"; constexpr auto teLoG="te_lo_g";
constexpr auto teLmF="te_lm_f"; constexpr auto teLmG="te_lm_g"; constexpr auto teLmQ="te_lm_q";
constexpr auto teHmF="te_hm_f"; constexpr auto teHmG="te_hm_g"; constexpr auto teHmQ="te_hm_q";
constexpr auto teHiF="te_hi_f"; constexpr auto teHiG="te_hi_g";
// Mid / Side
constexpr auto msOn     = "ms_on";      constexpr auto msBassMono = "ms_bassmono";
constexpr auto msWidth  = "ms_width";
constexpr auto msMidLoF="ms_mlo_f"; constexpr auto msMidLoG="ms_mlo_g";
constexpr auto msMidHiF="ms_mhi_f"; constexpr auto msMidHiG="ms_mhi_g";
constexpr auto msSidLoF="ms_slo_f"; constexpr auto msSidLoG="ms_slo_g";
constexpr auto msSidHiF="ms_shi_f"; constexpr auto msSidHiG="ms_shi_g";
// Couleur
constexpr auto colOn    = "col_on";     constexpr auto colMode  = "col_mode";
constexpr auto colDrive = "col_drive";  constexpr auto colMix   = "col_mix";
// Clipper
constexpr auto clOn     = "cl_on";      constexpr auto clMode   = "cl_mode";
constexpr auto clDrive  = "cl_drive";   constexpr auto clOs     = "cl_os";
// Limiteur
constexpr auto limOn    = "lim_on";     constexpr auto limGain  = "lim_gain";
constexpr auto limCeil  = "lim_ceil";   constexpr auto limRel   = "lim_rel";
constexpr auto limLook  = "lim_look";
// Sortie / dither
constexpr auto outTrim  = "out_trim";   constexpr auto ditMode  = "dit_mode";
constexpr auto ditBits  = "dit_bits";
// Global
constexpr auto target   = "target";     constexpr auto bypass   = "bypass";
} // namespace pid

namespace mono {

/** Cibles de diffusion : LUFS intégré et plafond true peak recommandés. */
struct DeliveryTarget { const char* name; float lufs; float dbtp; const char* note; };

inline const std::vector<DeliveryTarget>& deliveryTargets()
{
    static const std::vector<DeliveryTarget> T = {
        { "Libre (pas de cible)", -14.0f, -1.0f, "Aucune contrainte affichee." },
        { "Spotify / Tidal / Amazon", -14.0f, -1.0f, "Normalisation active : depasser -14 fait juste baisser le volume, en perdant la dynamique." },
        { "Apple Music (Sound Check)", -16.0f, -1.0f, "Cible la plus basse des grandes plateformes." },
        { "YouTube", -14.0f, -1.0f, "Normalise a -14 LUFS, encodage Opus/AAC." },
        { "Deezer", -15.0f, -1.0f, "Normalisation optionnelle cote utilisateur." },
        { "CD / distribution physique", -9.0f, -0.3f, "Pas de normalisation : le niveau est celui du fichier." },
        { "Club / DJ", -7.0f, -0.3f, "Niveau eleve assume, dynamique reduite." },
        { "Vinyle", -14.0f, -3.0f, "Grave en mono sous 150 Hz, sifflantes maitrisees, marge de crete large." },
        { "Broadcast EBU R128", -23.0f, -1.0f, "Norme televisuelle europeenne, tolerance +/-0.5 LU." },
        { "Cinema / OTT", -27.0f, -2.0f, "Dialogue-normalise, forte dynamique conservee." },
    };
    return T;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    using FP = AudioParameterFloat; using BP = AudioParameterBool; using CP = AudioParameterChoice;
    AudioProcessorValueTreeState::ParameterLayout L;

    auto freq = [] (float lo, float hi) { return NormalisableRange<float> (lo, hi, 0.1f, 0.25f); };
    auto lin  = [] (float lo, float hi, float st = 0.01f) { return NormalisableRange<float> (lo, hi, st, 1.0f); };
    auto skew = [] (float lo, float hi, float sk) { return NormalisableRange<float> (lo, hi, 0.01f, sk); };

    StringArray targetNames;
    for (const auto& t : deliveryTargets()) targetNames.add (t.name);

    L.add (std::make_unique<CP> (ParameterID{pid::target,1},  "Cible",  targetNames, 1));
    L.add (std::make_unique<BP> (ParameterID{pid::bypass,1},  "Bypass", false));

    L.add (std::make_unique<FP> (ParameterID{pid::inTrim,1},   "Entree",  lin (-12.f, 12.f, 0.1f), 0.f));
    L.add (std::make_unique<BP> (ParameterID{pid::rumbleOn,1}, "Rumble",  true));
    L.add (std::make_unique<FP> (ParameterID{pid::rumbleF,1},  "Rumble Hz", freq (10.f, 60.f), 20.f));

    // --- EQ correctif ---
    L.add (std::make_unique<BP> (ParameterID{pid::cqOn,1}, "EQ correctif", false));
    L.add (std::make_unique<FP> (ParameterID{pid::cq1F,1}, "C1 freq", freq (20.f, 20000.f), 250.f));
    L.add (std::make_unique<FP> (ParameterID{pid::cq1G,1}, "C1 gain", lin (-12.f, 12.f, 0.1f), 0.f));
    L.add (std::make_unique<FP> (ParameterID{pid::cq1Q,1}, "C1 Q",    skew (0.5f, 20.f, 0.35f), 4.f));
    L.add (std::make_unique<FP> (ParameterID{pid::cq2F,1}, "C2 freq", freq (20.f, 20000.f), 3000.f));
    L.add (std::make_unique<FP> (ParameterID{pid::cq2G,1}, "C2 gain", lin (-12.f, 12.f, 0.1f), 0.f));
    L.add (std::make_unique<FP> (ParameterID{pid::cq2Q,1}, "C2 Q",    skew (0.5f, 20.f, 0.35f), 4.f));

    // --- EQ dynamique ---
    auto addDyn = [&] (const char* on, const char* f, const char* q, const char* thr,
                       const char* r, const char* rng, const char* lbl, float defF)
    {
        L.add (std::make_unique<BP> (ParameterID{on,1},  juce::String (lbl) + " actif", false));
        L.add (std::make_unique<FP> (ParameterID{f,1},   juce::String (lbl) + " freq",  freq (30.f, 18000.f), defF));
        L.add (std::make_unique<FP> (ParameterID{q,1},   juce::String (lbl) + " Q",     skew (0.5f, 8.f, 0.5f), 1.5f));
        L.add (std::make_unique<FP> (ParameterID{thr,1}, juce::String (lbl) + " seuil", lin (-50.f, 0.f, 0.1f), -26.f));
        L.add (std::make_unique<FP> (ParameterID{r,1},   juce::String (lbl) + " ratio", skew (1.f, 10.f, 0.6f), 3.f));
        L.add (std::make_unique<FP> (ParameterID{rng,1}, juce::String (lbl) + " plage", lin (-9.f, 9.f, 0.1f), 4.f));
    };
    addDyn (pid::dq1On, pid::dq1F, pid::dq1Q, pid::dq1Thr, pid::dq1R, pid::dq1Rng, "Dyn 1", 200.f);
    addDyn (pid::dq2On, pid::dq2F, pid::dq2Q, pid::dq2Thr, pid::dq2R, pid::dq2Rng, "Dyn 2", 3500.f);

    // --- Multibande ---
    L.add (std::make_unique<BP> (ParameterID{pid::mbOn,1},   "Multibande", false));
    L.add (std::make_unique<CP> (ParameterID{pid::mbSolo,1}, "Solo", StringArray{"Off","Grave","Bas-med","Haut-med","Aigu"}, 0));
    L.add (std::make_unique<FP> (ParameterID{pid::mbX1,1},   "Coupure 1", freq (40.f, 400.f),    120.f));
    L.add (std::make_unique<FP> (ParameterID{pid::mbX2,1},   "Coupure 2", freq (300.f, 3000.f),  800.f));
    L.add (std::make_unique<FP> (ParameterID{pid::mbX3,1},   "Coupure 3", freq (2000.f, 12000.f),5000.f));
    L.add (std::make_unique<FP> (ParameterID{pid::mbAtt,1},  "MB attaque",skew (1.f, 200.f, 0.35f), 25.f));
    L.add (std::make_unique<FP> (ParameterID{pid::mbRel,1},  "MB relache",skew (30.f, 2000.f, 0.35f), 250.f));
    const char* mbT[] = { pid::mb1Thr, pid::mb2Thr, pid::mb3Thr, pid::mb4Thr };
    const char* mbR[] = { pid::mb1R,   pid::mb2R,   pid::mb3R,   pid::mb4R   };
    const char* mbG[] = { pid::mb1G,   pid::mb2G,   pid::mb3G,   pid::mb4G   };
    const char* mbN[] = { "Grave", "Bas-med", "Haut-med", "Aigu" };
    for (int b = 0; b < 4; ++b)
    {
        L.add (std::make_unique<FP> (ParameterID{mbT[b],1}, juce::String (mbN[b]) + " seuil", lin (-45.f, 0.f, 0.1f), -18.f));
        L.add (std::make_unique<FP> (ParameterID{mbR[b],1}, juce::String (mbN[b]) + " ratio", skew (1.f, 8.f, 0.6f), 1.8f));
        L.add (std::make_unique<FP> (ParameterID{mbG[b],1}, juce::String (mbN[b]) + " gain",  lin (-9.f, 9.f, 0.1f), 0.f));
    }

    // --- EQ tonal ---
    L.add (std::make_unique<BP> (ParameterID{pid::teOn,1},  "EQ tonal", true));
    L.add (std::make_unique<FP> (ParameterID{pid::teLoF,1}, "Assise Hz",  freq (30.f, 300.f), 80.f));
    L.add (std::make_unique<FP> (ParameterID{pid::teLoG,1}, "Assise",     lin (-6.f, 6.f, 0.1f), 0.f));
    L.add (std::make_unique<FP> (ParameterID{pid::teLmF,1}, "Bas-med Hz", freq (150.f, 1500.f), 400.f));
    L.add (std::make_unique<FP> (ParameterID{pid::teLmG,1}, "Bas-med",    lin (-6.f, 6.f, 0.1f), 0.f));
    L.add (std::make_unique<FP> (ParameterID{pid::teLmQ,1}, "Bas-med Q",  skew (0.3f, 3.f, 0.6f), 0.8f));
    L.add (std::make_unique<FP> (ParameterID{pid::teHmF,1}, "Presence Hz",freq (1000.f, 8000.f), 3000.f));
    L.add (std::make_unique<FP> (ParameterID{pid::teHmG,1}, "Presence",   lin (-6.f, 6.f, 0.1f), 0.f));
    L.add (std::make_unique<FP> (ParameterID{pid::teHmQ,1}, "Presence Q", skew (0.3f, 3.f, 0.6f), 0.8f));
    L.add (std::make_unique<FP> (ParameterID{pid::teHiF,1}, "Air Hz",     freq (5000.f, 20000.f), 12000.f));
    L.add (std::make_unique<FP> (ParameterID{pid::teHiG,1}, "Air",        lin (-6.f, 6.f, 0.1f), 0.f));

    // --- Mid / Side ---
    L.add (std::make_unique<BP> (ParameterID{pid::msOn,1},       "Mid/Side",  true));
    L.add (std::make_unique<FP> (ParameterID{pid::msBassMono,1}, "Bass mono", freq (20.f, 400.f), 110.f));
    L.add (std::make_unique<FP> (ParameterID{pid::msWidth,1},    "Largeur",   lin (0.f, 2.f, 0.001f), 1.f));
    L.add (std::make_unique<FP> (ParameterID{pid::msMidLoF,1}, "Mid grave Hz", freq (40.f, 500.f), 120.f));
    L.add (std::make_unique<FP> (ParameterID{pid::msMidLoG,1}, "Mid grave",    lin (-6.f, 6.f, 0.1f), 0.f));
    L.add (std::make_unique<FP> (ParameterID{pid::msMidHiF,1}, "Mid aigu Hz",  freq (2000.f, 18000.f), 8000.f));
    L.add (std::make_unique<FP> (ParameterID{pid::msMidHiG,1}, "Mid aigu",     lin (-6.f, 6.f, 0.1f), 0.f));
    L.add (std::make_unique<FP> (ParameterID{pid::msSidLoF,1}, "Side grave Hz",freq (40.f, 500.f), 120.f));
    L.add (std::make_unique<FP> (ParameterID{pid::msSidLoG,1}, "Side grave",   lin (-6.f, 6.f, 0.1f), 0.f));
    L.add (std::make_unique<FP> (ParameterID{pid::msSidHiF,1}, "Side aigu Hz", freq (2000.f, 18000.f), 9000.f));
    L.add (std::make_unique<FP> (ParameterID{pid::msSidHiG,1}, "Side aigu",    lin (-6.f, 6.f, 0.1f), 0.f));

    // --- Couleur / clipper / limiteur ---
    L.add (std::make_unique<BP> (ParameterID{pid::colOn,1},    "Couleur", false));
    L.add (std::make_unique<CP> (ParameterID{pid::colMode,1},  "Modele", StringArray{"BANDE","TRANSFO","LAMPE"}, 0));
    L.add (std::make_unique<FP> (ParameterID{pid::colDrive,1}, "Drive", lin (0.f, 12.f, 0.1f), 3.f));
    L.add (std::make_unique<FP> (ParameterID{pid::colMix,1},   "Dosage", lin (0.f, 1.f, 0.001f), 0.3f));

    L.add (std::make_unique<BP> (ParameterID{pid::clOn,1},    "Clipper", false));
    L.add (std::make_unique<CP> (ParameterID{pid::clMode,1},  "Courbe", StringArray{"DOUX","MOYEN","DUR"}, 0));
    L.add (std::make_unique<FP> (ParameterID{pid::clDrive,1}, "Clip drive", lin (0.f, 12.f, 0.1f), 0.f));
    L.add (std::make_unique<BP> (ParameterID{pid::clOs,1},    "OS x4", true));

    L.add (std::make_unique<BP> (ParameterID{pid::limOn,1},   "Limiteur", true));
    L.add (std::make_unique<FP> (ParameterID{pid::limGain,1}, "Gain", lin (0.f, 24.f, 0.1f), 0.f));
    L.add (std::make_unique<FP> (ParameterID{pid::limCeil,1}, "Plafond", lin (-6.f, 0.f, 0.1f), -1.f));
    L.add (std::make_unique<FP> (ParameterID{pid::limRel,1},  "Relache", skew (20.f, 1000.f, 0.4f), 150.f));
    L.add (std::make_unique<FP> (ParameterID{pid::limLook,1}, "Lookahead", lin (1.f, 10.f, 0.1f), 3.f));

    L.add (std::make_unique<FP> (ParameterID{pid::outTrim,1}, "Sortie", lin (-12.f, 12.f, 0.1f), 0.f));
    L.add (std::make_unique<CP> (ParameterID{pid::ditMode,1}, "Dither",
           StringArray{"Off","TPDF plat","Mise en forme legere","Mise en forme forte"}, 0));
    L.add (std::make_unique<CP> (ParameterID{pid::ditBits,1}, "Bits", StringArray{"16","20","24"}, 0));

    return L;
}

} // namespace mono
