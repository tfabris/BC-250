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

# Get local version number based on the logs. 
LOG_FILE="$HOME/.local/share/Steam/logs/bootstrap_log.txt"
LOCAL_VER=$(grep "installed version" "$LOG_FILE" | tail -n 1 | sed -n 's/.*installed version \([0-9]\+\).*/\1/p')

# Determine if the most recent opt-in line said "steamdeck_stable"
# or "publicbeta".
BETA_LINE=$(grep "Opted in to client beta" "$LOG_FILE" | tail -n 1)
if echo "$BETA_LINE" | grep -q "publicbeta"
then
    BRANCH="beta"
else
    BRANCH="stable"
fi

# Get web version based on the chosen branch.
if [ "$BRANCH" = "beta" ]
then
    URL="https://client-update.steamstatic.com/steam_client_publicbeta_ubuntu12"
else
    URL="https://client-update.steamstatic.com/steam_client_ubuntu12"
fi
WEB_VER=$(curl -s "$URL" | grep -o '"version"[[:space:]]\+"[0-9]\+"' | grep -o '[0-9]\+')

# Print results.
echo "-----------------------------------"
echo "Steam Version"
echo "-----------------------------------"
echo "Branch configured: $BRANCH"
echo "Locally Installed: $LOCAL_VER"
echo "Latest on Web:     $WEB_VER"
echo "-----------------------------------"
if [ -z "$LOCAL_VER" ] || [ -z "$WEB_VER" ]
then
    echo "❌ Error: Could not retrieve one or both version numbers."
    sleep 5
    exit 1
elif [ "$LOCAL_VER" -eq "$WEB_VER" ]
then
    echo "✅ Up to date! Your installed version matches the web."
    sleep 5
    exit 0
else
    echo "⚠️ Update available! Your version does not match the web."
    sleep 5
    exit 1
fi
