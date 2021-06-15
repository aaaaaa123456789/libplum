#!/bin/bash

declare -Ag files
addblank=false

function append_file {
  file=`realpath "$1"`
  if [[ "${files[$file]}" ]]; then return 0; fi
  if $addblank; then echo; fi
  addblank=false
  files[$file]=true
  while IFS= read -r line || [[ "$line" ]]; do
    if [[ "$line" =~ ^[[:space:]]*\#[[:space:]]*include[[:space:]]+\"(.*)\" ]]; then
      include="${BASH_REMATCH[1]}"
      append_file ${file%/*}/$include
      addblank=true
    else
      echo "$line"
      addblank=false
    fi
  done < $file
}

while [[ $# -gt 0 ]]; do
  append_file "$1"
  shift
done
