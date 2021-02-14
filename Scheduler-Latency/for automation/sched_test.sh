#!/bin/bash
# test most linux schedulers
# c file by Jonas Gasteiger
# script by Valentin Brandner

mkdir bin            # for c-compilations
mkdir sched_results  # for result-files

### File(s) to compile
CFILE="sched_test.c"



function other(){
    echo -n "\
-DMODE_SCHED_OTHER "
}


function fifo(){
    #sed -i -e '/#define MODE/d' $CFILE
    echo -n "\
-DMODE_SCHED_FIFO "
}

function dead(){
    #sed -i -e '/#define MODE/d' $CFILE
    #sed -i -e '/#define DEADLINE_/d' $CFILE
    echo -n "\
-DMODE_SCHED_DEADLINE "
}

#-----------DEADLINE SLEEP - normal mode
function dead_sleep(){
    dead
    echo -n "\
-DSLEEP \
-DDEADLINE_RUNTIME=100000 \
-DDEADLINE_DEADLINE=100000 \
-DDEADLINE_PERIOD=1000000"
}
function dead_sleep_x10(){
    dead
    echo -n "\
-DSLEEP \
-DDEADLINE_RUNTIME=1000000 \
-DDEADLINE_DEADLINE=1000000 \
-DDEADLINE_PERIOD=10000000"
}


# These are to run w + w/o HRTICKS

#----------DEADLINE EXAMINE_MISSES
function dead_examine(){ # and this with MISSX=2,=10
    dead
    echo -n "\
-DEXAMINE_DEADLINE_MISS  \
-DDEADLINE_RUNTIME=10000 \
-DDEADLINE_DEADLINE=10000 \
-DDEADLINE_PERIOD=100000"
}
function dead_examine_x10(){ # and this with MISSX=2,=10
    dead
    echo -n "\
-DEXAMINE_DEADLINE_MISS  \
-DDEADLINE_RUNTIME=100000 \
-DDEADLINE_DEADLINE=100000 \
-DDEADLINE_PERIOD=1000000"
}

#---------DEADLINE NOSLEEP
function dead_nosleep1(){
    dead
    echo -n "\
-DNOSLEEP \
-DDEADLINE_RUNTIME=10000 \
-DDEADLINE_DEADLINE=10000 \
-DDEADLINE_PERIOD=100000"
}

function dead_nosleep2(){
    dead
    echo -n "\
-DNOSLEEP \
-DDEADLINE_RUNTIME=100000 \
-DDEADLINE_DEADLINE=100000 \
-DDEADLINE_PERIOD=1000000"
}

function runit(){
    echo $(date)
    echo -e "$GREEN > Running $@ $NC"
    # We need root privileges for some schedulers
    sudo "$@" || exit 1
}

function compile(){
   [ -e $CFILE ] || exit 2
	echo -e "$GREEN > COMPILE: gcc $CFILE -lpthread $@ $NC"
    gcc $CFILE -lpthread $@ || exit 1
}

GREEN='\033[1;32m'
NC='\033[0m'


echo file is $CFILE
### Compile it
compile $(other)                      -o bin/other
compile $(fifo)                       -o bin/fifo
compile $(dead_sleep)                 -o bin/dead_sleep
compile $(dead_sleep_x10)             -o bin/dead_sleep_x10

compile $(dead_examine) -DMISSX=4     -o bin/dead_examine_4
compile $(dead_examine_x10) -DMISSX=4 -o bin/dead_examine_x10
compile $(dead_examine) -DMISSX=4     -o bin/dead_examine_4_reclaim    -DSCHED_FLAG_RECLAIM=1
compile $(dead_examine_x10) -DMISSX=4 -o bin/dead_examine_x10_reclaim  -DSCHED_FLAG_RECLAIM=1
compile $(dead_nosleep1)              -o bin/dead_nosleep1
compile $(dead_nosleep2)              -o bin/dead_nosleep2

#### And run it
if sudo cat /sys/kernel/debug/sched_features | grep ' HRTICK ' &>/dev/null; then
    echo '############################################################'
    echo Now type:
    echo '   echo HRTICK >> /sys/kernel/debug/sched_features && exit'
    echo '############################################################'
    echo 
    sudo -i
fi

echo ----- Running now ----
runit bin/other
runit bin/fifo
runit bin/dead_sleep    
runit bin/dead_sleep_x10

runit bin/dead_examine_4
runit bin/dead_examine_x10_reclaim
runit bin/dead_examine_4_reclaim
runit bin/dead_examine_x10
runit bin/dead_nosleep1
runit bin/dead_nosleep2
mv results* sched_results
