#/bin/sh

_sudo="sudo"

if [ "$OSTYPE" = "cygwin" ]; then
	_sudo=""
	[ ! -d /usr/local/bin ] && mkdir -p /usr/local/bin
	[ -f /usr/local/bin/sq ] && $_sudo rm /usr/local/bin/sq
else
	[ -L /usr/local/bin/sq ] && $_sudo rm /usr/local/bin/sq
fi

. build.sh double $*
[ $? -ne 0 ] && exit 1
$_sudo ln -s "$SQ_PATH_BIN" /usr/local/bin/sq
