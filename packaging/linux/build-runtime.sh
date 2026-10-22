#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Experimental clean-from-source Linux builder for Valkyria Revolution PS4RT.
# Creates an isolated ELF/lib bundle for tools/package_valkyria.py.
# Does not alter the checked-out source, previous smoke tests, or v1.0.
set -euo pipefail

fail() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
cd "$ROOT"
[[ -f build.sh && -f tools/package_valkyria.py && -f gpu/CMakeLists.txt ]] || fail 'This script must live under packaging/linux/ in the Valkyria Revolution source checkout.'
for cmd in git podman tar patchelf readelf ldd python3; do
  command -v "$cmd" >/dev/null || fail "Missing required command: $cmd"
done

IMAGE='localhost/ps4rt-builder:linux'
podman image exists "$IMAGE" || fail "Linux build image not found. First run: bash packaging/linux/build-images.sh"

# A unique working directory prevents accidental overwrite of the known-good test builds.
DEFAULT_PARENT="$HOME/Documents/PS4RT-Valkyria-Ubuntu-Build"
PARENT="${PS4RT_LINUX_WORK_PARENT:-$DEFAULT_PARENT}"
mkdir -p -- "$PARENT"
WORK="$(mktemp -d "$PARENT/valkyria-release-build.XXXXXXXX")"
SOURCE="$WORK/source"
STAGE="$WORK/bundle"
mkdir -p "$SOURCE" "$STAGE/bin/gpu" "$STAGE/lib"
REVISION="$(git rev-parse HEAD)"
printf '%s\n' "$REVISION" > "$WORK/source-revision.txt"

# Building a git archive ensures generated linked game executables, saves, and
# shader caches never enter the container or its output. Source is HEAD only.
git archive --format=tar HEAD | tar -xf - -C "$SOURCE"
[[ -f "$SOURCE/build.sh" && -f "$SOURCE/gpu/CMakeLists.txt" ]] || fail 'Committed source archive is incomplete.'

echo "=== VALKYRIA REVOLUTION LINUX CLEAN BUILD ==="
echo "Source revision: $REVISION"
echo "Builder: $IMAGE"
echo "Workspace: $WORK"
echo "Log: $WORK/build.log"
echo "The checkout and previous working bundles remain unchanged."

# Compiled from the committed source, using PS4RT_PORTABLE_BUILD for the
# Ubuntu toolchain. Game code and the user's original checkout are unchanged.

# Feed the build script on stdin;# Feed the build script on stdin; -i is required for Podman to accept heredoc
# input (the previous staging attempt failed without it).
if ! podman run --rm -i --security-opt label=disable \
     --env PS4RT_BUILD_JOBS="${PS4RT_BUILD_JOBS:-4}" \
     --mount "type=bind,source=$SOURCE,target=/work/source" \
     --mount "type=bind,source=$STAGE,target=/stage" \
     "$IMAGE" bash -s >"$WORK/build.log" 2>&1 <<'CONTAINER'; then
set -euo pipefail
cd /work/source
export CXXFLAGS="${CXXFLAGS:+$CXXFLAGS }-idirafter /usr/include"
# Needed for libvulkan.so and nested dependencies in the older Ubuntu sysroot.
export LDFLAGS="${LDFLAGS:+$LDFLAGS }-L/usr/lib/x86_64-linux-gnu -Wl,-rpath-link,/opt/ps4rt-deps/lib"
export PS4RT_LTO=OFF PS4RT_PGO=off PS4RT_PORTABLE_BUILD=1
command -v "$CC" >/dev/null
command -v "$CXX" >/dev/null

echo '=== COMPILE MAIN + GPU ==='
bash build.sh
[[ -f out/ps4rt && -f out/gpu/libps4rtgpu.so && -f out/ps4rt-gpu-capabilities ]]
install -Dm755 out/ps4rt /stage/bin/ps4rt
install -Dm755 out/gpu/libps4rtgpu.so /stage/bin/gpu/libps4rtgpu.so
install -Dm755 out/ps4rt-gpu-capabilities /stage/bin/ps4rt-gpu-capabilities

# Same allowlist as tools/package_valkyria.py. Collect via the builder's own
# linker resolution, never via random host-installed Fedora libraries.
CXX_LIB="$($CXX -print-file-name=libstdc++.so.6)"
GCC_LIB="$($CXX -print-file-name=libgcc_s.so.1)"
export CXX_LIB GCC_LIB
[[ -f "$CXX_LIB" && -f "$GCC_LIB" ]]
export LD_LIBRARY_PATH="$(dirname "$CXX_LIB"):/opt/ps4rt-deps/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

python3 - <<'PY'
import os, re, shutil, subprocess
from pathlib import Path
bundle = Path('/stage')
required = {
    'libavcodec.so.62','libavformat.so.62','libavutil.so.60',
    'libfmt.so.11','libgcc_s.so.1','libSDL3.so.0','libstdc++.so.6',
    'libswresample.so.6','libswscale.so.9',
    'libxxhash.so.0','libz.so.1','libZydis.so.5.0',
}
provided = {'libstdc++.so.6':os.environ['CXX_LIB'],
            'libgcc_s.so.1':os.environ['GCC_LIB']}
for binary in (bundle/'bin/ps4rt',bundle/'bin/gpu/libps4rtgpu.so'):
    result = subprocess.run(['ldd',str(binary)],capture_output=True,text=True)
    if result.returncode or 'not found' in result.stdout:
        raise SystemExit('Unable to resolve in builder: '+str(binary)+'\n'+result.stdout+result.stderr)
    for line in result.stdout.splitlines():
        match = re.match(r'\s*(\S+)\s+=>\s+(/\S+)\s+\(', line)
        if match:
            name,path = match.groups()
            if name in required and name not in provided:
                provided[name] = path
missing=required-set(provided)
if missing:
    raise SystemExit('Missing release libraries in builder: '+', '.join(sorted(missing)))
for name in sorted(required):
    path=Path(provided[name]).resolve(strict=True)
    dst=bundle/'lib'/name
    shutil.copy2(path,dst)
    if not dst.is_file() or dst.stat().st_size==0:
        raise SystemExit('Empty library: '+str(dst))
    print(name,'<-',path)
PY
CONTAINER
  tail -n 40 "$WORK/build.log" >&2
  fail "Container source build failed. Read $WORK/build.log; nothing was published or overwritten."
fi

# Complete the isolated test bundle with launcher and checked-in assets.
# Game-derived files will be generated ONLY inside $STAGE/out/<CUSAxxxxx> if
# the developer explicitly launches this stage for a smoke test.
cp -a -- "$SOURCE/scripts" "$STAGE/"
cp -a -- "$SOURCE/packaging/linux/Play-Valkyria.sh" "$STAGE/"
cp -a -- "$SOURCE/LICENSE" "$STAGE/"
cp -a -- "$SOURCE/fsr4_shaders" "$STAGE/"
patchelf --set-rpath '$ORIGIN/../lib:$ORIGIN/gpu' "$STAGE/bin/ps4rt"
patchelf --set-rpath '$ORIGIN/../../lib' "$STAGE/bin/gpu/libps4rtgpu.so"
patchelf --set-rpath '$ORIGIN/../lib' "$STAGE/bin/ps4rt-gpu-capabilities"
while IFS= read -r -d '' so; do
  patchelf --set-rpath '$ORIGIN' "$so"
done < <(find "$STAGE/lib" -maxdepth 1 -type f -name '*.so*' -print0)

# Verify the exact packaged-binary rules without requiring a Git commit.
# A live ldd check still uses the host Vulkan loader and glibc as intended.
STAGE="$STAGE" python3 - <<'PY' | tee "$WORK/elf-audit.txt"
import os,re,subprocess
from pathlib import Path
p=Path(os.environ['STAGE'])
needed=['bin/ps4rt','bin/gpu/libps4rtgpu.so','bin/ps4rt-gpu-capabilities']
needed += ['lib/'+x for x in (
 'libavcodec.so.62','libavformat.so.62','libavutil.so.60',
 'libfmt.so.11','libgcc_s.so.1','libSDL3.so.0','libstdc++.so.6',
 'libswresample.so.6','libswscale.so.9',
 'libxxhash.so.0','libz.so.1','libZydis.so.5.0')]
expected={'bin/ps4rt':'$ORIGIN/../lib:$ORIGIN/gpu',
          'bin/gpu/libps4rtgpu.so':'$ORIGIN/../../lib',
          'bin/ps4rt-gpu-capabilities':'$ORIGIN/../lib'}
for name in needed:
    elf=p/name
    if not elf.is_file():raise SystemExit('Missing ELF '+name)
    rpath=subprocess.check_output(['patchelf','--print-rpath',str(elf)],text=True).strip()
    if rpath!=expected.get(name,'$ORIGIN'):
        raise SystemExit(f'Bad RUNPATH: {name}: {rpath}')
    output=subprocess.check_output(['readelf','-W','--version-info',str(elf)],text=True)
    nums=[tuple(map(int,n.split('.'))) for n in re.findall(r'GLIBC_([0-9]+(?:\.[0-9]+)+)',output)]
    maximum=max(nums) if nums else None
    if maximum and maximum>(2,35):
        raise SystemExit(f'glibc too new: {name}: {maximum}')
    print(name,'maximum glibc:', '.'.join(map(str,maximum)) if maximum else 'none', 'RUNPATH:',rpath)
env=os.environ.copy()
env.pop('LD_LIBRARY_PATH',None); env.pop('LD_PRELOAD',None)
for name in ('bin/ps4rt','bin/gpu/libps4rtgpu.so','bin/ps4rt-gpu-capabilities'):
    r=subprocess.run(['ldd',str(p/name)],capture_output=True,text=True,env=env)
    if r.returncode or 'not found' in (r.stdout+r.stderr):
        raise SystemExit('Host dependency resolution failed for '+name+'\n'+r.stdout+r.stderr)
    for line in r.stdout.splitlines():
        m=re.match(r'\s*(\S+)\s+=>\s+(/\S+)\s+\(',line)
        if m and (p/'lib'/m.group(1)).exists():
            if Path(m.group(2)).resolve()!=(p/'lib'/m.group(1)).resolve():
                raise SystemExit('Bundled dependency escaped bundle: '+line)
    print('HOST RESOLUTION PASS:',name)
print('ELF BASELINE + RELATIVE PATHS: PASS')
PY

echo
echo '=== CLEAN SOURCE BUILD READY FOR GAMEPLAY CHECK ==='
echo "BUNDLE=$STAGE"
echo "BUILD_LOG=$WORK/build.log"
echo "ELF_AUDIT=$WORK/elf-audit.txt"
echo 'Once verified, this bundle can be passed to tools/package_valkyria.py --runtime --bundle-from.'
