%define name rat
%define version 4.2.23
%define release ipv6

Name: %{name}
Summary: UCL Robust Audio Tool
Version: %{version}
Release: %{release}
Group: Applications/Internet
Copyright: Copyright (c) 1995-2000 University College London
URL: http://www-mice.cs.ucl.ac.uk/multimedia/software/%{name}/
Source: http://www-mice.cs.ucl.ac.uk/multimedia/software/%{name}/releases/%{version}/%{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-root

%description
RAT is the premier open source voice-over-IP
application. It allows users to particpate in audio
conferences over the internet. These can be between
two participants directly, or between a group of
participants on a common multicast group. No special
features are required to use RAT in point-to-point
mode, but to use the multiparty conferencing facilities
of RAT, all participants must reside on a portion of the
Internet which supports IP multicast. RAT is based on
IETF standards, using RTP above UDP/IP as its
transport protocol, and conforming to the RTP profile
for audio and video conference with minimal control. 

RAT features sender based loss mitigation mechanisms
and receiver based audio repair techniques to compensate 
for packet loss, and load adaption in response to host 
performance. It runs on a range of platforms: FreeBSD, 
HP-UX, IRIX, Linux, NetBSD, Solaris, SunOS, and Windows 
95/NT. The source code is publicly available for porting 
to other platforms and for modification by others. 

Note that RAT does not perform call services like user 
location, neither does it listen to session announcements 
to discover advertised multicast sessions. For these 
purposes, it is recommended you use RAT in conjunction 
with the Session Directory (SDR), or a similar application. 

See http://www-mice.cs.ucl.ac.uk/multimedia/software/rat

%prep
%setup -q

%build
cd tcl-8.0/unix
%configure
make
cd ../../tk-8.0/unix
%configure
make
cd ../../common
%configure --enable-ipv6
make
cd ../rat
%configure --sysconfdir=/etc --mandir=%{_mandir} --enable-ipv6
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT{%{_prefix}{/bin,/sbin,/lib},%{_mandir}/man1}
mkdir -p $RPM_BUILD_ROOT%{_prefix}/local/etc/sdr/plugins
cd rat
%makeinstall prefix=$RPM_BUILD_ROOT%{_prefix}/local
cd $RPM_BUILD_ROOT%{_bindir}
ln -sf %{name}-%{version} %{name}

%clean
rm -rf $RPM_BUILD_ROOT


%files 
%defattr(-,root,root)
%doc %{name}/README %{name}/README.devices %{name}/README.files
%doc %{name}/README.gsm %{name}/README.mbus %{name}/README.playout
%doc %{name}/README.timestamps %{name}/COPYRIGHT
/usr/local/etc/sdr/plugins/*
%{_prefix}/bin/*
%{_mandir}/*/*

%changelog
* Mon Mar 24 2003 Kristian Hasler <k.hasler@cs.ucl.ac.uk>
- ipv6 build is default
- RAT SDR plugin is installed in /usr/local/etc/sdr/plugins
- now uses makefile to install files

* Tue Jan 04 2000 Colin Perkins <c.perkins@cs.ucl.ac.uk>
- initial build

