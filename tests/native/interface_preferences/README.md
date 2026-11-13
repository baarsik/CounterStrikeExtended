# Native startup preference reader

```sh
python3 tests/native/interface_preferences/run.py \
  --root . --evidence build/interface-preferences-green
```

On macOS this compiles the actual startup reader and native settings backend with
ASan/UBSan. Only the copied backend header's preferences domain is replaced with
a unique `org.counterstrikeextended.fixture.interface.*` domain. The fixture
refuses the product domain, writes only disposable fixture values and clears that
domain in a separate cleanup process even if a test fails. Source hashes and the
chosen domain are recorded beside the executable. No personal game/Steam setting
is changed, and no production injection point is added.

The same reader's Windows branch is compiled against a controlled, query-only
registry API. It covers the exact HKCU key/value, missing/open/query errors,
handle closure, REG_SZ versus binary, empty/valid/case-normalized and unbundled
identifiers, nonterminated/embedded-NUL data and the 128-byte boundary. This
also invokes the actual dedicated resolver for every record: valid, missing,
unterminated, hidden-suffix and wrong-type data must produce the same selection
as early startup. Safe custom CLI overrides still win, and invalid CLI values
retain strict saved-record validation. This checks the production branch and its
API contract; an actual Windows SDK build
and Windows process startup remain separate validation.

The native macOS reader/backend coverage includes missing/automatic, Russian,
Polish, mixed case, path/whitespace/custom/oversized identifiers and malformed
byte records. [Consumer selection](../interface_language/README.md) separately
checks that root/fallback paths, the surface and both dedicated GUI consumers
agree on command-line > saved > Steam/English precedence.

Before the new reader existed this runner reported its missing production
feature in `build/interface-preferences-red`.
