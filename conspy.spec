Summary:	Remote control for text mode virtual consoles
Name:		conspy
Version:	1.14
Release:	1ras
Distribution:	RedHat 7.2 Contrib
Group:		Applications/System
License:	AGPL-3.0+
Packager:	Russell Stuart <russell-rpm@stuart.id.au>
Vendor:		Russell Stuart <russell-conspy@stuart.id.au>
Url:		http://sourceforge.net/projects/%{name}/files/%{name}-%{version}/%{name}-%{version}.tar.gz
Source:		%{name}-%{version}.tar.gz
BuildRoot:	%{_tmppath}/%{name}-root

%description
Conspy takes over a text mode Linux virtual console in much
the same manner as VNC allows a remote user to take over a GUI.

%prep
%setup

%build
aclocal
automake --foreign --add-missing --copy
autoconf
%configure
make

%install
rm -rf %{buildroot}
%makeinstall

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%{_bindir}/conspy
%doc %{_mandir}/man1/*

%changelog
* Thu Jan 19 2006 Russell Stuart <russell-rpm@stuart.id.au>
- New upstream release.
* Mon Jan  9 2006 Russell Stuart <russell-rpm@stuart.id.au>
- New upstream release.
* Sun Jan  1 2006 Russell Stuart <russell-rpm@stuart.id.au>
- New upstream release.
* Wed May 21 2003 Russell Stuart <russell-rpm@stuart.id.au>
- Initial RPM
