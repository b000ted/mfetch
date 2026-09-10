# mfetch
A fork of a fork. mfetch is a macOS / Darwin port of [gfetch by kantiankant](https://github.com/kantiankant/gfetch)

# preview
```
        .:'          b000ted@macOS
    __ :'__          -----------
 .'`  `-'  ``.       os: macOS 15.8
:          .-'       kernel: 24.6.0
:         :          shell: zsh
 :         `-;       uptime: 23h 4m
  `.__.-.__.'        cpu: Apple M1
                     ram: 5.71 / 8.00 GiB
                     disk: 201.9 / 228.3 GiB
                     gpu: Apple M1 (integrated)
```

## credits

- **Original concept**: [Glenda Fetch by arwn](https://github.com/arwn)
- **C re-implementation**: [gfetch by kantiankant](https://github.com/kantiankant/gfetch)
  
## build

requirements: 
a C11 compiler and Xcode Command Line Tools

```
git clone https://github.com/b000ted/mfetch
cd mfetch
cc -Wall -Wextra -O2 -std=c11 -o mfetch src/gfetch.c
```

## Install

```
sudo cp mfetch /usr/local/bin/mfetch
```

Run:

```
mfetch
```

## license
GPLv3, inherited from the upstream `gfetch` project. See [LICENSE](LICENSE)
for the full text. Per section 5 of the license: this is a modified version
of `gfetch`, modified to add macOS/Darwin support (2026).
