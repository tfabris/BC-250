#!/bin/bash

# ------------------------------------------------------------------------------
#                           SteamVersion.sh
#                        ©2026 by Tony Fabris
# ------------------------------------------------------------------------------
# Compares the available version of Steam that can be downloaded from the
# internet, to the most-recently installed version according to the logs.
# reports output to the screen. Returns 0 on exit if the version is correct,
# returns 1 if not.
#
# The purpose of this script was to work around a bug I have been encountering
# on my BC-250 gaming rig, where Steam keeps saying "Updates Available" even
# when Steam is up to date and Bazzite is up to date. This tells me if that's
# really true or not. See the accompanying README.md for more details.
#
# Place this script in your user's $HOME folder, and set it to executable with
# this command:
#
#      chmod +x SteamVersion.sh
#
# Then add it to Steam as a "Non-Steam" game. In the "Target" box, put this:
#
#      env -u LD_PRELOAD -u LD_AUDIT konsole -e "$HOME/SteamVersion.sh" & exit
#
# NOTES:
#
# This is for the version of Steam running on my Bazzite installation on my
# BC-250. The local bootstrap_log.txt path might need to be changed if this is
# running on a different kind of system.
#
# At least one "check for updates" must have been run (either automatically or
# manually) in order for it to have been logged and for it to show up. This is
# especially true if you have recently changed whether or not you have opted-in
# to the public beta. When you change between Beta and Stable it does an
# installation of that version without the particular version line showing up
# in the log. Only after the first "check for updates" after the changeover
# will this script produce accurate output.
# ------------------------------------------------------------------------------

echo "Independently verifying whether or not the Steam Client needs an update."
echo "(This is a workaround for a bug where it always says an update is needed.)"
echo ""

# Steam's local bootstrap log file which contains the information we need, which
# is the locally installed version number, and whether or not we have opted-in
# to be part of the Steam Public Beta program.# .
LOG_FILE="$HOME/.local/share/Steam/logs/bootstrap_log.txt"

# The amount of time, in seconds, to pause and wait for the user to read the
# text output before closing and exiting the program.
PAUSE_TIME=8

# Command Steam to perform a "Check for Updates". This is an asynchronous
# command, so we have no simple way to wait until Steam is done. So just pause
# an arbitrary amount of time. About the same amount of time that we give the
# user to read the text output should be enough.
echo "Checking for Steam client updates..."
xdg-open "steam://checkforupdates"
sleep $PAUSE_TIME

# Determine if we are currently in the Steam Public Beta or not. Do this by
# checking if the most recent "opt-in" line in the logs said something similar to
# "steamdeck_stable", "publicbeta", or something else. If the answer is beta or
# stable, determine which of the two possible URLs that we check for the
# version available online. If not, exit with an error.
BETA_LINE=$(grep "Opted in to client beta" "$LOG_FILE" | tail -n 1)
BRANCH=$(echo "$BETA_LINE" | awk -F"'" '{print $2}')
case "$BRANCH" in
    "")
        echo "❌ 'Opted in to client beta' was not found in the log file."
        echo "❌ In Steam settings, select 'Check for Updates' so that the log file is populated."
        sleep $PAUSE_TIME
        exit 1
        ;;
    *beta*)
        echo "Found a beta branch in the log file: $BRANCH"
        URL="https://client-update.steamstatic.com/steam_client_publicbeta_ubuntu12"
        ;;
    *stable*)
        echo "Found a stable branch in the log file: $BRANCH"
        URL="https://client-update.steamstatic.com/steam_client_ubuntu12"
        ;;
    *)
        echo "❌ Unknown branch format in the log file, neither Stable nor Beta: $BRANCH"
        sleep $PAUSE_TIME
        exit 1        
        ;;
esac

# Get the local Steam version number based on the log, and validate it. 
LOCAL_VER=$(grep "installed version" "$LOG_FILE" | tail -n 1 | sed -n 's/.*installed version \([0-9]\+\).*/\1/p')
if [[ ! "$LOCAL_VER" =~ ^[0-9]+$ ]] || [[ "$LOCAL_VER" -eq 0 ]]; then
    echo "❌ Failed to retrieve a local version number. Check $LOG_FILE for problems."
    sleep $PAUSE_TIME
    exit 1
fi

# Get the available Steam version number from the web, and validate it.
WEB_VER=$(curl -s "$URL" | grep -o '"version"[[:space:]]\+"[0-9]\+"' | grep -o '[0-9]\+')
if [[ ! "$WEB_VER" =~ ^[0-9]+$ ]] || [[ "$WEB_VER" -eq 0 ]]; then
    echo "❌ Failed to retrieve a Steam version number from the web. Check internet connection."
    sleep $PAUSE_TIME
    exit 1
fi

# Print results, pause long enough for the user to read it, then exit.
echo "-----------------------------------"
echo "Steam Version"
echo "-----------------------------------"
echo "Branch configured: $BRANCH"
echo "Locally Installed: $LOCAL_VER"
echo "Latest on Web:     $WEB_VER"
echo "-----------------------------------"
if [ "$LOCAL_VER" -eq "$WEB_VER" ]
then
    echo "✅ Up to date! Installed version matches the web."
    sleep $PAUSE_TIME
    exit 0
else
    echo "⚠️ Update available! Installed version does not match the web."
    sleep $PAUSE_TIME
    exit 1
fi
