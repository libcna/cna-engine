# Frozen easy-3d equivalence oracle

This directory contains the smallest test-only subset of
`openeggbert/easy-3d` needed by `Easy3DEquivalenceTests`. It was extracted
from commit `ad63bf9c176af5a798ad05e75c21e8802dda0f2f` when the retired library
was decoupled from cna-engine.

The code is intentionally private to the test executable. It is not an Easy3D
compatibility layer, is not installed, and must not be used by production
targets. Its purpose is to freeze the old algorithms so later engine changes
can still be checked against the migration baseline after the original
repository is archived or removed.

The original code is MIT-licensed by Robert Vokáč; see `LICENSE`.
