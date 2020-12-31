#!/usr/bin/ksh

PROJET=asterisk-addons
#Repertoire temporaire utiliser pour preparer les packages
TEMPDIR=/tmp


#Preparation du fichier spec de packaging rpm

#Creation de l'environnement de packaging rpm
function create_rpm
{
    if [ ! -x /usr/sbin/asterisk ]; then
	echo "Asterisk pas installe"
	exit 20
    fi
    ASTERISK_DEP=`rpm -q --qf "%{version}" asteriskv`
    echo "Ces appli asterisk sont compiles pour la version " $ASTERISK_DEP

    #Cree l'environnement de creation de package
    #Creation des macros rpmbuild
    rm -f ~/.rpmmacros
    touch ~/.rpmmacros
    echo "%_topdir" $HOME"/rpmbuild" >> ~/.rpmmacros
    echo "%_tmppath %{_topdir}/TMP" >> ~/.rpmmacros
    echo "%_signature gpg" >> ~/.rpmmacros
    echo "%_gpg_name IVeSkey" >> ~/.rpmmacros
    echo "%_gpg_path" $HOME"/rpmbuild/gnupg" >> ~/.rpmmacros
    echo "%vendor IVeS" >> ~/.rpmmacros
    #Import de la clef gpg IVeS
    mkdir -p $HOME/rpmbuild
    cd $HOME/rpmbuild
    if [[ -z $1 || $1 -ne nosign ]]
        then svn export http://svn.ives.fr/svn-libs-dev/gnupg
    fi
    mkdir -p SOURCES
    mkdir -p SPECS
    mkdir -p BUILD
    mkdir -p SRPMS
    mkdir -p TMP
    mkdir -p RPMS
    mkdir -p RPMS/noarch
    mkdir -p RPMS/x86_64
    mkdir -p RPMS/i386
    mkdir -p RPMS/i686
    mkdir -p RPMS/i586
    #Recuperation de la description du package 
    cd -
    cp ${PROJET}.spec $HOME/rpmbuild/SPECS/${PROJET}.spec
    rm -rf $HOME/rpmbuild/SOURCES/channels
    rm -rf $HOME/rpmbuild/SOURCES/res
    cp -rp channels $HOME/rpmbuild/SOURCES
    cp -p configure $HOME/rpmbuild/SOURCES
    cp -rp res $HOME/rpmbuild/SOURCES
    mkdir $HOME/rpmbuild/SOURCES/configs/
    cp -p configs/ooh323.conf.sample $HOME/rpmbuild/SOURCES/configs/
    cd $HOME/rpmbuild/SOURCES
    echo "Telechargement depuis DIGIUM"
    rm -f asterisk-addons*
    #wget http://downloads.digium.com/pub/asterisk/releases/asterisk-addons-1.4.12.tar.gz
    #wget http://download.ives.fr/asterisk/asterisk-addons-1.4.7.tar.gz
    
    #Cree le package
    if [[ -z $1 || $1 -ne nosign ]]
        then rpmbuild -bb --define "asterisk_dep ${ASTERISK_DEP}" --sign $HOME/rpmbuild/SPECS/${PROJET}.spec
        else rpmbuild -bb --define "asterisk_dep ${ASTERISK_DEP}"  $HOME/rpmbuild/SPECS/${PROJET}.spec
    fi
 
    echo "************************* fin du rpmbuild ****************************"
    #Recuperation du rpm
    cd -
    mv -f $HOME/rpmbuild/RPMS/i386/*.rpm $PWD/.
    mv -f $HOME/rpmbuild/RPMS/x86_64/*.rpm $PWD/.
    clean
}

function clean
{
  	# On efface les liens ainsi que le package precedemment créé
  	echo Effacement des fichiers et liens gnupg rpmbuild ${PROJET}.rpm ${TEMPDIR}/${PROJET}
  	rm -rf $HOME/rpmbuild/SPECS/${PROJET}.spec $HOME/rpmbuild/gnupg $HOME/rpmbuild/BUILD/${PROJET}
}

case $1 in
  	"clean")
  		echo "Nettoyage des liens et du package crees par la cible dev"
  		clean ;;
  	"rpm")
  		echo "Creation du rpm"
  		create_rpm $2;;
  	*)
  		echo "usage: install.ksh [options]" 
  		echo "options :"
  		echo "  rpm		Generation d'un package rpm"
  		echo "  clean		Nettoie tous les fichiers cree par le present script, liens, tar.gz et rpm";;
esac
