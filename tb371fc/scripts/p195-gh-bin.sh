#!/bin/bash
# p195-gh-bin.sh — install gh CLI static binary into ~/bin (no sudo), via proxy
export http_proxy=http://172.20.208.1:7890 https_proxy=http://172.20.208.1:7890
TAG=$(curl -sIL -o /dev/null -w '%{url_effective}\n' https://github.com/cli/cli/releases/latest | grep -oE 'tag/v[0-9.]+' | cut -c6-)
VER=$(curl -s https://api.github.com/repos/cli/cli/releases/latest | grep -m1 '"tag_name"' | cut -d'"' -f4 | cut -c2-)
[ -z "$VER" ] && VER="$TAG"
echo "VER=$VER"
mkdir -p ~/bin
cd /tmp || exit 1
curl -sL -o gh.tgz "https://github.com/cli/cli/releases/download/v${VER}/gh_${VER}_linux_amd64.tar.gz" || exit 1
tar xzf gh.tgz
cp "gh_${VER}_linux_amd64/bin/gh" ~/bin/gh
~/bin/gh --version
echo DONE
