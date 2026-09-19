import re, io

def strings(path, minlen=6):
    data = open(path, 'rb').read()
    return set(m.group().decode() for m in re.finditer(rb'[ -~]{%d,}' % minlen, data))

tw = strings(r'D:\work\code-work\project\tb371fc-kernel\reference\android_device_lenovo_TB371FC-TWRP\prebuilt\kernel')
st = strings(r'D:\work\code-work\project\tb371fc-kernel\fw\TB371FC_ZUI_16.0.474\image\boot.img')

# stock kernel is embedded in boot.img at 4096; redo for stock only
data = open(r'D:\work\code-work\project\tb371fc-kernel\fw\TB371FC_ZUI_16.0.474\image\boot.img', 'rb').read()
k = data[4096:4096 + 37445648]
st = set(m.group().decode() for m in re.finditer(rb'[ -~]{6,}', k))

only_tw = tw - st
only_st = st - tw
io.open(r'D:\work\code-work\project\tb371fc-kernel\out\strings-only-tw.txt', 'w', encoding='utf-8').write('\n'.join(sorted(only_tw)))
io.open(r'D:\work\code-work\project\tb371fc-kernel\out\strings-only-st.txt', 'w', encoding='utf-8').write('\n'.join(sorted(only_st)))
print('tw total:', len(tw), 'stock total:', len(st))
print('only_tw:', len(only_tw), 'only_stock:', len(only_st))

KEY = {
 'NAMESPACE': ('namespace', 'pid_ns', 'ipc_ns', 'user_ns', 'net_ns', 'uts_ns'),
 'OVERLAYFS': ('overlay', 'ovl_'),
 'VETH/BRIDGE': ('veth', 'bridge', 'br_'),
 'NETFILTER': ('iptable', 'ipt_', 'nf_nat', 'masquerade', 'conntrack', 'xt_'),
 'CGROUP/MEMCG': ('cgroup', 'memcg', 'mem_cgroup'),
 'BPF': ('bpf',),
 'TUN/TAP': ('tun_', 'tun0'),
 'SECCOMP': ('seccomp',),
 'DOCKER-ish': ('docker', 'containerd', 'overlay2'),
}
for cat, kws in KEY.items():
    tw_hits = sorted(s for s in only_tw if any(k in s.lower() for k in kws))
    st_hits = sorted(s for s in only_st if any(k in s.lower() for k in kws))
    print('== %s: tw-only=%d stock-only=%d' % (cat, len(tw_hits), len(st_hits)))
    for s in tw_hits[:6]: print('   TW :', s[:100])
    for s in st_hits[:6]: print('   ST :', s[:100])
