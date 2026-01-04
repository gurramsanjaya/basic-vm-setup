#!/bin/bash
set -e

: ${RESULTS_FILE:=aggregated_blocklist}
: ${DENY_URLS_FILE:=deny_urls}
: ${DENY_EXTRAS:=true}
: ${DENY_EXTRAS_FILE:=deny_extras}
: ${FORCE_ALLOW:=true}
: ${FORCE_ALLOW_FILE:=force_allow}

# this matches anything that doesn't start with a '#' and has no spaces in the line
AWK_PRG='/^[^#][^[:space:]]*$/'

mapfile -t URLS < <(awk "$AWK_PRG" "$DENY_URLS_FILE" )

> "$RESULTS_FILE"
for u in "${URLS[@]}"; do
  echo "Trying $u ..."
  # echo "##  DenyList from ${RESULTS_FILE}" >> "$RESULTS_FILE"
  curl -sSL "$u" >> "$RESULTS_FILE"
done

if [[ $DENY_EXTRAS == "true" ]]; then
  echo "Adding extra denylist from ${DENY_EXTRAS_FILE}"
  # echo "## Extras from ${DENY_EXTRAS_FILE}" >> "$RESULTS_FILE"
  cat "$DENY_EXTRAS_FILE" >> "$RESULTS_FILE"
fi

## force allow, might not work correctly...
# this will pick up every line from force allow file
if [[ $FORCE_ALLOW == "true" ]]; then
  while IFS= read -r line; do
    echo "Force allowing ${line} ..."
    sed -i '/^[^[:space:]]*'"$line"'[^[:space:]]*$/d' "$RESULTS_FILE"
  done < <(awk "$AWK_PRG" "$FORCE_ALLOW_FILE")
fi
