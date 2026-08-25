# Protocol fixture provenance

Protocol fixtures belong here once they are collected. Every fixture must be
safe to commit and must include a sidecar or manifest entry with:

- variant (`MD1`, `MD2`, `MD3`, or `MD4`);
- raw byte length and the notification boundary observed by the capture tool;
- source classification: `hardware-verified`, `capture-verified`, `app-derived`,
  `synthetic`, or `unavailable`;
- capture/source date and device context when known;
- expected normalized fields only when independently supported by the source;
- whether command bytes and acknowledgement behavior were tested on hardware.

T00 contains no response fixtures. The APK and its extracted JavaScript are
app-derived evidence, not wire captures, and are documented in
`doc/validation.md`. Do not turn lengths inferred from JavaScript indexes or
the six-block XOR loop into accepted wire lengths.

Do not commit credentials, BLE addresses, names, serial numbers, account data,
or unredacted captures containing sensitive identifiers. Prefer anonymized raw
bytes with a reproducible checksum and keep private originals outside the
repository. Synthetic fixtures must be labelled `synthetic` and must never be
described as hardware or capture evidence.
