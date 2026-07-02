# NetNemesis

NetNemesis is a Linux toolkit for analyzing and attacking network services. The project combines packet crafting, flooding, network scanning, fingerprinting, and a simple botnet system in a single command-line interface.

> Warning: using this software on networks or devices without authorization may be illegal. Use NetNemesis only in controlled environments and with explicit permission.

## Requirements

- Linux
- Privilegi `root`
- CMake 3.10+
- Compiler C++ compatibile C++17
- OpenSSL development libraries

## Build

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

## Usage

Run the command from a terminal with root privileges:

```bash
sudo ./netnemesis
```

## Main commands

- `dos <ip> <port>`: runs a SYN flood against a target
- `ddos <ip> <port> [n]`: runs a distributed SYN flood with `n` threads
- `icmp <ip> [packets]`: sends ICMP echo request packets
- `slowloris <ip> <port> [n]`: starts a Slowloris attack with `n` connections
- `stop_slowloris`: stops the current Slowloris attack
- `arp <target> <gateway>`: starts ARP spoofing between target and gateway
- `scan`: starts the preconfigured local network scan (192.168.1.0/24)
- `scan_stop`: stops the current scan
- `servers`: shows the servers found by the scan
- `connections`: shows locally detected game connections
- `fingerprint <ip> <port>`: attempts to identify the service running on a port
- `botnet`: opens the botnet menu for master/slave configuration
- `hunt`: enables automatic scan mode
- `clear`: clears the screen
- `exit` / `quit`: exits the application

## Added features

- local identification of game servers based on known ports
- detection of active game connections from `/proc/net/tcp` and `/proc/net/tcp6`
- real support for the `num_connections` parameter in the Slowloris attack
- simple fingerprinting for Minecraft and Source Engine

## Technical notes

- The `SlowlorisAttacker` module keeps HTTP connections open and sends partial headers to consume server resources.
- The `NetworkScanner` performs TCP probes on common game ports and stores discovered servers.
- The `fingerprint` function uses simplified requests to Minecraft and Source Engine to identify services.

## Future improvements

- configurable CIDR support for scanning
- recognition of more game protocols and non-standard servers
- advanced parsing of response messages for versions and maps
- authentication and encryption for the botnet
- file-based logging and reporting interface
