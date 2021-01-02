Name:      asterisk-addons
Version:   1.4.18
#Ne pas enlever le .ives a la fin de la release !
#Cela est utilise par les scripts de recherche de package.
Release:   1.ives%{dist}
Summary:   Asterisk Addons
Vendor:    IVeS
Group:     Utilities/System
License: GPL
URL:       http://www.ives.fr
Source0: http://svn.digium.com/svn/asterisk-addons/tags/1.4.12
BuildArchitectures: x86_64
BuildRoot:  %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires:  ivespkg, asteriskv >= 1.4.28

%description
Additional application for Asterisk PBX

%clean
echo "############################# Clean"
cd $RPM_SOURCE_DIR/%name
make clean
cd ..
rm -f %name
echo Clean du repertoire $RPM_BUILD_ROOT
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf "$RPM_BUILD_ROOT"

%prep
cd $RPM_SOURCE_DIR/%name
%configure --with-mysqlclient

%build
echo "############################# Build"
echo $PWD
cd $RPM_SOURCE_DIR/%name
#make menuselect // use default 
make

%install
echo "Install" $PWD
cd $RPM_SOURCE_DIR/%name
make DESTDIR=$RPM_BUILD_ROOT install samples

%files
%defattr(-,root,root,-)
%config(noreplace) /etc/asterisk/cdr_mysql.conf
%config(noreplace) /etc/asterisk/mysql.conf
%config(noreplace) /etc/asterisk/ooh323.conf
%config(noreplace) /etc/asterisk/res_mysql.conf

%{_libdir}/asterisk/modules/app_addon_sql_mysql.so
%{_libdir}/asterisk/modules/app_saycountpl.so
%{_libdir}/asterisk/modules/cdr_addon_mysql.so
%{_libdir}/asterisk/modules/chan_ooh323.so
%{_libdir}/asterisk/modules/format_mp3.so
%{_libdir}/asterisk/modules/res_config_mysql.so

%changelog
* Tue Jul 21 2020 Emmanuel BUU <emmanuel.buu@ives.fr>
- res_config_mysql now returns an error code to have a better idea of DB errors
- version 1.4.17

* Tue May 26 2020 Emmanuel BUU <emmanuel.buu@ives.fr>
- wait 100ms for connection lock to avoid errors in load situation
- migrated to mariaDB client libs
- version 1.4.15

* Fri May 22 2020 Emmanuel BUU <emmanuel.buu@ives.fr>
- added mysql_close to avoid connection leak
- version 1.4.14

* Mon Mar 19 2018 Emmanuel BUU <emmanuel.buu@ives.fr>
- added connect timeout of 15 s to avoid deadlock
- added deadlock avoidance on realtime func
- version 1.4.13

* Thu Dec 08 2016 Emmanuel BUU <emmanuel.buu@ives.fr>
- relaxed dependency with asterisk 1.4.20 and above
- version 1.4.12.33

* Wed Jul 08 2015 Thomas Carvello  <thomas.carvello@ives.fr>
- Version 1.4.12.27
- no level limit for H323 call
* Fri Nov 21 2014 Philippe Verney  <philippe.verney@ives.fr>
- Version 1.4.12.26
- Fix reinvite and limit level @ 22
* Tue Mar 25 2014 Philippe Verney  <philippe.verney@ives.fr>
- Version 1.4.12.24
- Fix support of url-ID.
* Tue Mar 18 2014 Philippe Verney  <philippe.verney@ives.fr>
- Version 1.4.12.23
- Add support of url-ID.
* Thu May 23 2013 Philippe Verney  <philippe.verney@ives.fr>
- Version 1.4.12.22
- Add support of FlowCommandControl.
- Fix H624 profile and level negociation ( ITU H241 / H264  ) .
* Wed May 22 2013 Philippe Verney  <philippe.verney@ives.fr>
- Version 1.4.12.21
- Add  Conference request / enterH243TerminalID
* Tue May 21 2013 Philippe Verney  <philippe.verney@ives.fr>
- Version 1.4.12.20
- Add support of miscellaneousIndication logicalChannelActive
* Thu May 02 2013 Philippe Verney  <philippe.verney@ives.fr>
- Version 1.4.12.19
- fix video bitrate nego.
* Mon Apr 29 2013  Philippe Verney  <philippe.verney@ives.fr>
- Version 1.4.12.18
- fix h323 stack log
* Fri Apr 19 2013  Philippe Verney  <philippe.verney@ives.fr>
- Version 1.4.12.17
- fix master slave determination by modulo
- fix MaxBitRate nego
* Thu Apr 11 2013 Philippe Verney  <philippe.verney@ives.fr>
- Version 1.4.12.16
- fix crash on sendH245 Message if H245 not established
* Thu Apr 11 2013 Philippe Verney  <philippe.verney@ives.fr>
- Version 1.4.12.15
- fix nat after first rtp
- fix open rtp stream
- fix codec nego
- fix msd / olc nego
- obsolete waitOLC  / sendMSD
- new config param:termtype=[50 - 240] value of termtype recommanded  Terminal=50 Gateway=90 Gatekeeper=130 MCU=190 default=50
- fix dtmf mode , by default rfc1833 / inband / Q931 / alphanumeric / signal is active
- fix onCallEstablished after OpenLogicalChannel
- fix answer
- config mode not mandatory
- fix close RP video
- fix connect timeout
* Thu Apr 04 2013 Philippe Verney  <philippe.verney@ives.fr>
- Version 1.4.12.13
- Send TCS on connect
* Mon Feb 25 2013 Philippe Verney  <philippe.verney@ives.fr>
- Version 1.4.12.12
- Fix tunneling / PublicIP on H245 if present 
* Fri Jan 11 2013 Philippe Verney  <philippe.verney@ives.fr>
- Version 1.4.12.10
- Fix Polycom Transfer Mode circuit /packet mode
- Fix configuration Faststart / tunnelling / Gatekeeper on make call 
- Add User Name on H323-id for outgoing call 
- Add addVp200Info=yes/no on config 
- Add BearerCapabilityCircuitMode=yes/no Transfer Mode is circuit on setup (default yes) no = transfert mode in packet mode 
- Add H323-ID on incomming call if dialedDigits or e164 number is not present on caller number
* Wed Dec 12 2012 Philippe Verney  <philippe.verney@ives.fr>
- Version 1.4.12.9
- Fix crash outgoing call with Polycom 
- add IpPublic on video OLC 
* Mon Nov 12 2012 Philippe Verney  <philippe.verney@ives.fr>
- Version 1.4.12.8
- Ajout trace socket ( open / send /recv / shutdown / close )
* Mon Nov 12 2012 Philippe Verney  <philippe.verney@ives.fr>
- Version 1.4.12.7
- Add trace on Failed to convert address
* Fri Nov 09 2012 Philippe Verney  <philippe.verney@ives.fr>
- Version 1.4.12.6
- Add Nat rtp option : publicIp 
- Add trace on open close socket ( ooSocketClose close socket / ooSocketBind socket ) 
* Wed May 23 2012 Philippe Verney  <philippe.verney@ives.fr>
- Fix callForward
* Tue Sep 27 2011 Philippe FAVIER <philippe.favier@ives.fr>
- add option to optionally keep SSRC constant or not (config param in ooh323.cfg)
- add check on callerid_num to avoid seg violation with strdup(NULL)
* Mon Sep 19 2011 Philippe FAVIER <philippe.favier@ives.fr>
- character filtering in callingPartyNumber, calledPartyNumber, ooCallAddAlias() to avoid problems in SIP messages
* Mon Sep 19 2011 Philippe FAVIER <philippe.favier@ives.fr>
- set username to callerid_num if user not found in user list
* Wed May 18 2011 Emmanuel BUU <emmanuel.buu@ives.fr>
- support for SSRC change added
* Tue Jan 11 2011 Emmanuel BUU <emmanuel.buu@ives.fr>
- dependency with asterskv 1.4.19r
* Tue Jan 11 2011 Emmanuel BUU <emmanuel.buu@ives.fr>
- migrated to addons 1.4.12
- activated mechanism to reorder RTP packet on video connections for ooh323 channel
* Fri Sep 24 2010 Philippe Verney <philippe.verney@ives.fr>
- add ip on ast ctx
* Tue Oct 27 2009 Emmanuel BUU <emmanuel.buu@ives.fr>
- Added custom chan_ooh323 with video support
- switched to asterisk-addons 1.4.9
* Fri Apr 17 2009 Eric Delas <eric.delas@ives.fr> 1.4.7-2.ives
- Package for 1.4.19f-1.ives asterisk version
* Thu Apr 02 2009 Emmanuel BUU <emmanuel.buu@ives.fr> 0.1.1-1.ives
- package from local source tree. x86_64 package version

