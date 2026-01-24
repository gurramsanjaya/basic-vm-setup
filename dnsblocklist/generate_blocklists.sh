#!/bin/bash
set -eo pipefail

# TODO: make a github flow that autoupdates this or something....

: ${RESULTS_FILE:=aggregated_blocklist}
: ${DENY_URLS_FILE:=deny_urls}
: ${DENY_EXTRAS:=true}
: ${DENY_EXTRAS_FILE:=deny_extras}
: ${FORCE_ALLOW:=true}
: ${FORCE_ALLOW_FILE:=force_allow}


RESULTS_TEMP="${RESULTS_FILE}_temp"

function cleanup() {
    echo "cleaning up..."
    rm -f "$RESULTS_TEMP"
    trap - 0
}
trap "cleanup" 0 SIGHUP SIGTERM SIGINT

# just to make sure the first entry of the next file/download doesn't get directly appended
# to the last entry of the previous download
function ensure_spacing() {
  echo >> "$1"
  sed -i '$ {/^$/d}' "$1"
}

# this matches anything that doesn't start with a '#' and has no spaces in the line
AWK_PRG='/^[^#][^[:space:]]*$/'

mapfile -t URLS < <(awk "$AWK_PRG" "$DENY_URLS_FILE" )

> "$RESULTS_TEMP"
for u in "${URLS[@]}"; do
  echo "Trying $u ..."
  # echo "##  DenyList from ${RESULTS_TEMP}" >> "$RESULTS_TEMP"
  curl -sSL "$u" >> "$RESULTS_TEMP"
  ensure_spacing "$RESULTS_TEMP"
done

if [[ $DENY_EXTRAS == "true" ]]; then
  echo "Adding extra denylist from ${DENY_EXTRAS_FILE}"
  # echo "## Extras from ${DENY_EXTRAS_FILE}" >> "$RESULTS_TEMP"
  cat "$DENY_EXTRAS_FILE" >> "$RESULTS_TEMP"
  ensure_spacing "$RESULTS_TEMP"
fi

## force allow, might not work correctly...
# this will pick up every line from force allow file
if [[ $FORCE_ALLOW == "true" ]]; then
  while IFS= read -r line; do
    echo "Force allowing ${line} ..."
    sed -i '/^[^[:space:]]*'"$line"'[^[:space:]]*$/d' "$RESULTS_TEMP"
  done < <(awk "$AWK_PRG" "$FORCE_ALLOW_FILE")
fi

## remove any comments
echo "Removing any comments..."
sed -i '/^[[:space:]]*#/d' "$RESULTS_TEMP"

## finally sort it
echo "Sorting the file..."
sort "$RESULTS_TEMP" > "$RESULTS_FILE"
