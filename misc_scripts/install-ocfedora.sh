#!/usr/bin/env bash

# AUTHOR        Kyle A. Matheny <kamathen@us.ibm.com>
# AUTHOR        Alessandro Perucchi <peru@ch.ibm.com>
# AUTHOR        Bhavya Maheshwari <bhavya.maheshwari@in.ibm.com>
#
# Purpose       Installation of the base Open Client layer for
#               Fedora 22


# ===---===---===---===---===---===---===---===---===---===---===---===---===---
#
# Please place all declarations here

if [[ -f /etc/os-release ]]
then
	. /etc/os-release
else
	echo "Problem with your installation could not find /etc/os-release"
	exit 1
fi

#use target as the minimum supported release as per
#http://w3.the.ibm.com/ecmweb/cybersecurity/approved_os.html#Linux
#usually releases ahead of above link are supported #ibm-moves-slowly
declare -A TARGET=([DISTRO]='Fedora' [ARCH]=64 [RELEASE]=21)
declare -A CURRENT=([DISTRO]=$NAME [ARCH]=$(getconf LONG_BIT) [RELEASE]=$VERSION_ID)

OC_RELEASE_RPM=openclient-release-${CURRENT[RELEASE]}-1.noarch
LOGFILE="/tmp/${0##*/}.$(date +%Y%m%d%H%M%S).log"

export PRGNAME=$0
FIRST_INSTALL="YES"
IBM_REG_TOOL="YES"
INSTALL_DNF_PLUGINS="NO"
MEMTEST="YES"

# ===---===---===---===---===---===---===---===---===---===---===---===---===---

function usage {

  cat << EOF

Installation script for IBM Fedora ${CURRENT[RELEASE]} Open Client Layer
=======================================================

Usage: $PRGNAME [-h][-r][-p][-y]

where:
        -h : this help
        -r : run IBM Registration Tool only
        -p : Do not install the ibm-yum-plugins packages
             (no access to licensed software e.g. IBM Java will be provided)
        -y : install only the IBM Yum Plugins
             (enables access to licensed software e.g. IBM Java)

This installation script will (by default):
  1) Install the basic IBM Fedora ${CURRENT[RELEASE]} Open Client layer
      (the base layer aids compliance with IBM standard ITCS300)
  2) Install the ibm-yum-plugins which are used to enable access to a repository of licensed software
     (e.g. IBM Java that is not available in an open repository)
  3) Finally, register this computer with the IBM OpenClient team so that usage can be tracked.

EOF
}

# ===---===---===---===---===---===---===---===---===---===---===---===---===---

function abortInstallation {
	printMessage E "Installation failed (Please check the logs at $LOGFILE)"
	exit 1
}

# ===---===---===---===---===---===---===---===---===---===---===---===---===---

function printMessage {
	typeset TYPMSG="$1"
	typeset MESSAGE="$2"

	typeset TMPSTMP=$(date +"%D %T")

	typeset OUTMSG="${TYPMSG} $TMPSTMP : $MESSAGE"

	case $TYPMSG in
		n) echo | tee -a $LOGFILE ;;
		V) echo "$OUTMSG" >> $LOGFILE ;;
		*) echo "${TYPMSG}: $MESSAGE"
		   echo "$OUTMSG" >> $LOGFILE
		   ;;
	esac
}

# ===---===---===---===---===---===---===---===---===---===---===---===---===---

printMessage V "Launching: $0 $@"

printMessage I "Welcome to the installation of the IBM ${CURRENT[DISTRO]} \
               ${CURRENT[RELEASE]} Base Open Client Layer \
               (v$(awk '/^# Version/ {print $NF}' $0))"
printMessage n

# ===---===---===---===---===---===---===---===---===---===---===---===---===---
#
# Check if we are in a non 64bit distribution

if [[ ${CURRENT[ARCH]} != '64' ]]
then
	printMessage W "You are using ${CURRENT[DISTRO]} ${CURRENT[RELEASE]} in ${CURRENT[ARCH]}bit."
	printMessage W "Please beware that the IBM Fedora Community does not actively work"
	printMessage W "on this version, all development is done for ${CURRENT[DISTRO]} ${TARGET[ARCH]}bit."
	printMessage W "We will try to give support as 'best effort'"
	printMessage n
	printMessage W "32bit support might be dropped at any time, if we lack volunteers."
	printMessage n

	while true
	do
		read -p "Do you wish to continue installing using ${CURRENT[ARCH]}bit? [Y/N] " CONT32
		case $CONT32 in
			[Yy]* )
				break;;
			[Nn]* )
				printMessage W "Exiting Installation"
				exit 1;;
			* ) printMessage W "Please answer yes or no.";;
		esac
	done
fi

# ===---===---===---===---===---===---===---===---===---===---===---===---===---
#
# Check for any flags passed to the script

while getopts "rhpy" opt
do
	case $opt in
		r) IBM_REG_TOOL="YES"
		   if [[ -z $Y_OPTION ]]; then
		   	INSTALL_DNF_PLUGINS="NO"
		   fi
		   FIRST_INSTALL="NO"
		   R_OPTION="YES"
		   ;;
		p) INSTALL_DNF_PLUGINS="NO"
		   ;;
		y) INSTALL_DNF_PLUGINS="YES"
		   if [[ -z $R_OPTION ]]; then
		   	IBM_REG_TOOL="NO"
		   fi
		   FIRST_INSTALL="NO"
		   Y_OPTION="YES"
		   ;;
		h) usage
		   exit 1
		   ;;
	esac
done

printMessage V "Installation with options: IBM_REG_TOOL=$IBM_REG_TOOL, \
                FIRST_INSTALL=$FIRST_INSTALL, \
                INSTALL_DNF_PLUGINS=$INSTALL_DNF_PLUGINS, R_OPTION=$R_OPTION, \
                Y_OPTION=$Y_OPTION"

# ===---===---===---===---===---===---===---===---===---===---===---===---===---
#
# Only a root user should be executing this script

if [[ $(id -un) != "root" ]]
then
	printMessage E "You must be root to run this command."
	printMessage E "for example:  sudo $0 $@"

	abortInstallation
fi

# ===---===---===---===---===---===---===---===---===---===---===---===---===---
#
# Install on Fedora 20 machines, only

if [[ ${CURRENT[RELEASE]} -le ${TARGET[RELEASE]} ]]
then
	printMessage E "You must install this layer with a Fedora distribution later than ${TARGET[RELEASE]}"
	printMessage E "Unfortunately you are using Fedora ${CURRENT[RELEASE]}"

	abortInstallation
fi

# ===---===---===---===---===---===---===---===---===---===---===---===---===---
#
# IBM network access is required for installing packages

ping -c1 -w5 w3.ibm.com > /dev/null 2>&1
if [[ $? -ne 0 ]]
then
	printMessage E "You need to be connected to the IBM network to launch this script."

	abortInstallation
fi

# ===---===---===---===---===---===---===---===---===---===---===---===---===---
#
# Correct Distro
# Correct Release
# Correct User Privs
# Connected to IBM Network
#
# Time to Rock 'n Roll !

printMessage I "All logs will be stored in $LOGFILE"

if [[ $FIRST_INSTALL == "YES" ]]
then


	# >> ===-| Memtest86+

	# Untested for F22. Skip

	# if [[ $MEMTEST == "YES" ]]
	# then
	# 	rpm -q memtest86+ > /dev/null 2>&1

	# 	if [ $? -ne 0 ]
	# 	then
	# 		printMessage I "Installing Memtest86+"

	# 		yum install -y memtest86+ >> $LOGFILE 2>&1
	# 		if [ $? -ne 0 ]
	# 		then
	# 			printMessage E "Memtest86+ did not install. Please see ${LOGFILE} for details."
	# 			MEMTEST="NO"
	# 		else
	# 			printMessage I "Done"
	# 			printMessage n
	# 		fi
	# 	fi


	# 	# ===-| Add Memtest86+ to GRUB2 |-===

	# 	if [[ $MEMTEST == "YES" ]]
	# 	then
	# 		printMessage I "Adding Memtest86+ to Grub2"
	# 		printMessage n

	# 		/usr/sbin/memtest-setup >> $LOGFILE 2>&1
	# 		grub2-mkconfig -o /boot/grub2/grub.cfg >> $LOGFILE 2>&1

	# 		cat /boot/grub2/grub.cfg|grep memtest86+ > /dev/null 2>&1
	# 		if [ $? -ne 0 ]
	# 		then
	# 			printMessage E "Memtest86+ did not get added to Grub2, please see ${LOGFILE} for details"
	# 		fi

	# 		printMessage I "Done"
	# 		printMessage n
	# 	fi

	# fi


	# >> ===-| OpenClient RPM Installation

	printMessage I "Installing the first Open Client Package (${OC_RELEASE_RPM})"

	dnf install -y --nogpgcheck http://ocfedora.hursley.ibm.com/fedora/${CURRENT[RELEASE]}/`uname -i`/${OC_RELEASE_RPM}.rpm >> $LOGFILE 2>&1
	if [[ $? -ne 0 ]]
	then
		printMessage E "${OC_RELEASE_RPM} could not be installed successfully, \
		                and retry again"
		abortInstallation
	fi

	rpm -q ${OC_RELEASE_RPM} > /dev/null 2>&1
	if [[ $? -ne 0 ]]
	then
		printMessage E "You might need to check your environment because \
		                ${OC_RELEASE_RPM} is apparently not installed, and \
		                retry again"
		abortInstallation
	fi

	printMessage I "Done"
	printMessage n


	# >> ===-| Install base layer

	printMessage I "Installing the 'IBM Client Base' groupinstall"

	dnf groupinstall -y "IBM Client Base" >> $LOGFILE 2>&1
	if [[ $(dnf groups summary "IBM Client Base" 2>/dev/null|awk -F':' '/^Installed groups:/i {print $2}') -eq 0 ]]
	then
		printMessage E "You might need to retry again, or check your network before"
		abortInstallation
	fi

	printMessage I "Done"
	printMessage n
fi

# ===---===---===---===---===---===---===---===---===---===---===---===---===---
#
# Install the dnf plugins if wanted (by default yes :-D)

if [[ $INSTALL_DNF_PLUGINS == "YES" ]]
then
	printMessage I "Installing the 'ibm-dnf-plugins'"

	dnf install -y ibm-dnf-plugins >> $LOGFILE 2>&1
	if [[ $? -ne 0 ]]
	then
		printMessage E "Could not install ibm-dnf-plugins, and please retry again."
		abortInstallation
	fi

	rpm -q ibm-dnf-plugins > /dev/null 2>&1
	if [[ $? -ne 0 ]]
	then
		printMessage E "You might need to retry again, or check your network before"
		abortInstallation
	fi

	printMessage I "Done"
	printMessage n
fi

# ===---===---===---===---===---===---===---===---===---===---===---===---===---
#
# Check if the IBM_REG_TOOL is installed or not

if [[ $IBM_REG_TOOL == "YES" ]]
then
	printMessage I "Starting IBM Registration Tool..."

	rpm -q ibmregtool > /dev/null 2>&1
	if [[ $? -ne 0 ]]
	then
		printMessage E "RPM package IBM_REG_TOOL is not installed, cannot run IBM Registration Tool."
		printMessage E "Please install the 'IBM Client Base' by running this script like that:"
		printMessage E "  sudo $0 [-p]"

		abortInstallation
	fi

	python /usr/share/ibmregtool/ibmregtool.py
	printMessage I "Done"
fi

printMessage I "Installation succeeded (logs are at : $LOGFILE)"

# ===---===---===---===---===---===---===---===---===---===---===---===---===---
#
# History
#
# (History has been trunkated. See F-20 install-ocfedora.sh for passed
#  changes.
#
# 26 December 2014 Bhavya Maheshwari
#                  - Ported to Fedora 21
#
#
# 5 January 2015   Kyle A. Matheny
#                  - Merge together changes
#
#
#
# 2 July 2015      Kyle A. Matheny
#                  - YUM to DNF
#                  - Remove MemTest86+
#                  - Remove ibm-yum-plugins
#                  - Bump Fedora Version to 22
#
#
# 16 Nov 2015      Bhavya Maheshwari
#				   				 - Changed all instances of yum to dnf
#                  - Changed ibm-yum-plugins to ibm-dnf-plugins
#                  - Fixed ibm-yum/dnf-plugins installation issue
#                  - Added version 23 alongwith 22
# 31 Dec 2015			Bhavya Maheshwari
#									 - Made script release agnostic
#									 - Will only work on versions above IBM approved_os
