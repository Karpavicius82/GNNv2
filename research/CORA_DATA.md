# Cora dataset for probe_cora.cpp

probe_cora.cpp expects `cora/cora.content` and `cora/cora.cites` (the original
LINQS Cora release) in the working directory. Fetch:

    curl -sSL -o cora.tgz https://linqs-data.soe.ucsc.edu/public/lbc/cora.tgz
    tar xzf cora.tgz

probe_cora builds as a standalone target alongside every contract and probe:

    cmake -B build -S . && cmake --build build --target probe_cora

Run it from the directory that holds `cora/` (the probe reads the two files
relative to the working directory):

    ./build/probe_cora

The dataset itself is not committed.
