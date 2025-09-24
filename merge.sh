#!/usr/bin/env bash

set -e
declare -A files
addblank=false

function append_file {
  local file
  if command -v grealpath >/dev/null 2>&1; then
    file="$(grealpath "$1")"
  else
    file="$(realpath "$1")"
  fi
  [[ -z ${files[$file]} ]] || return 0
  ! $addblank || echo
  addblank=false
  files["$file"]=true
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

for inputfile; do
  append_file "$inputfile"
done
