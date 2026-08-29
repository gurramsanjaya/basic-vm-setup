#!/usr/bin/env bash
set -eo pipefail

SCRIPT_DIR=$(cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd)

: ${OUTPUT_DIR:="$SCRIPT_DIR"} # dummy value
: ${OUTPUT_FILE:="blocked-names.txt"}

: ${INPUT_DIR:="$SCRIPT_DIR"}
: ${DENY_URLS_FILE:=deny_urls}
## optional (will skip if file doesn't exist)
: ${DENY_EXTRAS_FILE:=deny_extras}
: ${FORCE_ALLOW_FILE:=force_allow}

OUTPUT_TEMP=$(readlink -f "${OUTPUT_DIR}/${OUTPUT_FILE}_temp")
DENY_EXTRAS=false
FORCE_ALLOW=false

## --- Function Block ---

function cleanup() {
  local exit_code=$?
  trap - EXIT SIGHUP SIGTERM SIGINT
  echo "cleaning up..."
  rm -f "$OUTPUT_TEMP"
  echo "script exited with code: $exit_code"
  exit $exit_code
}

# just to make sure the first entry of the next file/download doesn't get directly appended
# to the last entry of the previous download
function ensure_spacing() {
  echo >> "$1"
  sed -i '$ {/^$/d}' "$1"
}


## --- Main Block ---
trap "cleanup" EXIT SIGHUP SIGTERM SIGINT

# basic checks
OUTPUT_DIR=$(readlink -e "$OUTPUT_DIR")
OUTPUT_FILE=$(readlink -f "${OUTPUT_DIR}/${OUTPUT_FILE}")
INPUT_DIR=$(readlink -e "$INPUT_DIR")
DENY_URLS_FILE=$(readlink -e "${INPUT_DIR}/${DENY_URLS_FILE}")
if DENY_EXTRAS_FILE=$(readlink -e "${INPUT_DIR}/${DENY_EXTRAS_FILE}") ; then
  echo "deny extras file exists..."
  DENY_EXTRAS=true
fi
if FORCE_ALLOW_FILE=$(readlink -e "${INPUT_DIR}/${FORCE_ALLOW_FILE}") ; then
  echo "force allow file exists..."
  FORCE_ALLOW=true
fi

# this matches anything that doesn't start with a '#' and has no spaces in the line
AWK_PRG='/^[^#][^[:space:]]*$/'

mapfile -t URLS < <(awk "$AWK_PRG" "$DENY_URLS_FILE" )

> "$OUTPUT_TEMP"
for u in "${URLS[@]}"; do
  echo "Trying to fetch from $u ..."
  # echo "##  DenyList from ${OUTPUT_TEMP}" >> "$OUTPUT_TEMP"
  curl -sSL "$u" >> "$OUTPUT_TEMP"
  ensure_spacing "$OUTPUT_TEMP"
done

if [[ $DENY_EXTRAS == "true" ]]; then
  echo "Adding extra denylist from ${DENY_EXTRAS_FILE}"
  # echo "## Extras from ${DENY_EXTRAS_FILE}" >> "$OUTPUT_TEMP"
  cat "$DENY_EXTRAS_FILE" >> "$OUTPUT_TEMP"
  ensure_spacing "$OUTPUT_TEMP"
fi

## force allow, might not work correctly...
# this will pick up every line from force allow file
if [[ $FORCE_ALLOW == "true" ]]; then
  while IFS= read -r line; do
    echo "Force allowing ${line} ..."
    sed -i '/^[^[:space:]]*'"$line"'[^[:space:]]*$/d' "$OUTPUT_TEMP"
  done < <(awk "$AWK_PRG" "$FORCE_ALLOW_FILE")
fi

## remove any comments
echo "Removing any comments..."
sed -i '/^[[:space:]]*#/d' "$OUTPUT_TEMP"

## clean prefixes and port suffixes: (Ex. https://, http://, socks5://, socks4://, :5236, etc)
sed -i -E 's/^(http|https|socks4|socks5):\/\/([^:]+)(:[[:digit:]]+){0,1}[[:space:]]*$/\2/' "$OUTPUT_TEMP"

## finally sort it
echo "Sorting the file..."
sort -u "$OUTPUT_TEMP" > "$OUTPUT_FILE"
