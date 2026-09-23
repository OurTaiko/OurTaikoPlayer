#!/usr/bin/env bash
set -euo pipefail

git_options=(-c credential.helper=)
case "${SKINS_ALLOW_INSECURE_TLS:-false}" in
  true)
    # Temporary exception requested for this host's expired certificate.
    # The -c setting applies to this fetch and its Git subprocesses only.
    git_options+=(-c http.https://ese.tjadataba.se/.sslVerify=false)
    ;;
  false) ;;
  *) echo '::error::SKINS_ALLOW_INSECURE_TLS must be true or false.'; exit 1 ;;
esac

if [[ -n "${GITEA_USER:-}" || -n "${GITEA_TOKEN:-}" ]]; then
  if [[ -z "${GITEA_USER:-}" || -z "${GITEA_TOKEN:-}" ]]; then
    echo '::error::Set both GITEA_USER and GITEA_TOKEN, or leave both unset for public skins.'
    exit 1
  fi
  auth=$(printf '%s:%s' "$GITEA_USER" "$GITEA_TOKEN" | base64 | tr -d '\r\n')
  echo "::add-mask::$auth"
  git_options+=(-c "http.https://ese.tjadataba.se/.extraheader=Authorization: Basic $auth")
fi

export GIT_TERMINAL_PROMPT=0
git submodule sync --recursive -- Skins/PyTaikoGreen
git "${git_options[@]}" submodule update --init --recursive -- Skins/PyTaikoGreen
