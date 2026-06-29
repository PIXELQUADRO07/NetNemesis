#!/bin/bash
echo "[*] Configurazione NetNemesis..."
mkdir -p build
cd build
cmake ..
make -j$(nproc)
echo "[*] Build completata. Esegui con: sudo ./netnemesis"
