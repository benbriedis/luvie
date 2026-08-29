#! /bin/bash 

# This is an old school example as Bristol doesn't provide a plugin version 
# and doesn't implement the NSM protocol. It also doesn't use Jack MIDI connections.
# Using it with a Bash script then is one of the better options.

# Start Jack if it isn't already running, then wait until it's up.
if ! jack_lsp >/dev/null 2>&1; then
	jackd -d alsa &
	while ! jack_lsp >/dev/null 2>&1; do sleep 0.5; done
fi

# Restore the saved patch if we have one. Bristol only writes it when you pick a
# memory slot and hit Save in its GUI, so to update prophet.mem do that once and
# then export/copy the slot's .mem file to here (~/.bristol/memory/prophet/).
PATCH="$PWD/prophet.mem"
[ -f "$PATCH" ] && IMPORT=(-import "$PATCH")
startBristol -jack -midi seq -prophet "${IMPORT[@]}" &

luvie bristol.luvie  &

sleep 3
aconnect 'luvie:inst1' 'bristol'


