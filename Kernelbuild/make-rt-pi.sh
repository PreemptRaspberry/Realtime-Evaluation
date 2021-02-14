#!/bin/bash
# This is an automated script which compiles the RT Raspberry Kernel
#set -e
#---------------------------------------------------------------------#
#            ~Global variables. You can chage these~
PATCH_FILE=          #...leave empty or paste path to *.patch RT patch
_MENU_CONFIG=        #...menuconfig/xconfig/nconfig/gconfig or leave empty
TARGET_PATH=raspi-os #...where to clone the raspi git repo (relational to pwd)
INSTALL_MOD_PATH=mnt/ext4 # where to install the modules (also relational)
INSTALL_DTBS_PATH=mnt/fat32
ARCH_BITS=32         #...32 bit or 64 bit build
RPI_VERSION=3        #...RPi 3 or 4
#---------------------------------------------------------------------#

####################
#                  #
#  Main function   #
#                  #
####################
function main() {
  initVerbosity
  checkDependencies || exit 1

  echo "All dependencies there! Let's go"

  if [ ! -d "$TARGET_PATH" ]; then
    debugOutput "Cloning from git"
    cat <<EOF
###############################
Cloning Raspi Sources from GitHub

EOF
      git clone --depth=1 https://github.com/raspberrypi/linux $TARGET_PATH
  else
    debugOutput "$(realpath $TARGET_PATH) given as git directory" 
    read -p "Revert all changes to HEAD and ${UNDL}delete${NUNDL} \
all unversioned files? [Y/n] " -n 1 -r
    if echo $REPLY | grep -q ^'[yY]'$; then
      chd $TARGET_PATH
      if [ "$LOCAL_CONFIG" = 'true' ]; then
	debugOutput "Backing up local .config to ../.config.bak"
       	cp -n .config ../.config.bak || error 'An error occurred while saving your local ´.config´ file'
      fi
      git clean -fd
      git reset --hard HEAD
      git pull --rebase=true

      chd $LAST_PWD
    fi
  fi
  [ -d $TARGET_PATH ] || error "Cannot access directory $TARGET_PATH"

  chd $TARGET_PATH
  if [ "$PATCH_FILE" ] && [ -s $PATCH_FILE ]; then 
    echo "Provided patch-file at $PATCH_FILE"
  else
    echo '##############################'
    echo 'make mrproper (Cleaning source path)'
    echo
    make mrproper

    downloadPatchFile
  fi

  if [ "$LOCAL_CONFIG" != 'true' ]; then
    prepareConfigFile
  fi

  echo '##############################'
  echo Patching
  chd $TARGET_PATH
  cat $(realpath $PATCH_FILE) | patch $QUIET -p1 || error 'Patching failed'

  if [ "$LOCAL_CONFIG" = 'true' ]; then
    mv ../.config.bak .config
  fi

# Ask to edit config file
  echo '##############################'
  read -p 'Edit the config file now and enable PREEMPT_RT [Y/n] ' -n 1 -r
  if ! echo $REPLY | grep -q ^'[Nn]'$; then
    if [ -n $_MENU_CONFIG ]; then
      makeTheMake "$_MENU_CONFIG"
    else
      error 'Could not find any driver to run ´xconfig´ or ´menuconfig´'
      echo Opening your editor $EDITOR now...
      sleep 0.7
      $EDITOR ./.config
    fi
  fi

  if ! grep -q "PREEMPT\_RT=y" .config; then
    echo $RED WARNING! No PREEMPT_RT enabled. $NC
  fi


# Compiling now
  cat << EOF

###############################################################
###############################################################

Cross-compiling the kernel now
   > make ARCH=arm[/64] KERNEL=kernel[7/8] CROSS_COMPILE=(arm/aarch)[/64]-linux-gnu[eabihf-/-] [z/]Image modules dtbs

###############################################################
###############################################################

EOF
  compileKernel && askToBurnToSDCard

} #end of main()

##########################################################################################
#       ~ functions ~

###################
#                 #
#checkDependencies#
#                 #
###################
function checkDependencies() {
  debugOutput "Check dependencies..."
  RET=0
  CMDs="git gunzip wget bc make"
  #bison flex 
  LIBs="libssl libc6"

# Check if 64 or 32 bit architecture specified
  if [ $ARCH_BITS -ne 32 ] && [ $ARCH_BITS -ne 64 ]; then
    read -p 'Please specify 32 or 64 bit build: ' -r
    ARCH_BITS=$REPLY
  fi
  if [ $ARCH_BITS -eq 32 ]; then
    checkCommand arm-linux-gnueabihf-gcc && _CROSS_COMPILER='arm-linux-gnueabihf-'
    checkCommand arm-none-eabi-gcc && _CROSS_COMPILER='arm-none-eabi-'
    ARCH='arm'
  elif [ $ARCH_BITS -eq 64 ]; then
    checkCommand 'aarch64-linux-gnu-gcc' && _CROSS_COMPILER='aarch64-linux-gnu-'
    checkCommand 'arm64-linux-gnu-gcc' && _CROSS_COMPILER='arm64-linux-gnu-'
    ARCH='arm64'
  else
    error 'Arch (32/64) not given' && exit 1
  fi
  debugOutput "_CROSS_COMPILER = $_CROSS_COMPILER"

  # Check neccessary commands
  for cmd in $CMDs; do
    soFar="$missing"
    checkCommand $cmd || missing="$soFar $cmd"
  done

# Check neccessary libraries
  for lib in $LIBs; do
    soFar="$missing"
    checkLib $lib  || missing="$soFar $lib"
  done

  # Determining which menuconfig to use
  if [  -z "$_MENU_CONFIG" ]; then
    if ls /usr/lib/ | grep ^qt >$DEVNULL ; then
      _MENU_CONFIG=xconfig
    elif checkLib 'libncurses' ; then
      _MENU_CONFIG=menuconfig
    fi
  fi
  debugOutput "Menu to use for Kconfig: $_MENU_CONFIG"

  [ -z "$_CROSS_COMPILER" ] \
    && echo "Could not find suitable cross-compiler." \
    && if [ $ARCH_BITS -eq 32 ]; then \
     error 'Please install ´gcc-arm-linux-gnueabihf´ or ´arm-none-eabi-binutils´\nand corrosponding gcc'; \
   elif [ $ARCH_BITS -eq 64 ]; then \
     error 'Please install ´gcc-aarch64-linux-gnu´ or ´aarch64-linux-gnu-binutils´\nand corrosponding gcc'; \
    fi \
    && RET=1

  [ -n "$missing" ] \
    && error $(printf 'Missing Dependencies\\n please install: %s\\n' $missing) \
    && RET=1

  return $RET
}

checkCommand(){
  command -v $1 >$DEVNULL 
  return $?
}
checkLib(){
  /sbin/ldconfig -p | grep $1 1>/dev/null #$DEVNULL
  return $?
}

####################
#                  #
#prepareConfigFile #
#                  #
####################
function prepareConfigFile()
{
  chd $TARGET_PATH
  # Generate default config
  if [ $RPI_VERSION -eq 3 ]; then
    if [ $ARCH_BITS -eq 32 ]; then
      makeTheMake 'bcm2709_defconfig'
    else
      makeTheMake 'bcmrpi3_defconfig'
    fi
  else #if Pi4
    makeTheMake 'bcm2711_defconfig'
  fi
}

####################
#                  #
#downloadPatchFile #
#                  #
####################
function downloadPatchFile()
{
   debugOutput "target path $TARGET_PATH"
  [ $(pwd) = $(realpath $TARGET_PATH) ] && chd $(realpath $TARGET_PATH)
  RASPI_KERNEL_VERSION="$(cat .git/HEAD | sed -E 's/(.*rpi-)(.*)(\.y.*)/\2/')"
  if [ -z "$RASPI_KERNEL_VERSION" ]; then
    echo 'Could not find kernel version. Please check current git branch.'
    echo 'And paste the version string like ´x.x´ here: '
    read RASPI_KERNEL_VERSION
  fi
  URL="https://cdn.kernel.org/pub/linux/kernel/projects/rt/${RASPI_KERNEL_VERSION}/"

  debugOutput "RaspberryOS uses Linux Kernel ´$RASPI_KERNEL_VERSION´"
  chd $LAST_PWD

  if wget $URL $QUIET -O .tmpfile; then
     debugOutput "Try to find name of patch-file to download"
     [ $SUPER_VERBOSE = true ] && cat .tmpfile
     PATCH_FILE=$(grep "rt[0-9][0-9]\.patch\.gz" .tmpfile | sed -e 's/.*="//' | sed -e 's/">patch.*//')
     debugOutput "Found ´$PATCH_FILE´"
     rm .tmpfile
  fi
  if [ -z "$PATCH_FILE" ]; then
    cat <<EOF
##### ERROR ###################
Could not download newest patch file.
   Please copy the file name of the the newest patch-${RASPI_KERNEL_VERSION}****.patch.gz file
   from here: $URL
EOF
    read -p 'And paste it (*.gz) here: ' -r
    PATCH_FILE=$(echo $REPLY | grep '^patch.*\.patch.gz$')
    while [ -z $PATCH_FILE ]; do
      echo 'Not the expected form: patch(.*)patch.gz'
      read -p '   Try again: ' -r
      PATCH_FILE=$(echo $REPLY | grep '^patch.*\.patch.gz$')
    done
  fi
  
  cat <<EOF
###############################

Downloading $PATCH_FILE

EOF

  GZ_FILE=preempt_rt.patch.gz
  wget "$URL$PATCH_FILE" $QUIET -O $GZ_FILE
  PATCH_FILE=preemt_rt.patch
  if ! gunzip $GZ_FILE -c > $PATCH_FILE ; then
    cat <<EOF
This did not work! Please download the newest file in the form
   ´patch-${RASPI_KERNEL_VERSION}****.patch.gz´
manually and copy it to
   ´$(realpath ${GZ_FILE})´
EOF
    rm $(realpath ${GZ_FILE})
    read -p 'Press [RETURN] when done. ' -r
    [ -e $(realpath ${PATCH_FILE}) ] || error 'Cannot access $PATCH_FILE'
    gunzip $GZ_FILE -c > $PATCH_FILE || error 'Cannot unpack this file'
  fi

  PATCH_FILE=$(realpath $PATCH_FILE)
  echo "${GREEN}Patch is now located at $PATCH_FILE${NC}"
} #end of downloadPatchFile()

####################
#                  #
#  compileKernel   #
#                  #
####################
function compileKernel(){
  _JOBS="-j$(($(nproc)*15/10))" # number of cpu cores * 1.5 floored 
  cat <<EOF
You have two options now:
  [1] Make a fast build
        Will use $_JOBS threads
        You will definitely not be able to use this computer besides the build
  [2] Keep this computer available
        Let ´make´ use $(nproc) threads
        You will be able to work on this computer
EOF

  read -p 'Select an option [1]: ' -n 1 -r
  echo
  [ $REPLY -eq 2 ] && _JOBS="-j$(nproc)"

  debugOutput "Build the kernel now on $_JOBS threads"
  makeTheMake modules dtbs
}

####################
#                  #
#   makeTheMake    #
#                  #
####################
# checkDependencies() must be called before this function
# to set the right compiler
function makeTheMake(){
  debugOutput "CROSS_COMPILER=$_CROSS_COMPILER"
  debugOutput "ARCH=$ARCH"
  debugOutput "INSTALL_MOD_PATH=$INSTALL_MOD_PATH"
  debugOutput "INSTALL_DTBS_PATH=$INSTALL_DTBS_PATH"
  
  if [ $ARCH_BITS -eq 32 ]; then
    _IMAGE='zImage'
    KERNEL='kernel7'
    ARCH='arm'
  elif [ $ARCH_BITS -eq 64 ]; then
    _IMAGE='Image'
    KERNEL='kernel8'
    ARCH='arm64'
  fi

  debugOutput "$@"
  # don't add Image target when menu, 'modules_install' or 'default config' is called
  case "$@" in
    *"bcm2709_defconfig"* | *"bcm2711_defconfig"* | *"bcmrpi3_defconfig"*)
      _IMAGE="";;
    *"$_MENU_CONFIG"*)
      _IMAGE="";;
    *"modules_install"*)
      _IMAGE="";
      ADD_PATH="sudo /usr/bin/env PATH=$PATH";;
  esac

  debugOutput "$ADD_PATH make ${_JOBS} KERNEL=${KERNEL} ARCH=${ARCH} CROSS_COMPILE=${_CROSS_COMPILER} $_IMAGE $@"
  $ADD_PATH make ${_JOBS} KERNEL=${KERNEL} ARCH=arm64 CROSS_COMPILE=${_CROSS_COMPILER} "$@" $_IMAGE 
}

####################
#                  #
#   burnToSDCard   #
#                  #
####################
function askToBurnToSDCard(){
  read -p 'Do you want to copy the kernel to an SD card? [N/y] ' -n 1 -r
  if echo "$REPLY" | grep $QUIET '^[n|N]$' &>/dev/null ; then
   return 0
  fi

  cat <<EOF
Output of lslk:
  $(lsblk)

EOF

  read -p 'Specify block device (e.g. /dev/mmcblk0) ' -r
  if [ -e "$REPLY" ]; then
    BLK_DEV=$REPLY

    chd $LAST_PWD
    mkdir -p $INSTALL_DTBS_PATH
    mkdir -p $INSTALL_MOD_PATH
    if [ -e ${BLK_DEV}1 ];then
      _P1="${BLK_DEV}1"
      _P2="${BLK_DEV}2"
    else
      _P1="${BLK_DEV}p1"
      _P2="${BLK_DEV}p2"
    fi
    sudo mount $_P1 $INSTALL_DTBS_PATH || error "Could not mount $_P1"
    sudo mount $_P2 $INSTALL_MOD_PATH || error "Could not mount $_P2"
    
    cat <<EOF
###############################

Installing kernel modules to $_P2

EOF
    chd $TARGET_PATH
    makeTheMake "INSTALL_MOD_PATH=$INSTALL_MOD_PATH modules_install"

    cat <<EOF
###############################

Installing drivers to $_P1

EOF
    sudo cp ${INSTALL_DTBS_PATH}/$KERNEL.img ${INSTALL_DTBS_PATH}/$KERNEL-backup.img
    sudo cp ${TARGET_PATH}/arch/${ARCH}/boot/${_IMAGE} ${INSTALL_DTBS_PATH}/$KERNEL.img
    sudo cp ${TARGET_PATH}/arch/${ARCH}/boot/dts/overlays/*.dtb* ${INSTALL_DTBS_PATH}/overlays/
    sudo cp ${TARGET_PATH}/arch/${ARCH}/boot/dts/overlays/README ${INSTALL_DTBS_PATH}/overlays/
    if [ $ARCH_BITS -eq 64 ];then
      sudo cp ${TARGET_PATH}/arch/${ARCH}/boot/dts/broadcom/*.dtb ${INSTALL_DTBS_PATH}/
    else
      sudo cp ${TARGET_PATH}/arch/${ARCH}/boot/dts/*.dtb ${INSTALL_DTBS_PATH}/
    fi
    debugOutput "Unmounting $BLK_DEV"
    sudo umount ${INSTALL_DTBS_PATH}
    sudo umount $INSTALL_MOD_PATH
    echo You can now remove $BLK_DEV
    echo

    cat <<EOF
##################################################
#                                                #
#                    Done!                       #
#  Thank you for travelling with Deutsche Bahn!  #
#                                                #
##################################################
EOF
  else 
    error "$REPLY not found.\
  Run 'make-all.sh -f -g $(realpath --relative-to $LAST_PWD $TARGET_PATH)' again"
  fi
}

####################
#                  #
#     showHelp     #
#                  #
####################
function showHelp()
{
cat <<EOF
$0 -[h|fvgcp]
Automated building Raspberry OS with rt-patch
Licensed under: WTFPL

This script will:
   - Clone the raspberry github repo
   - (Try to automatically) download the matching preemt-rt patch
   - Apply a preconfigured .config file and let you edit that
   - Compile the kernel
   - Copy the image to a blockdevice (after asking)

Allowed arguments:
  -f		Just copy the kernel to an sd card
		Call this if you already built the kernel
  -g [path]	Give a path to an already pulled raspberry os
  -p [path]	Provide the patch-file if already downloaded
  -c		Use a local kernel config
		This '.config' file has to be in the root of the git repo
  -v		Be verbose
  -vv		Be more verbose
  -h|--help	Show this help

EOF
}

####################
#                  #
# verbose output   #
#                  #
####################
initVerbosity(){
  RED='\033[0;31m'
  GREEN='\033[0;32m'
  CYAN='\033[0;36m'
  NC='\033[0m'         #...no color
  UNDL=`tput smul`     #...underline
  NUNDL=`tput rmul`    #...no underline

  if [ "$BE_VERBOSE" = 'true' ];then
    QUIET='--quiet'
  fi
  if [ "$SUPER_VERBOSE" = 'true' ];then
    DEVNULL=${1:-/dev/stdout}
    QUIET='--verbose'
    cat <<EOF
------variables so far---------
BE_VERBOSE       '$BE_VERBOSE'
SUPER_VERBOSE    '$SUPER_VERBOSE'
LOCAL_CONFIG     '$LOCAL_CONFIG'
ARCH_BITS        '$ARCH_BITS'
LAST_PWD         '$LAST_PWD'
_CROSS_COMPILER  '$_CROSS_COMPILER'
_MENU_CONFIG     '$_MENU_CONFIG'
TARGET_PATH      '$TARGET_PATH'
GIT_PATH         '$GIT_PATH'
PATCH_FILE       '$PATCH_FILE'
RPI_VERSION      '$RPI_VERSION'
DO_COPY          '$DO_COPY'
-------------------------------
EOF
  else
    DEVNULL='/dev/null'
  fi
}

####################
#                  #
# helper functions #
#                  #
####################

debugOutput(){
  if [ "$BE_VERBOSE" = "true" ];then
    echo -e "${GREEN}[DEBUG] $@${NC}"
  fi
}

error(){
   echo -e "${RED}[ERROR] $@${NC}"
   exit 1
}

chd(){
  cd $@
  debugOutput "PWD now: $(pwd)"
}

###################
#                 #
# Executing here  #
#                 #
###################
LAST_PWD=$(pwd)      #...just to cd back
_CROSS_COMPILER=     #...Identifier string for arm gcc

###GETOPT
OPTS="getopt -o fhvvg:cp: -l help -- $@"
if [ $? -ne 0 ]; then echo "Unknown argument. For help type: $0 -h" >&2 ; exit 1 ; fi

while true; do
  case "$1" in
    -f) DO_COPY='true'; shift;;
    -v) BE_VERBOSE='true'; shift;;
    -vv) BE_VERBOSE='true'; SUPER_VERBOSE='true'; shift;;
    -p) PATCH_FILE=$(realpath $2);
      [ ! -e $PATCH_FILE ] && error "$PATCH_FILE not found!";
      shift 2;;
    -g) TARGET_PATH=$(realpath $2); shift 2;;
    -c) LOCAL_CONFIG='true'; shift;;
    -h) showHelp "$2"; exit 0;;
    --help) showHelp; exit 0;;
    *) break;;
  esac
done

# Making paths absolute
tmp=$(realpath $TARGET_PATH)
TARGET_PATH=$tmp
tmp=$(realpath $INSTALL_MOD_PATH)
INSTALL_MOD_PATH=$tmp
tmp=$(realpath $INSTALL_DTBS_PATH)
INSTALL_DTBS_PATH=$tmp

initVerbosity
checkDependencies

if [ $DO_COPY ];then
  askToBurnToSDCard
  exit 0
fi

main
chd $LAST_PWD

