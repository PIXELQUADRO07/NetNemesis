#!/bin/bash
echo "[*] Configurazione NetNemesis..."
mkdir -p build
cd build
cmake ..
make -j$(nproc)
echo "[*] Build completed. Run with: sudo ./netnemesis"
