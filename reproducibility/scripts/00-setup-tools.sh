#!/usr/bin/env bash
# Checks the prerequisites. If no usable LaTeX installation is found, a
# minimal TeX Live is installed below reproducibility/tools (no root access
# needed, nothing is written outside of that directory).

source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

missing=()
for tool in git cmake make "$LCE_CC" "$LCE_CXX" python3 wget gunzip perl; do
  command -v "$tool" >/dev/null || missing+=("$tool")
done
if ((${#missing[@]})); then
  die "missing programs: ${missing[*]}
On Ubuntu 24.04 the prerequisites are installed with
  sudo apt install build-essential gcc-12 g++-12 cmake git wget perl python3 \\
                   libtbb-dev libomp-dev libsdsl-dev libdivsufsort-dev"
fi
python3 -c 'import sqlite3' || die "python3 lacks the sqlite3 module"

LATEX_PACKAGES=(pgfplots.sty tikz.sty siunitx.sty booktabs.sty multirow.sty
                subcaption.sty xcolor.sty geometry.sty amssymb.sty)

latex_is_usable() {
  command -v pdflatex >/dev/null && command -v kpsewhich >/dev/null || return 1
  local package
  for package in "${LATEX_PACKAGES[@]}"; do
    kpsewhich "$package" >/dev/null || return 1
  done
}

install_texlive() {
  local dir="$LCE_TOOLS_DIR/texlive" tmp
  mkdir -p "$LCE_TOOLS_DIR" "$TMP_DIR"
  tmp="$(mktemp -d "$TMP_DIR/install-tl.XXXXXX")"
  log "installing a minimal TeX Live into $dir"
  wget -q -O "$tmp/install-tl.tar.gz" \
      https://mirror.ctan.org/systems/texlive/tlnet/install-tl-unx.tar.gz
  tar -xzf "$tmp/install-tl.tar.gz" -C "$tmp" --strip-components=1
  cat > "$tmp/texlive.profile" <<EOF
selected_scheme scheme-basic
TEXDIR $dir
TEXMFLOCAL $dir/texmf-local
TEXMFSYSCONFIG $dir/texmf-config
TEXMFSYSVAR $dir/texmf-var
TEXMFHOME $dir/texmf-home
TEXMFCONFIG $dir/texmf-config
TEXMFVAR $dir/texmf-var
instopt_portable 1
instopt_adjustpath 0
tlpdbopt_autobackup 0
tlpdbopt_install_docfiles 0
tlpdbopt_install_srcfiles 0
EOF
  perl "$tmp/install-tl" --no-interaction --profile "$tmp/texlive.profile" \
      > "$LCE_TOOLS_DIR/install-tl.log" 2>&1 \
      || die "TeX Live installation failed, see $LCE_TOOLS_DIR/install-tl.log"
  PATH="$dir/bin/x86_64-linux:$PATH"
  tlmgr install pgf pgfplots xcolor siunitx booktabs multirow caption \
      >> "$LCE_TOOLS_DIR/install-tl.log" 2>&1 \
      || die "tlmgr failed, see $LCE_TOOLS_DIR/install-tl.log"
  rm -rf "$tmp"
}

if latex_is_usable; then
  log "LaTeX: $(command -v pdflatex)"
else
  install_texlive
  latex_is_usable || die "the LaTeX installation lacks required packages"
  log "LaTeX: $(command -v pdflatex)"
fi
log "compilers: $("$LCE_CC" --version | head -n1) / $("$LCE_CXX" --version | head -n1)"
log "all prerequisites are available"
