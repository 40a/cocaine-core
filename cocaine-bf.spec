%define cocaine_runtime_name cocaine-runtime

Summary:	Cocaine - Core Libraries
Name:		libcocaine-core3
Version:	0.12.0.9
Release:	1%{?dist}


License:	GPLv2+
Group:		System Environment/Libraries
URL:		http://www.github.com/cocaine
Source0:	%{name}-%{version}.tar.bz2
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires: boost-devel, boost-iostreams, boost-thread, boost-system
BuildRequires: libmhash-devel, libtool-ltdl-devel, libuuid-devel, libcgroup-devel
BuildRequires: msgpack-devel, libarchive-devel, binutils-devel

%if %{defined rhel} && 0%{?rhel} < 7
BuildRequires: cmake28
%else
BuildRequires: cmake
%endif

Obsoletes: srw

%description
Cocaine is an open application cloud platform.

%package devel
Summary: Development files for %{name}
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}
Requires: boost-devel, libtool-ltdl-devel, msgpack-devel

%description devel
Cocaine development headers package.

%package -n %{cocaine_runtime_name}
Summary:	Cocaine - Runtime
Group:		Development/Libraries

%description -n %{cocaine_runtime_name}
Cocaine runtime components package.

%prep
%setup -q

%build
%if %{defined rhel}
%if 0%{?rhel} == 6
%{cmake28} -DCOCAINE_LIBDIR=%{_libdir} .
%endif
%if 0%{?rhel} > 6
%{cmake} -DCOCAINE_LIBDIR=%{_libdir} .
%endif
%else
%{cmake} -DCOCAINE_LIBDIR=%{_libdir} .
%endif

make %{?_smp_mflags}

%install
rm -rf %{buildroot}

make install DESTDIR=%{buildroot}

rm -f %{buildroot}%{_libdir}/*.a
rm -f %{buildroot}%{_libdir}/*.la

%if 0%{?fedora} >= 19
# Install systemd unit
install -p -D -m 644 fedora/cocaine-runtime.service %{buildroot}/%{_unitdir}/%{cocaine_runtime_name}.service
%else
install -dD %{buildroot}%{_sysconfdir}/init.d/
install -m 755 debian/cocaine-runtime.init %{buildroot}%{_sysconfdir}/init.d/%{cocaine_runtime_name}
%endif

install -d -m 755 %{buildroot}%{_localstatedir}/run/cocaine

%if 0%{?fedora} >= 19
mkdir -p %{buildroot}%{_tmpfilesdir}
# Install systemd tmpfiles config
install -p -D -m 644 fedora/cocaine-runtime.tmpfiles %{buildroot}%{_tmpfilesdir}/%{cocaine_runtime_name}.conf
%endif

install -d %{buildroot}%{_sysconfdir}/cocaine
install -m644 debian/cocaine-runtime.conf %{buildroot}%{_sysconfdir}/cocaine/cocaine-default.conf

%post -p /sbin/ldconfig

%post -n %{cocaine_runtime_name}
%if 0%{?fedora} >= 19
if [ $1 -eq 1 ] ; then
    /bin/systemctl daemon-reload >/dev/null 2>&1 || :
fi
%endif

%postun -p /sbin/ldconfig

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%doc README.md
%{_libdir}/*.so.*

%files devel
%defattr(-,root,root,-)
%{_includedir}/*
%{_libdir}/libcocaine-core.so

%files -n %{cocaine_runtime_name}
%defattr(-,root,root,-)
%{_bindir}/cocaine-runtime

%if 0%{?fedora} >= 19
%{_tmpfilesdir}/%{cocaine_runtime_name}.conf
%{_unitdir}/%{cocaine_runtime_name}.service
%attr(0775,root,zabbix) %dir %{_localstatedir}/run/cocaine
%else
%{_sysconfdir}/init.d/*
%endif
%{_sysconfdir}/cocaine/cocaine-default.conf

%changelog
* Wed Apr 22 2015 Andrey Sibiryov <me@kobology.ru> 0.12.0.9-1
- Native external services support. Standalone native services can now
  attach themselves to the Locators to be announced.
- 'Bootstrap is complete' core signal. Services can listen for this signal
  to hook into bootstrapping process.
- Blackhole v0.5.
- Bugfix: wrong lock held for certain Locator operations.
- Bugfix: routing streams are now also closed on Locator shutdown.
- Some optimizations and cleanup.

* Fri Apr 17 2015 Evgeny Safronov <division494@gmail.com> 0.12.0.8-1
- Bugfix: fixed app vs. published services list inconsistency.

* Wed Apr 08 2015 Andrey Sibiryov <me@kobology.ru> 0.12.0.7-1
- Remote connections are now retried on failure.
- Bugfix: endpoints while connecting remotes were corrupted in logs.

* Tue Apr 07 2015 Andrey Sibiryov <me@kobology.ru> 0.12.0.6-1
- External service support. Non-native services can now be exposed via the
  Cocaine service discovery mechanics, including gateway setup and stuff.
- External routers can now hook up into RG path in Locator and sync all the
  routing group changes in realtime.
- Bugfix: multiple versions of the same service in the cluster won't render
  such services unusable anymore.

* Fri Apr 03 2015 Andrey Sibiryov <me@kobology.ru> 0.12.0.5-1
- Bugfix: sometimes new Sessions weren't properly registered with Engines.

* Fri Apr 03 2015 Andrey Sibiryov <me@kobology.ru> 0.12.0.4-1
- Multiple thread-safety fixes for Actor, Locator and others.
- Refactored Sessions to enable verbose error logging, at last.
- Reverted setting of TCP_NODELAY option on client sockets.

* Fri Mar 20 2015 Evgeny Safronov <division494@gmail.com> 0.12.0.3-1
- Bugfix: do not read/write from/to already deleted channel.

* Wed Mar 18 2015 Anton Tiurin <noxiouz@yandex.ru> 0.12.0.2-1
- Bugfix: pass a valid local hostname in --locator arg for slaves.

* Tue Mar 17 2015 Andrey Sibiryov <me@kobology.ru> 0.12.0.1-1
- Release 0.12.

