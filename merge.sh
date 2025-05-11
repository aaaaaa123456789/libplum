#!/bin/bash

set -e
files=()
addblank=false

function already_included {
  local target="$1"
  for f in "${files[@]}"; do
    if [[ "$f" == "$target" ]]; then return 0; fi
  done
  return 1
}

function append_file {
  local file="$(realpath "$1")"
  if already_included "$file"; then return 0; fi
  if $addblank; then echo; fi
  addblank=false
  files+=("$file")
  while IFS= read -r line || [[ -n "$line" ]]; do
    if [[ $line =~ ^[[:space:]]*\#[[:space:]]*include[[:space:]]+\"(.*)\" ]]; then
      include="${BASH_REMATCH[1]}"
      append_file "${file%/*}/$include"
      addblank=true
    else
      echo "$line"
      addblank=false
    fi
  done < "$file"
}

for inputfile in "$@"; do
  append_file "$inputfile"
done
