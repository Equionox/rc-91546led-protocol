#!/bin/bash
# Schiebt dieses Repository zu GitHub. Vorher auf github.com anlegen:
#   New repository -> Name: rc-91546led-protocol -> Public
#   KEIN README, KEINE Lizenz, KEIN .gitignore ankreuzen.
set -e
BENUTZER="${1:-Equionox}"
NAME="rc-91546led-protocol"

echo "Identitaet in diesem Repository:"
git config user.name; git config user.email
echo

if ! git remote get-url origin >/dev/null 2>&1; then
    git remote add origin "git@github.com:$BENUTZER/$NAME.git"
    echo "Gegenstelle gesetzt: git@github.com:$BENUTZER/$NAME.git"
fi

echo "SSH-Zugang pruefen ..."
ssh -T git@github.com 2>&1 | head -2 || true
echo
git push -u origin main
