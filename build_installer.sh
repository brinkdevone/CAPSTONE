#!/usr/bin/env bash
# ============================================================================
#  CAPSTONE — Fabrication de l'installeur macOS (.pkg)
#
#  À exécuter SUR UN MAC (pkgbuild / productbuild sont des outils Apple).
#  Produit : dist/CAPSTONE-<version>.pkg  — universel arm64 + x86_64
#
#  Signature (optionnelle) via variables d'environnement :
#     APP_CERT   = "Developer ID Application: Ton Nom (TEAMID)"
#     INST_CERT  = "Developer ID Installer: Ton Nom (TEAMID)"
#     NOTARY_PROFILE = nom d'un profil `xcrun notarytool store-credentials`
#  Sans ces variables, l'installeur est produit non signé (voir README §Gatekeeper).
# ============================================================================
set -euo pipefail

cd "$(dirname "$0")/.."
ROOT="$PWD"
VERSION="${VERSION:-1.0.0}"
IDENT_BASE="com.ateliercapstone.capstone"
BUILD_DIR="$ROOT/build"
ART="$BUILD_DIR/CAPSTONE_artefacts/Release"
STAGE="$ROOT/build/pkg-stage"
DIST="$ROOT/dist"

if [[ "$(uname)" != "Darwin" ]]; then
  echo "ERREUR : ce script requiert macOS (pkgbuild/productbuild sont des outils Apple)." >&2
  exit 1
fi

echo "==> 1/6  Compilation universelle (arm64 + x86_64)"
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13
cmake --build "$BUILD_DIR" --config Release -j"$(sysctl -n hw.ncpu)"

echo "==> 2/6  Banc de test DSP"
"$BUILD_DIR/capstone_dsp_test"

echo "==> 3/6  Validation Audio Unit (auval)"
# Le plugin doit d'abord être visible du système pour qu'auval le trouve.
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"
rsync -a --delete "$ART/AU/CAPSTONE.component" "$HOME/Library/Audio/Plug-Ins/Components/"
killall -9 AudioComponentRegistrar 2>/dev/null || true
sleep 2
if auval -v aufx Cpst Atmn; then
  echo "    auval : PASS"
else
  echo "    auval : ECHEC — Logic Pro refusera le plugin. Build interrompu." >&2
  exit 1
fi

echo "==> 4/6  Signature des bundles"
if [[ -n "${APP_CERT:-}" ]]; then
  for b in "$ART/AU/CAPSTONE.component" "$ART/VST3/CAPSTONE.vst3" "$ART/Standalone/CAPSTONE.app"; do
    [[ -e "$b" ]] || continue
    codesign --force --deep --options runtime --timestamp \
             --sign "$APP_CERT" "$b"
    codesign --verify --strict --verbose=2 "$b"
  done
  echo "    bundles signés"
else
  echo "    APP_CERT absent — bundles non signés (installation locale uniquement)"
fi

echo "==> 5/6  Construction des composants .pkg"
rm -rf "$STAGE" "$DIST"; mkdir -p "$STAGE" "$DIST"

# Un pkgbuild par format : l'utilisateur pourra décocher ce qu'il ne veut pas.
build_component () {           # $1 bundle  $2 install-location  $3 suffixe id  $4 sortie
  local src="$1" loc="$2" idsuffix="$3" out="$4"
  [[ -e "$src" ]] || { echo "    (absent, ignoré) $src"; return; }
  local root="$STAGE/root-$idsuffix"
  mkdir -p "$root"
  rsync -a "$src" "$root/"
  pkgbuild --root "$root" \
           --install-location "$loc" \
           --identifier "$IDENT_BASE.$idsuffix" \
           --version "$VERSION" \
           --scripts "$ROOT/packaging/scripts" \
           "$STAGE/CAPSTONE-$idsuffix.pkg"
  echo "    OK  $out"
}

build_component "$ART/AU/CAPSTONE.component"  "/Library/Audio/Plug-Ins/Components" au   "Audio Unit"
build_component "$ART/VST3/CAPSTONE.vst3"     "/Library/Audio/Plug-Ins/VST3"       vst3 "VST3"
build_component "$ART/Standalone/CAPSTONE.app" "/Applications"                     app  "Application"

echo "==> 6/6  Assemblage de l'installeur final"
PKG="$DIST/CAPSTONE-$VERSION.pkg"
productbuild --distribution "$ROOT/packaging/distribution.xml" \
             --package-path "$STAGE" \
             --resources    "$ROOT/packaging/resources" \
             "$DIST/CAPSTONE-unsigned.pkg"

if [[ -n "${INST_CERT:-}" ]]; then
  productsign --sign "$INST_CERT" "$DIST/CAPSTONE-unsigned.pkg" "$PKG"
  rm "$DIST/CAPSTONE-unsigned.pkg"
  pkgutil --check-signature "$PKG"
  if [[ -n "${NOTARY_PROFILE:-}" ]]; then
    echo "    Notarisation Apple…"
    xcrun notarytool submit "$PKG" --keychain-profile "$NOTARY_PROFILE" --wait
    xcrun stapler staple "$PKG"
    echo "    notarisé et agrafé"
  fi
else
  mv "$DIST/CAPSTONE-unsigned.pkg" "$PKG"
  echo "    INST_CERT absent — installeur non signé"
fi

echo
echo "============================================================"
echo " Installeur prêt : $PKG"
du -h "$PKG" | cut -f1 | sed 's/^/ Taille : /'
echo "============================================================"
