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
    echo "%_topdir" "${PWD}/rpmbuild" >> ~/.rpmmacros
    echo "%_tmppath %{_topdir}/TMP" >> ~/.rpmmacros
    echo "%_signature gpg" >> ~/.rpmmacros
    echo "%_gpg_name IVeSkey" >> ~/.rpmmacros
    echo "%_gpg_path" "${PWD}/rpmbuild/gnupg" >> ~/.rpmmacros
    echo "%vendor IVeS" >> ~/.rpmmacros
    #Import de la clef gpg IVeS
    mkdir -p rpmbuild/gnupg
    if [[ -z $1 || $1 -ne nosign ]]
    then
	cd rpmbuild/gnupg
	svn export http://svn.ives.fr/svn-libs-dev/gnupg
	cd -
    fi
    mkdir -p rpmbuild/SOURCES
    mkdir -p rpmbuild/SPECS
    mkdir -p rpmbuild/BUILD
    mkdir -p rpmbuild/SRPMS
    mkdir -p rpmbuild/TMP
    mkdir -p rpmbuild/RPMS
    mkdir -p rpmbuild/RPMS/noarch
    mkdir -p rpmbuild/RPMS/i386
    mkdir -p rpmbuild/RPMS/i686
    mkdir -p rpmbuild/RPMS/i586

    #Recuperation de la description du package 
    ln -s $PWD rpmbuild/SOURCES/${PROJET}
    cp ${PROJET}.spec rpmbuild/SPECS/${PROJET}.spec
    mkdir rpmbuild/SOURCES/configs/
    cp -p configs/ooh323.conf.sample rpmbuild/SOURCES/configs/
    
    #Cree le package
    if [[ -z $1 || $1 -ne nosign ]]
        then rpmbuild -bb --define "asterisk_dep ${ASTERISK_DEP}" --sign rpmbuild/SPECS/${PROJET}.spec
        else rpmbuild -bb --define "asterisk_dep ${ASTERISK_DEP}" rpmbuild/SPECS/${PROJET}.spec
    fi
 
    echo "************************* fin du rpmbuild ****************************"
    #Recuperation du rpm
#    mv -f $HOME/rpmbuild/RPMS/i386/*.rpm $PWD/.
    mv -f rpmbuild/RPMS/x86_64/*.rpm $PWD/.
    clean
}

function clean
{
  	# On efface les liens ainsi que le package precedemment créé
  	echo Effacement des fichiers et liens gnupg rpmbuild ${PROJET}.rpm ${TEMPDIR}/${PROJET}
  	rm -rf rpmbuild/SPECS/${PROJET}.spec rpmbuild/gnupg
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
