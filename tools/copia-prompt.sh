#!/bin/bash
# Copia negli appunti il prompt per le AI esterne (docs/progetto/prompt-script-ai.md).
set -e
cd "$(dirname "$0")/.."
f=docs/progetto/prompt-script-ai.md
awk '/^=== INIZIO PROMPT ===$/{p=1; next} /^=== FINE PROMPT ===$/{p=0} p' "$f" | pbcopy
echo "Prompt copiato negli appunti: incollalo nella chat dell'AI."
