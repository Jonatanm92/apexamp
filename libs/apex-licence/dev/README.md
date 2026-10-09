# Development signing key — NOT SECRET

`development-secret-key.txt` is the private half of the key pair that plugin
builds use when `APEX_LICENCE_PUBLIC_KEY` is not set. It is public on purpose,
so anyone working on the code can make test keys:

    apex_keygen issue libs/apex-licence/dev/development-secret-key.txt --owner "Tester"

Because anyone can make keys for it, **a build with the development key must
never be sold**. Such builds say "DEVELOPMENT BUILD" on the unlock screen.
For release builds make your own key pair (see SELLING.md) and configure with
`-DAPEX_LICENCE_PUBLIC_KEY=<your public key>`.
