# Teeworlds InfCroya
InfClass with battle royale elements for Teeworlds 0.7+ based on [InfClass by necropotame](https://github.com/necropotame/teeworlds-infclass). InfCroya stands for "InfClass Royale".
## Additional dependencies
[GeoLite2++](https://www.ccoderun.ca/GeoLite2++/api/) is used for IP geolocation. This product includes GeoLite2 data created by MaxMind, available from
[https://maxmind.com](https://www.maxmind.com).
```bash
sudo apt install libmaxminddb-dev
```

## Building
Install [bam](https://github.com/matricks/bam) 0.5.1 build tool.
```
git clone https://github.com/yavl/teeworlds-infcroya
cd teeworlds-infcroya
```
Copy bam executable into teeworlds-infcroya directory.

### on Ubuntu / Mint / Debian
```bash
sudo apt install libmaxminddb-dev liblua5.3-dev
./bam server
```

### on macOS
via [Homebrew](https://brew.sh):
```bash
brew install libmaxminddb lua
./bam server
```

### on Windows
GCC should be installed, e.g. [Mingw-w64](https://mingw-w64.org).
```
bam config geolocation=false
bam server
```
To compile with VS:
```
bam config geolocation=false compiler="cl"
bam server
```