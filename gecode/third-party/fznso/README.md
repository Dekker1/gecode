# The vendored FZnSO headers

`fznso_types.h`, `fznso.hpp` and `fznso_export.hpp` are copied verbatim from an
FZnSO release — `c/` and `cpp/` in that checkout. Each includes the next by a
relative path, so the three have to stay side by side, and none of them may be
edited here.

They are header-only and describe a protocol, not a library: nothing is linked
against them. Vendoring is what lets `fznso-gecode` build from this repository
alone, with no second checkout to point a cache variable at.

`FZNSO_ABI_VERSION` in `fznso_types.h` is what `FZNSO_EXPORT_SOLVER` reports, and
a consumer rejects a solver whose version differs from its own. Re-vendor all
three together.
