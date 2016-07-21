#! /bin/sh
# Borrowed from Linux 2.0.40 kernel source tree and modyfied, as its GPL
#
# This script is used to configure the linux kernel.
#
# It was inspired by the challenge in the original Configure script
# to ``do something better'', combined with the actual need to ``do
# something better'' because the old configure script wasn't flexible
# enough.
#
# Please send comments / questions / bug fixes to raymondc@microsoft.com.
#
#              ***** IMPORTANT COMPATIBILITY NOTE ****
# If configuration changes are made which might adversely effect 
# Menuconfig or xconfig, please notify the respective authors so that 
# those utilities can be updated in parallel.
#
# Menuconfig:  <roadcapw@titus.org>
# xconfig:     <apenwarr@foxnet.net>  <eric@aib.com>
#              ****************************************
#
# Each line in the config file is a command.
#
# 050793 - use IFS='@' to get around a bug in a pre-version of bash-1.13
# with an empty IFS.
#
# 030995 (storner@osiris.ping.dk) - added support for tri-state answers,
# for selecting modules to compile.
#
# 180995 Bernhard Kaindl (bkaindl@ping.at) - added dummy functions for
# use with a config.in modified for make menuconfig.
#
# 301195 (boldt@math.ucsb.edu) - added help text support
#
# 281295 Paul Gortmaker - make tri_state functions collapse to boolean
# if module support is not enabled.
#
# 010296 Aaron Ucko (ucko@vax1.rockhurst.edu) - fix int and hex to accept
# arbitrary ranges
#
# 150296 Dick Streefland (dicks@tasking.nl) - report new configuration
# items and ask for a value even when doing a "make oldconfig"
#
# 200396 Tom Dyas (tdyas@eden.rutgers.edu) - when the module option is
# chosen for an item, define the macro <option_name>_MODULE
#
# 090397 Axel Boldt (boldt@math.ucsb.edu) - avoid ? and + in regular 
# expressions for GNU expr since version 1.15 and up use \? and \+.

# 040697 Larry Augustin (lma@varesearch.com) - integer expr test
# fails with GNU expr 1.12. Re-write to work with new and old expr.

#
# Make sure we're really running bash.
#
# I would really have preferred to write this script in a language with
# better string handling, but alas, bash is the only scripting language
# that I can be reasonable sure everybody has on their linux machine.
#
[ -z "$BASH" ] && { echo "Configure requires bash" 1>&2; exit 1; }

# Disable filename globbing once and for all.
# Enable function cacheing.
set -f -h

#
# Dummy functions for use with a config.in modified for menuconf
#
function mainmenu_option () {
    :
}
function mainmenu_name () {
    :
}
function endmenu () {
    :
}

#
# help prints the corresponding help text from Configure.help to stdout
#
#       help variable
#
function help () {
  if [ -f Documentation/Configure.help ]
  then
     #first escape regexp special characters in the argument:
     var=$(echo "$1"|sed 's/[][\/.^$*]/\\&/g')
     #now pick out the right help text:
     text=$(sed -n "/^$var[     ]*\$/,\${
            /^$var[     ]*\$/b
            /^#.*/b
            /^[     ]*\$/q
            p
            }" Documentation/Configure.help)
     if [ -z "$text" ]
     then
      echo; echo "  Sorry, no help available for this option yet.";echo
     else
      (echo; echo "$text"; echo) | ${PAGER:-more}
     fi
  else
     echo;
     echo "  Can't access the file Documentation/Configure.help which"
     echo "  should contain the help texts."
     echo
  fi
}


#
# readln reads a line into $ans.
#
#   readln prompt default oldval
#
function readln () {
    if [ "$DEFAULT" = "-d" -a -n "$3" ]; then
        echo "$1"
        ans=$2
    else
        echo -n "$1"
        # [ -z "$3" ] && echo -n "(NEW) "
        IFS='@' read ans </dev/tty || exit 1
        [ -z "$ans" ] && ans=$2
    fi
}

#
# comment does some pretty-printing
#
#   comment 'xxx'
# 
function comment () {
    echo "*"; echo "* $1" ; echo "*"
    (echo "" ; echo "#"; echo "# $1" ; echo "#") >>$CONFIG
    (echo "" ; echo "/*"; echo " * $1" ; echo " */") >>$CONFIG_H
}

#
# define_bool sets the value of a boolean argument
#
#   define_bool define value
#
function define_bool () {
    case "$2" in
     "y")
        echo "set($1 y)" >>$CONFIG
        echo "#define $1 1" >>$CONFIG_H
        ;;

     "m")
        echo "set($1 m)" >>$CONFIG
        echo "#undef  $1" >>$CONFIG_H
        echo "#define $1_MODULE 1" >>$CONFIG_H
        ;;

     "n")
        echo "# $1 is not set" >>$CONFIG
        echo "#undef  $1" >>$CONFIG_H
        ;;
    esac
    eval "$1=$2"
}

#
# bool processes a boolean argument
#
#   bool question define
#
function bool () {
    old=$(eval echo "\${$2}")
    def=${3}
    if [ -z ${def} ]; then
        def=n
    fi
    case "$def" in
     "y" | "m") defprompt="Y/n/?"
          def="y"
          ;;
     "n") defprompt="N/y/?"
          ;;
    esac
    while :; do
      readln "$1 ($2) [$defprompt] " "$def" "$old"
      case "$ans" in
        [yY] | [yY]es ) define_bool "$2" "y"
                break;;
        [nN] | [nN]o )  define_bool "$2" "n"
                break;;
        * )             help "$2"
                ;;
      esac
    done
}

#
# tristate processes a tristate argument
#
#   tristate question define
#
function tristate () {
    if [ "$CONFIG_MODULES" != "y" ]; then
      bool "$1" "$2"
    else 
      old=$(eval echo "\${$2}")
      def=${old:-'n'}
      case "$def" in
       "y") defprompt="Y/m/n/?"
        ;;
       "m") defprompt="M/n/y/?"
        ;;
       "n") defprompt="N/y/m/?"
        ;;
      esac
      while :; do
        readln "$1 ($2) [$defprompt] " "$def" "$old"
        case "$ans" in
          [yY] | [yY]es ) define_bool "$2" "y"
                  break ;;
          [nN] | [nN]o )  define_bool "$2" "n"
                  break ;;
          [mM] )          define_bool "$2" "m"
                  break ;;
          * )             help "$2"
                  ;;
        esac
      done
    fi
}

#
# dep_tristate processes a tristate argument that depends upon
# another option.  If the option we depend upon is a module,
# then the only allowable options are M or N.  If Y, then
# this is a normal tristate.  This is used in cases where modules
# are nested, and one module requires the presence of something
# else in the kernel.
#
#   tristate question define default
#
function dep_tristate () {
    old=$(eval echo "\${$2}")
    def=${old:-'n'}
    if [ "$3" = "n" ]; then
        define_bool "$2" "n"
    elif [ "$3" = "y" ]; then
        tristate "$1" "$2"
    else
       if [ "$CONFIG_MODULES" = "y" ]; then
        case "$def" in
         "y" | "m") defprompt="M/n/?"
              def="m"
              ;;
         "n") defprompt="N/m/?"
              ;;
        esac
        while :; do
          readln "$1 ($2) [$defprompt] " "$def" "$old"
          case "$ans" in
              [nN] | [nN]o )  define_bool "$2" "n"
                      break ;;
              [mM] )          define_bool "$2" "m"
                      break ;;
              [yY] | [yY]es ) echo 
   echo "  This answer is not allowed, because it is not consistent with"
   echo "  your other choices."
   echo "  This driver depends on another one which you chose to compile"
   echo "  as a module. This means that you can either compile this one"
   echo "  as a module as well (with M) or leave it out altogether (N)."
                      echo
                      ;;
              * )             help "$2"
                      ;;
          esac
        done
       fi
    fi
}

#
# define_int sets the value of a integer argument
#
#   define_int define value
#
function define_int () {
    echo "set($1 $2)" >>$CONFIG
    echo "#define $1 ($2)" >>$CONFIG_H
    eval "$1=$2"
}

#
# int processes an integer argument
#
#   int question define default
# GNU expr changed handling of ?.  In older versions you need ?,
# in newer you need \?
OLD_EXPR=`expr "0" : '0\?'`
if [ $OLD_EXPR -eq 1 ]; then
    INT_EXPR='0$\|-\?[1-9][0-9]*$'
else
    INT_EXPR='0$\|-?[1-9][0-9]*$'
fi
function int () {
    old=$(eval echo "\${$2}")
    def=${old:-$3}
    while :; do
      readln "$1 ($2) [$def] " "$def" "$old"
      if expr "$ans" : $INT_EXPR > /dev/null; then
        define_int "$2" "$ans"
        break
      else
        help "$2"
      fi
    done
}
#
# define_hex sets the value of a hexadecimal argument
#
#   define_hex define value
#
function define_hex () {
    echo "set($1 $2)" >>$CONFIG
    echo "#define $1 0x${2#*[x,X]}" >>$CONFIG_H
    eval "$1=$2"
}

#
# hex processes an hexadecimal argument
#
#   hex question define default
#
function hex () {
    old=$(eval echo "\${$2}")
    def=${old:-$3}
    def=${def#*[x,X]}
    while :; do
      readln "$1 ($2) [$def] " "$def" "$old"
      ans=${ans#*[x,X]}
      if expr "$ans" : '[0-9a-fA-F][0-9a-fA-F]*$' > /dev/null; then
        define_hex "$2" "$ans"
        break
      else
        help "$2"
      fi
    done
}

#
# choice processes a choice list (1-out-of-n)
#
#   choice question choice-list default
#
# The choice list has a syntax of:
#   NAME WHITESPACE VALUE { WHITESPACE NAME WHITESPACE VALUE }
# The user may enter any unique prefix of one of the NAMEs and
# choice will define VALUE as if it were a boolean option.
# VALUE must be in all uppercase.  Normally, VALUE is of the
# form CONFIG_<something>.  Thus, if the user selects <something>,
# the CPP symbol CONFIG_<something> will be defined and the
# shell variable CONFIG_<something> will be set to "y".
#
function choice () {
    question="$1"
    choices="$2"
    old=
    def=$3

    # determine default answer:
    names=""
    set -- $choices
    firstvar=$2
    while [ -n "$2" ]; do
        if [ -n "$names" ]; then
            names="$names, $1"
        else
            names="$1"
        fi
        if [ "$(eval echo \"\${$2}\")" = "y" ]; then
            old=$1
            def=$1
        fi
        shift; shift
    done

    val=""
    while [ -z "$val" ]; do
        ambg=n
        readln "$question ($names) [$def] " "$def" "$old"
        ans=$(echo $ans | tr a-z A-Z)
        set -- $choices
        while [ -n "$1" ]; do
            name=$(echo $1 | tr a-z A-Z)
            case "$name" in
                "$ans"* )
                    if [ "$name" = "$ans" ]; then
                        val="$2"
                        break   # stop on exact match
                    fi
                    if [ -n "$val" ]; then
                        echo;echo \
        "  Sorry, \"$ans\" is ambiguous; please enter a longer string."
                        echo
                        val=""
                        ambg=y
                        break
                    else
                        val="$2"
                    fi;;
            esac
            shift; shift
        done
        if [ "$val" = "" -a "$ambg" = "n" ]; then
            help "$firstvar"
        fi
    done
    set -- $choices
    while [ -n "$2" ]; do
        if [ "$2" = "$val" ]; then
            # echo "  defined $val"
            define_bool "$2" "y"
        else
            define_bool "$2" "n"
        fi
        shift; shift
    done
}

CONFIG=config
CONFIG_H=include/kernel/config.h
trap "rm -f $CONFIG $CONFIG_H ; exit 1" 1 2

#
# Make sure we start out with a clean slate.
#
echo "#" > $CONFIG
echo "# Automatically generated make config: don't edit" >> $CONFIG
echo "#" >> $CONFIG

echo "/*" > $CONFIG_H
echo " * Automatically generated C config: don't edit" >> $CONFIG_H
echo " */" >> $CONFIG_H
echo "#define AUTOCONF_INCLUDED" >> $CONFIG_H

DEFAULT=""
if [ "$1" = "-d" ] ; then
    DEFAULT="-d"
    shift
fi

echo 
echo "Configuring script for system"
echo

. $CONFIG_IN scripts/main_config.in

echo
echo "System is now hopefully configured and is ready for building. "
echo "To be sure that it will be build correctly, run 'make clean'."
echo

exit 0