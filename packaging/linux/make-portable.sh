#!/usr/bin/env bash
# Creates a self contained portable build of Prixum Launcher from a configured and built CMake tree.
#
#   packaging/linux/make-portable.sh [build dir] [output .tar.gz]
#
# Qt and the libraries it needs are copied next to the launcher, basic system libraries (glibc, OpenGL, X11,
# Wayland, fonts, D-Bus, audio) come from the system. The result runs on distributions that are at least as new
# as the one it was built on. All data (instances, accounts, settings) is stored inside the extracted folder.

set -euo pipefail

src_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$(realpath "${1:-${src_root}/build-release}")"
output="$(realpath -m "${2:-${src_root}/dist/PrixumLauncher-Linux-$(uname -m)-Portable.tar.gz}")"
mkdir -p "$(dirname "${output}")"

binary_name="prixumlauncher"
work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT
stage="${work}/PrixumLauncher"

echo ":: Installing into ${stage}"
cmake --install "${build_dir}" --prefix "${stage}" > /dev/null
# portable.txt, the data lands next to the launcher
cmake --install "${build_dir}" --prefix "${stage}" --component portable > /dev/null
# the generic start script of the portable component, ours below also sets up the bundled libraries
rm -f "${stage}/PrixumLauncher"

qt_plugins="$(qtpaths6 --query QT_INSTALL_PLUGINS 2> /dev/null || qmake6 -query QT_INSTALL_PLUGINS)"
echo ":: Copying Qt plugins from ${qt_plugins}"
mkdir -p "${stage}/plugins" "${stage}/lib"
# only the plugins of Qt itself that the launcher needs, desktop specific ones (KDE image formats, layer shell,
# virtual keyboard, ...) would drag half of their desktop along
plugins=(
    platforms/libqxcb.so platforms/libqwayland.so platforms/libqoffscreen.so
    platforminputcontexts/libcomposeplatforminputcontextplugin.so platforminputcontexts/libibusplatforminputcontextplugin.so
    platformthemes/libqxdgdesktopportal.so
    imageformats/libqgif.so imageformats/libqico.so imageformats/libqjpeg.so imageformats/libqsvg.so imageformats/libqwebp.so
    iconengines/libqsvgicon.so
    tls/libqopensslbackend.so tls/libqcertonlybackend.so
    networkinformation/libqnetworkmanager.so
    xcbglintegrations/libqxcb-egl-integration.so xcbglintegrations/libqxcb-glx-integration.so
    wayland-shell-integration/libxdg-shell.so
    wayland-decoration-client/libbradient.so
    wayland-graphics-integration-client/libqt-plugin-wayland-egl.so wayland-graphics-integration-client/libdmabuf-server.so
    wayland-graphics-integration-client/libshm-emulation-server.so
)
for plugin in "${plugins[@]}"; do
    if [[ -f "${qt_plugins}/${plugin}" ]]; then
        mkdir -p "${stage}/plugins/$(dirname "${plugin}")"
        cp "${qt_plugins}/${plugin}" "${stage}/plugins/${plugin}"
    else
        echo "   missing plugin ${plugin}, skipped"
    fi
done

cat > "${stage}/bin/qt.conf" << 'EOF'
[Paths]
Prefix = ..
Plugins = plugins
EOF

# libraries every desktop has, bundling those breaks graphics drivers, input methods or fonts
exclude='^(ld-linux.*|libc|libm|libmvec|libdl|libpthread|librt|libresolv|libutil|libanl|libnsl|libBrokenLocale|libthread_db|libnss_.*|libGL|libGLX|libEGL|libGLdispatch|libOpenGL|libGLESv2|libgbm|libdrm|libdrm_.*|libvulkan|libX11|libX11-xcb|libXau|libXdmcp|libXext|libXrender|libXi|libXfixes|libXcursor|libXrandr|libxcb|libxcb-(dri2|dri3|glx|present|randr|render|shape|shm|sync|xfixes|xinput|xkb)|libxshmfence|libwayland-(client|server|egl|cursor)|libfontconfig|libfreetype|libexpat|libdbus-1|libsystemd|libudev|libcap|libasound|libpulse|libz|libuuid|libblkid|libmount|libselinux|libgcc_s|libstdc\+\+)\.so'

echo ":: Collecting shared libraries"
mapfile -t objects < <(find "${stage}/bin" "${stage}/plugins" -type f \( -name "${binary_name}" -o -name '*.so' \))
declare -A copied=()
for object in "${objects[@]}"; do
    while read -r name path; do
        [[ -z "${path}" || ! -e "${path}" ]] && continue
        # the dynamic loader is listed with its full path
        name="$(basename "${name}")"
        [[ "${name}" =~ ${exclude} ]] && continue
        [[ -n "${copied[${name}]:-}" ]] && continue
        cp -L "${path}" "${stage}/lib/${name}"
        copied[${name}]=1
    done < <(ldd "${object}" | awk '/=>/ { print $1, $3 }')
done
echo "   ${#copied[@]} libraries bundled"

cat > "${stage}/PrixumLauncher" << 'EOF'
#!/usr/bin/env bash
# Starts the portable Prixum Launcher with the libraries shipped next to it.
if [[ $EUID -eq 0 ]]; then
    echo "Prixum Launcher should not be run as root." >&2
    exit 1
fi
dir="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="${dir}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
# the launcher removes these entries again before it starts Minecraft
export LAUNCHER_LD_LIBRARY_PATH="${dir}/lib"
export QT_QPA_PLATFORMTHEME="${QT_QPA_PLATFORMTHEME:-xdgdesktopportal}"
exec "${dir}/bin/prixumlauncher" "$@"
EOF
chmod +x "${stage}/PrixumLauncher" "${stage}/bin/${binary_name}"

# desktop integration files belong to installed builds
rm -rf "${stage}/share/applications" "${stage}/share/metainfo" "${stage}/share/mime" "${stage}/share/man"
cp "${src_root}/program_info/org.prixumlauncher.PrixumLauncher.svg" "${stage}/icon.svg"

echo ":: Packing ${output}"
tar -C "${work}" -czf "${output}" PrixumLauncher
du -h "${output}" | cut -f1
