#!/bin/sh

case $0 in (/*|./*|../*) ME=$0 ;; (*) ME=./$0 ;; esac
BASE_DIR=${ME%/*}/build
DEST_DIR=${BASE_DIR}/dest

API=4.1

set -- \
	--enable-sandbox \
	--enable-javascript=false \
	"$@"

LD_LIBRARY_PATH="${DEST_DIR}/lib64${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
exec "${DEST_DIR}/libexec/webkit2gtk-${API}/MiniBrowser" "$@"
