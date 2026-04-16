# AUR packaging

The `PKGBUILD` in this directory is what the **Release** GitHub Actions
workflow publishes to [aur.archlinux.org](https://aur.archlinux.org/) on
each tagged release.

## Enabling AUR publishing

The `aur-release` job in `.github/workflows/release.yml` is gated by the
repository variable `AUR_ENABLED`. To turn it on:

1. Log in to [aur.archlinux.org](https://aur.archlinux.org/) and create an
   empty package named `hyprmark`.
2. Generate a dedicated SSH key pair (`ssh-keygen -t ed25519 -f aur_key`)
   and add the public key to your AUR account.
3. In the GitHub repo settings, add these **secrets**:
   - `AUR_SSH_KEY` — contents of the private key
   - `AUR_USERNAME` — the name to use in git commits (e.g. `Robin Duckett`)
   - `AUR_EMAIL` — the email to use in git commits
4. Add the **variable** `AUR_ENABLED` with value `true`.
5. Tag a release (`git tag v0.1.1 && git push origin v0.1.1`).

The job will render the PKGBUILD with the tag version and push it to AUR.
