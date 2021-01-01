#!/bin/ksh
# =============================================================================
# Unpublished Confidential Information of IVES Do not disclose.       
# Copyright (c)  IVES All Rights Reserved.                    
# ---------------------------------------------------------------------------
#
# COMPANY   IVES
#
# author   Philippe Verney
#
# file     $HeadURL: http://svn.ives.fr/svn-libs-dev/asterisk/asterisk-addons-rpm/trunk/channels/check_port.ksh $
#
# brief    Check TCP port state
#               
# version  $Revision: 3532 $
#
# date     $Date: 2012-11-13 07:06:22 +0100 (Tue, 13 Nov 2012) $
# 
# remarks  
# 
#--------------------------------------------------------------------------- 
# $Log$
# =============================================================================
#  -----1=0-------2=0-------3=0-------4=0-------5=0-------6=0-------7=0-------8

# =============================================================================
# Constant 
# =============================================================================
integer gPort=1720
netstatCmd="/bin/netstat"
socketStatOK="LISTEN"
socketStatKO="CLOSE_WAIT"
integer gCriticalNumber=10 
EXIT_SUCCESS=0
EXIT_ERROR=1
echo_on_stdout=1

# =============================================================================
# Affichage
# =============================================================================

# Vert
PrintOK()
{
    if [ $echo_on_stdout -eq 1 ] 
        then 
        printf "[\033[32m  OK  \033[0m]\n"
    fi
}

# Rouge
PrintFailed()
{
    if [ $echo_on_stdout -eq 1 ] 
        then 
        printf "[\033[31mFAILED\033[0m]\n"
        if [ $debug -ne 0 ]
            then
            printf "=============== last log ================\n"
            tail -n 25 $LOG_FILE
            printf "=========================================\n"
        fi
    fi
}

# jaune
PrintWarning()
{
    if [ $echo_on_stdout -eq 1 ] 
        then 
        printf "[\033[33m INFO \033[0m]\n"
    fi
}



printLine()
{
    if [ $echo_on_stdout -eq 1 ] 
        then 
        RES_COL=60
        printf "$1"
        printf "\033[%sG" $RES_COL
    fi
}



start_line()
{
    if [ $echo_on_stdout -eq 1 ] 
        then 
        printf "Convert $inFile to $outFile\n"
    fi
}

usage()
{
    printf "\033[1mUsage \033[0m\n"
    printf "\033[1mNAME\033[0m\n"
    printf "\t $0 Check if TCP port $gPort is in WAIT_CLOSE state for \n"
    printf "\033[1mSYNOPSIS\033[0m\n"
    printf "\t $0  <-p port (default $gPort) >"
    printf "\t $0 COMMANDS\n"
    printf "\033[1mDESCRIPTION\033[0m\n"
    printf "\t\033[1m -p port \033[0m  TCP port state\n"
    printf "\t\033[1m Version :  $Revision: 3532 $  \033[0m \n"
}

# =============================================================================
# Affichage
# =============================================================================
checkTcpPort()
{
    printLine "Check if H.323 gateway is lock on socket : "
    nOK=`$netstatCmd -an | egrep "$gPort" | egrep -v "$socketStatOK" | wc | awk '{print $1 }'`
    nKO=`$netstatCmd -an | egrep "$gPort" | egrep "$socketStatKO" | wc | awk '{print $1 }'`
    if [ $nKO -ge 10 ] 
        then 
        PrintFailed
        exit $EXIT_ERROR
        else 
          if [ $nOK -eq 0 ] 
              then  PrintOK
          else 
              PrintWarning
              $netstatCmd -an | egrep "$gPort" | egrep -v "$socketStatOK" 
          fi
          exit $EXIT_SUCCESS
    fi
}




Execut()
{
 checkTcpPort
}

# =============================================================================
# main : parse args and exeute 
# =============================================================================

while [ "$1" ] 
  do    
  case "$1" in
      -p)
      shift
      gPort=$1
      ;;
      -h|help)
      usage
      exit $EXIT_SUCCESS
      ;;
      *)
      echo "Unknown: $1"
      usage
      exit $EXIT_ERROR
      ;;
  esac
  shift
done
Execut
