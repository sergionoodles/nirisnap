# Maintainer: Sergio <sergio@example.com>
pkgname=nirisnap
pkgver=0.1.0
pkgrel=1
pkgdesc='Native Wayland screenshot and annotation overlay for Niri'
arch=('x86_64')
url='https://github.com/sergionoodles/nirisnap'
license=('MIT')
depends=('qt6-base' 'layer-shell-qt' 'wayland' 'niri' 'wl-clipboard')
makedepends=('cmake' 'ninja' 'pkgconf' 'qt6-wayland' 'wayland-protocols' 'git')
optdepends=(
  'tesseract: OCR text recognition'
  'tesseract-data-eng: OCR English data'
  'libnotify: capture-finished notifications via notify-send'
)
# After tagging v$pkgver upstream, refresh with: updpkgsums && makepkg --printsrcinfo > .SRCINFO
source=("$pkgname-$pkgver.tar.gz::$url/archive/refs/tags/v$pkgver.tar.gz")
sha256sums=('956a6520fa4144fdb200e40103518aaa448b1951417105f1706af41c67f62c83')

build() {
  cmake -S "$pkgname-$pkgver" -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DBUILD_TESTING=OFF
  cmake --build build
}

check() {
  cmake -S "$pkgname-$pkgver" -B build-check -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DBUILD_TESTING=ON
  cmake --build build-check
  QT_QPA_PLATFORM=offscreen ./build-check/nirisnap-smoke ./build-check/nirisnap-smoke-output
}

package() {
  DESTDIR="$pkgdir" cmake --install build
}
