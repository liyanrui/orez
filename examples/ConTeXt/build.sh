#!/bin/bash
cp ~/var/nikola/agn-project/meta-doc/agn_km.orz km.orz
orez -w km.orz | orez-ctx > km.tex
ctx km
cp km.pdf ~/var/nikola/agn-project/files/
