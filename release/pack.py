"""Pack the release zips (run after the builds): python release/pack.py [pc|switch|all|linux]   (default all)

  pc      release/ragdoll-button-windows-v<V>.zip   after: sh hookshot/build.sh   (out/hookshot/)
  switch  release/ragdoll-button-switch-v<V>.zip     after: sh build.sh            (out/ragdoll-button/exefs/subsdk7)
  linux   release/ragdoll-button-linux-v<V>.zip      after: sh hookshot/build.sh linux   (out/linux/; on Linux or WSL)

<V> is MOD_VERSION in src/config.inc. PC: extracted into the port folder, the mod lands in mods\\Ragdoll Button\\hsml\\
(1.0.0; before: ...\\hookshot\\, which HSML still reads: delete an old copy, or it loads twice). The HSML loader is its
own download. Both zips carry the licence notices, the presets in release/presets/ (if any) and release/README.txt.
1.1.1: no source in the zips; it is on GitHub (https://github.com/ClaudioSchmidt/oot-ragdoll-button, tag v<V>).
1.2.0: the Switch zip has no Fast Travel notice: the Switch build is a client of HSML's Mod Menu (hsml
switch/client/hsml_client.inc) and carries no menu, so none of switch/modmenu_nx.inc is in it; the menu needs HSML 2.1.
"""
import os, re, sys, zipfile
root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
hsml = os.path.abspath(os.path.join(root, '..', 'hsml'))
VERSION = re.search(r'MOD_VERSION = "([^"]+)"', open(os.path.join(root, 'src', 'config.inc'), encoding='utf-8').read()).group(1)

LICENCES = {
    'LICENSE': 'vendor/exlaunch/LICENSE',                 # GPL-2.0, this mod's licence (and exlaunch's)
    'LICENSE-HSML-MIT.txt': 'hsml:LICENSE',               # the SDK and the menu kit (PC); Switch: HSML's client, page SDK and kit
    'LICENSE-imgui-MIT.txt': 'vendor/imgui/LICENSE.txt',  # Dear ImGui, Omar Cornut (the menu is drawn with it)
}


def tree(base, rel, keep=lambda f: True):
    out = []
    for d, dirs, fs in os.walk(os.path.join(base, rel)):
        dirs[:] = sorted(x for x in dirs if x not in ('__pycache__', '.git'))
        out += [os.path.relpath(os.path.join(d, f), base).replace('\\', '/') for f in sorted(fs) if keep(f)]
    return out


def source():
    files = {}
    own = ['build.sh', 'README.md', 'FINDINGS.md'] + tree(root, 'src') + \
          tree(root, 'hookshot', lambda f: re.search(r'\.(cpp|hpp|inc|sh|in)$', f))   # not tools/tune.py: a dev tool with a path on my PC
    for f in own:
        files['source/oot-ragdoll/' + f] = f
    for f in ['hsml.h', 'LICENSE'] + tree(hsml, 'sdk') + tree(hsml, 'ui') + tree(hsml, 'switch'):
        files['source/hsml/' + f] = 'hsml:' + f
    return files


def presets(dest):
    d = os.path.join(root, 'release', 'presets')
    return {dest + f: 'release/presets/' + f for f in sorted(os.listdir(d)) if f.endswith('.txt')} if os.path.isdir(d) else {}


def pc():
    m = 'mods/Ragdoll Button/'
    files = {
        m + 'hsml/ragdoll-button.dll': 'out/hookshot/ragdoll-button.dll',
        m + 'mod.txt': 'out/hookshot/mod.txt',   # what HSML's mod list shows (written by hookshot/build.sh)
        'README.txt': 'release/README.txt',
    }
    files.update(LICENCES)
    for f in sorted(os.listdir(os.path.join(root, 'hookshot'))):   # the images of its info in HSML's menu (banner, banner2 ..)
        if re.fullmatch(r'banner\d?\.(png|jpg|jpeg)', f): files[m + f] = 'hookshot/' + f
    files.update(presets('presets/'))
    return 'ragdoll-button-windows-v%s.zip' % VERSION, files


def linux():
    """HSML 2.1, Linux (not released before a run on Linux): the same layout as pc() with the .so of
    sh hookshot/build.sh linux, and release/README-linux.txt as README.txt (README.txt stays the PC and Switch zips')."""
    m = 'mods/Ragdoll Button/'
    so = os.path.join(root, 'out', 'linux', 'ragdoll-button.so')
    if not os.path.exists(so) or os.path.getmtime(so) < os.path.getmtime(os.path.join(root, 'src', 'config.inc')):
        sys.exit('out/linux/ragdoll-button.so missing or older than src/config.inc: run sh hookshot/build.sh linux (on Linux or in WSL)')
    files = {
        m + 'hsml/ragdoll-button.so': 'out/linux/ragdoll-button.so',
        m + 'mod.txt': 'out/linux/mod.txt',
        'README.txt': 'release/README-linux.txt',
    }
    files.update(LICENCES)
    for f in sorted(os.listdir(os.path.join(root, 'hookshot'))):
        if re.fullmatch(r'banner\d?\.(png|jpg|jpeg)', f): files[m + f] = 'hookshot/' + f
    files.update(presets('presets/'))
    return 'ragdoll-button-linux-v%s.zip' % VERSION, files


def switch():
    plugin = os.path.join(root, 'out', 'ragdoll-button', 'exefs', 'subsdk7')
    if not os.path.exists(plugin) or os.path.getmtime(plugin) < os.path.getmtime(os.path.join(root, 'src', 'config.inc')):
        sys.exit('out/ragdoll-button/exefs/subsdk7 missing or older than src/config.inc: run sh build.sh')
    files = {
        'ragdoll-button/exefs/subsdk7': 'out/ragdoll-button/exefs/subsdk7',
        'README.txt': 'release/README.txt',
    }
    files.update(LICENCES)
    files.update(presets('OoT-Mods/presets/'))
    return 'ragdoll-button-switch-v%s.zip' % VERSION, files


def pack(name, files):
    out = os.path.join(root, 'release', name)
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
        for arc, src in files.items():
            z.write(os.path.join(hsml, src[5:]) if src.startswith('hsml:') else os.path.join(root, src), arc)
    print(out, os.path.getsize(out), 'bytes,', len(files), 'files')


if __name__ == '__main__':
    what = sys.argv[1] if len(sys.argv) > 1 else 'all'   # (all: pc and switch; linux only when named)
    for t in (['pc', 'switch'] if what == 'all' else [what]):
        pack(*{'pc': pc, 'switch': switch, 'linux': linux}[t]())
