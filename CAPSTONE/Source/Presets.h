#pragma once
// ============================================================================
//  CAPSTONE — Presets d’usine
//  Points de départ, pas des solutions : en mastering un preset ne peut pas
//  connaître le mix. Chacun applique des gestes volontairement petits.
// ============================================================================
#include "Params.h"
#include <vector>

namespace mono {

struct PVal   { const char* id; float v; };
struct Preset { const char* category; const char* name; const char* note; std::vector<PVal> values; };

inline const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> P = {
    // ------------------------------------------------------------------------
    // PRESETS STREAMING — profils musicaux généraux
    // ------------------------------------------------------------------------
    { "STREAMING", "Rock — Spotify / Tidal",
      "Colle douce, largeur mesurée, plafond -1 dBTP. Vise -14 LUFS sans forcer.",
      { {pid::target,1.f},
        {pid::mbOn,1.f},{pid::mbX1,110.f},{pid::mbX2,750.f},{pid::mbX3,5000.f},
        {pid::mb1Thr,-16.f},{pid::mb1R,2.f},{pid::mb2Thr,-18.f},{pid::mb2R,1.6f},
        {pid::mb3Thr,-18.f},{pid::mb3R,1.6f},{pid::mb4Thr,-20.f},{pid::mb4R,1.8f},
        {pid::teLoF,75.f},{pid::teLoG,0.8f},{pid::teHiF,12000.f},{pid::teHiG,1.2f},
        {pid::msBassMono,110.f},{pid::msWidth,1.08f},{pid::msSidHiG,1.f},
        {pid::colOn,1.f},{pid::colMode,0.f},{pid::colDrive,3.f},{pid::colMix,0.3f},
        {pid::clOn,1.f},{pid::clDrive,1.5f},
        {pid::limGain,4.f},{pid::limCeil,-1.f},{pid::limRel,150.f} } },

    { "STREAMING", "Hard rock — Dense",
      "Multibande plus ferme sur le bas-médium, clipper plus présent, largeur contenue.",
      { {pid::target,1.f},
        {pid::mbOn,1.f},{pid::mbX1,100.f},{pid::mbX2,700.f},{pid::mbX3,4500.f},
        {pid::mb1Thr,-14.f},{pid::mb1R,2.5f},{pid::mb2Thr,-16.f},{pid::mb2R,2.2f},{pid::mb2G,-0.8f},
        {pid::mb3Thr,-17.f},{pid::mb3R,1.8f},{pid::mb4Thr,-19.f},{pid::mb4R,2.f},
        {pid::dq2On,1.f},{pid::dq2F,3200.f},{pid::dq2Q,1.4f},{pid::dq2Thr,-24.f},{pid::dq2Rng,3.f},
        {pid::teLoG,1.f},{pid::teHmF,2500.f},{pid::teHmG,0.6f},{pid::teHiG,1.f},
        {pid::msBassMono,120.f},{pid::msWidth,1.05f},
        {pid::colOn,1.f},{pid::colMode,1.f},{pid::colDrive,4.f},{pid::colMix,0.4f},
        {pid::clOn,1.f},{pid::clMode,1.f},{pid::clDrive,3.f},
        {pid::limGain,6.f},{pid::limCeil,-1.f},{pid::limRel,120.f} } },

    { "STREAMING", "Pop — Brillant et large",
      "Air et côtés ouverts, EQ dynamique qui tient les sifflantes du mix.",
      { {pid::target,1.f},
        {pid::mbOn,1.f},{pid::mb1Thr,-17.f},{pid::mb1R,1.8f},{pid::mb4Thr,-22.f},{pid::mb4R,2.f},
        {pid::dq2On,1.f},{pid::dq2F,7000.f},{pid::dq2Q,2.f},{pid::dq2Thr,-28.f},{pid::dq2Rng,3.5f},
        {pid::teLoF,70.f},{pid::teLoG,0.6f},{pid::teHiF,13000.f},{pid::teHiG,1.8f},
        {pid::msBassMono,100.f},{pid::msWidth,1.15f},{pid::msSidHiF,9000.f},{pid::msSidHiG,1.8f},
        {pid::colOn,1.f},{pid::colMode,2.f},{pid::colDrive,2.5f},{pid::colMix,0.25f},
        {pid::limGain,4.5f},{pid::limCeil,-1.f} } },

    { "STREAMING", "Rock atmosphérique — Dynamique préservée",
      "Peu de compression, LRA large. Pour un morceau qui vit de ses écarts de niveau.",
      { {pid::target,1.f},
        {pid::mbOn,1.f},{pid::mb1Thr,-20.f},{pid::mb1R,1.4f},{pid::mb2Thr,-22.f},{pid::mb2R,1.3f},
        {pid::mb3Thr,-22.f},{pid::mb3R,1.3f},{pid::mb4Thr,-24.f},{pid::mb4R,1.4f},
        {pid::mbAtt,40.f},{pid::mbRel,400.f},
        {pid::teLoG,0.5f},{pid::teHiF,14000.f},{pid::teHiG,1.5f},
        {pid::msBassMono,90.f},{pid::msWidth,1.2f},{pid::msSidHiG,1.5f},
        {pid::colOn,1.f},{pid::colDrive,2.f},{pid::colMix,0.2f},
        {pid::limGain,2.f},{pid::limCeil,-1.f},{pid::limRel,250.f},{pid::limLook,5.f} } },

    // ------------------------------------------------------------------------
    // PROFILS PAR PLATEFORME — cibles de livraison explicites
    // ------------------------------------------------------------------------
    { "PLATEFORMES", "Spotify — Streaming standard",
      "Cible Spotify : -14 LUFS intégrés et True Peak ≤ -1 dBTP. Évite la sur-compression.",
      { {pid::target,1.f},
        {pid::mbOn,1.f},{pid::mb1Thr,-18.f},{pid::mb1R,1.5f},{pid::mb2Thr,-20.f},{pid::mb2R,1.4f},
        {pid::mb3Thr,-20.f},{pid::mb3R,1.4f},{pid::mb4Thr,-22.f},{pid::mb4R,1.5f},
        {pid::msBassMono,100.f},{pid::msWidth,1.03f},
        {pid::colOn,1.f},{pid::colDrive,1.5f},{pid::colMix,0.15f},
        {pid::clOn,1.f},{pid::clDrive,1.0f},
        {pid::limGain,3.f},{pid::limCeil,-1.f},{pid::limRel,180.f},{pid::limLook,5.f} } },

    { "PLATEFORMES", "Apple Music — Sound Check",
      "Profil Apple Music stéréo : -16 LUFS intégrés, plafond -1 dBTP. Conserve davantage de dynamique.",
      { {pid::target,2.f},
        {pid::mbOn,1.f},{pid::mb1Thr,-20.f},{pid::mb1R,1.4f},{pid::mb2Thr,-22.f},{pid::mb2R,1.3f},
        {pid::mb3Thr,-22.f},{pid::mb3R,1.3f},{pid::mb4Thr,-24.f},{pid::mb4R,1.4f},
        {pid::msBassMono,100.f},{pid::msWidth,1.02f},
        {pid::colOn,1.f},{pid::colDrive,1.2f},{pid::colMix,0.12f},
        {pid::clOn,1.f},{pid::clDrive,0.8f},
        {pid::limGain,1.5f},{pid::limCeil,-1.f},{pid::limRel,220.f},{pid::limLook,6.f} } },

    { "PLATEFORMES", "YouTube Music — Streaming",
      "Profil YouTube : -14 LUFS comme point de départ et True Peak ≤ -1 dBTP. Vérifie aussi l’encodage final.",
      { {pid::target,3.f},
        {pid::mbOn,1.f},{pid::mb1Thr,-18.f},{pid::mb1R,1.6f},{pid::mb2Thr,-20.f},{pid::mb2R,1.5f},
        {pid::mb3Thr,-20.f},{pid::mb3R,1.5f},{pid::mb4Thr,-22.f},{pid::mb4R,1.6f},
        {pid::dq2On,1.f},{pid::dq2F,6500.f},{pid::dq2Q,1.8f},{pid::dq2Thr,-27.f},{pid::dq2Rng,2.5f},
        {pid::msBassMono,100.f},{pid::msWidth,1.04f},
        {pid::colOn,1.f},{pid::colDrive,1.8f},{pid::colMix,0.18f},
        {pid::limGain,3.f},{pid::limCeil,-1.f},{pid::limRel,170.f},{pid::limLook,5.f} } },

    { "PLATEFORMES", "Deezer — Streaming",
      "Profil Deezer : cible -15 LUFS et plafond -1 dBTP. Compression modérée pour préserver les transitoires.",
      { {pid::target,4.f},
        {pid::mbOn,1.f},{pid::mb1Thr,-19.f},{pid::mb1R,1.5f},{pid::mb2Thr,-21.f},{pid::mb2R,1.4f},
        {pid::mb3Thr,-21.f},{pid::mb3R,1.4f},{pid::mb4Thr,-23.f},{pid::mb4R,1.5f},
        {pid::msBassMono,100.f},{pid::msWidth,1.03f},
        {pid::colOn,1.f},{pid::colDrive,1.5f},{pid::colMix,0.15f},
        {pid::limGain,2.5f},{pid::limCeil,-1.f},{pid::limRel,190.f},{pid::limLook,5.f} } },

    { "PLATEFORMES", "Amazon Music — Streaming",
      "Profil Amazon : -14 LUFS et plafond -1 dBTP. Master propre pour limiter les artefacts de transcodage.",
      { {pid::target,1.f},
        {pid::mbOn,1.f},{pid::mb1Thr,-18.f},{pid::mb1R,1.5f},{pid::mb2Thr,-20.f},{pid::mb2R,1.4f},
        {pid::mb3Thr,-20.f},{pid::mb3R,1.4f},{pid::mb4Thr,-22.f},{pid::mb4R,1.5f},
        {pid::msBassMono,100.f},{pid::msWidth,1.03f},
        {pid::colOn,1.f},{pid::colDrive,1.4f},{pid::colMix,0.15f},
        {pid::limGain,3.f},{pid::limCeil,-1.f},{pid::limRel,180.f},{pid::limLook,5.f} } },

    { "PLATEFORMES", "SoundCloud — Streaming",
      "Profil SoundCloud : -14 LUFS intégrés et True Peak ≤ -1 dBTP pour limiter les artefacts des formats avec perte.",
      { {pid::target,1.f},
        {pid::mbOn,1.f},{pid::mb1Thr,-18.f},{pid::mb1R,1.5f},{pid::mb2Thr,-20.f},{pid::mb2R,1.4f},
        {pid::mb3Thr,-20.f},{pid::mb3R,1.4f},{pid::mb4Thr,-22.f},{pid::mb4R,1.5f},
        {pid::dq2On,1.f},{pid::dq2F,7000.f},{pid::dq2Q,1.8f},{pid::dq2Thr,-28.f},{pid::dq2Rng,2.5f},
        {pid::msBassMono,100.f},{pid::msWidth,1.03f},
        {pid::colOn,1.f},{pid::colDrive,1.2f},{pid::colMix,0.12f},
        {pid::limGain,3.f},{pid::limCeil,-1.f},{pid::limRel,180.f},{pid::limLook,5.f} } },

    { "PLATEFORMES", "TIDAL — Streaming / Lossless",
      "Profil TIDAL : livraison haute qualité, dynamique préservée. Le profil utilise la cible streaming -14 LUFS / -1 dBTP comme repère conservateur.",
      { {pid::target,1.f},
        {pid::mbOn,1.f},{pid::mb1Thr,-20.f},{pid::mb1R,1.4f},{pid::mb2Thr,-22.f},{pid::mb2R,1.3f},
        {pid::mb3Thr,-22.f},{pid::mb3R,1.3f},{pid::mb4Thr,-24.f},{pid::mb4R,1.4f},
        {pid::msBassMono,100.f},{pid::msWidth,1.04f},
        {pid::colOn,1.f},{pid::colDrive,1.0f},{pid::colMix,0.10f},
        {pid::limGain,2.f},{pid::limCeil,-1.f},{pid::limRel,220.f},{pid::limLook,6.f} } },

    // ------------------------------------------------------------------------
    // SUPPORTS PHYSIQUES / DIFFUSION / OUTILS
    // ------------------------------------------------------------------------
    { "PHYSIQUE", "CD — Niveau élevé",
      "Pas de normalisation en aval : le niveau du fichier est celui qu’on entendra.",
      { {pid::target,5.f},
        {pid::mbOn,1.f},{pid::mb1Thr,-13.f},{pid::mb1R,2.5f},{pid::mb2Thr,-15.f},{pid::mb2R,2.f},
        {pid::mb3Thr,-16.f},{pid::mb3R,2.f},{pid::mb4Thr,-18.f},{pid::mb4R,2.2f},
        {pid::colOn,1.f},{pid::colMode,1.f},{pid::colDrive,4.f},{pid::colMix,0.35f},
        {pid::clOn,1.f},{pid::clMode,1.f},{pid::clDrive,4.f},
        {pid::limGain,9.f},{pid::limCeil,-0.3f},{pid::limRel,100.f},
        {pid::ditMode,2.f},{pid::ditBits,0.f} } },

    { "PHYSIQUE", "Vinyle — Préparé pour la gravure",
      "Grave mono jusqu’à 200 Hz, sifflantes tenues, marge de crête large : le burin ne pardonne rien.",
      { {pid::target,7.f},
        {pid::rumbleOn,1.f},{pid::rumbleF,25.f},
        {pid::msBassMono,200.f},{pid::msWidth,0.95f},
        {pid::dq2On,1.f},{pid::dq2F,7500.f},{pid::dq2Q,2.5f},{pid::dq2Thr,-26.f},{pid::dq2Rng,5.f},
        {pid::mbOn,1.f},{pid::mb1Thr,-18.f},{pid::mb1R,2.f},{pid::mb4Thr,-22.f},{pid::mb4R,2.5f},
        {pid::teHiF,11000.f},{pid::teHiG,-0.5f},
        {pid::limGain,1.f},{pid::limCeil,-3.f},{pid::limRel,250.f},{pid::limLook,6.f} } },

    { "DIFFUSION", "Club / DJ",
      "Grave ferme et centre, niveau assumé. À vérifier impérativement sur grosse enceinte.",
      { {pid::target,6.f},
        {pid::mbOn,1.f},{pid::mbX1,90.f},{pid::mb1Thr,-11.f},{pid::mb1R,3.f},
        {pid::mb2Thr,-14.f},{pid::mb2R,2.2f},{pid::mb3Thr,-15.f},{pid::mb3R,2.f},{pid::mb4Thr,-17.f},{pid::mb4R,2.2f},
        {pid::msBassMono,140.f},{pid::msWidth,0.98f},
        {pid::colOn,1.f},{pid::colMode,1.f},{pid::colDrive,5.f},{pid::colMix,0.4f},
        {pid::clOn,1.f},{pid::clMode,2.f},{pid::clDrive,5.f},
        {pid::limGain,11.f},{pid::limCeil,-0.3f},{pid::limRel,80.f} } },

    { "DIFFUSION", "Broadcast EBU R128",
      "-23 LUFS, tolérance +/-0.5 LU. Aucune couleur ajoutée : on livre une mesure.",
      { {pid::target,8.f},
        {pid::mbOn,1.f},{pid::mb1Thr,-24.f},{pid::mb1R,1.5f},{pid::mb2Thr,-26.f},{pid::mb2R,1.4f},
        {pid::mb3Thr,-26.f},{pid::mb3R,1.4f},{pid::mb4Thr,-28.f},{pid::mb4R,1.5f},
        {pid::msWidth,1.f},{pid::msBassMono,60.f},
        {pid::limGain,0.f},{pid::limCeil,-1.f},{pid::limRel,300.f},{pid::limLook,6.f} } },

    { "OUTILS", "Transparent — Mesure seule",
      "Tout est neutre : le plugin ne sert qu’à mesurer LUFS, true peak, LRA et corrélation.",
      { {pid::target,0.f},{pid::mbOn,0.f},{pid::teOn,0.f},{pid::msOn,0.f},
        {pid::colOn,0.f},{pid::clOn,0.f},{pid::limOn,0.f} } },

    { "OUTILS", "Vérification mono / phase",
      "Largeur à zéro : tout passe en mono. Le test le plus rapide pour repérer une annulation.",
      { {pid::target,0.f},{pid::msOn,1.f},{pid::msWidth,0.f},
        {pid::mbOn,0.f},{pid::colOn,0.f},{pid::clOn,0.f},{pid::limOn,0.f} } },
    };
    return P;
}

} // namespace mono
