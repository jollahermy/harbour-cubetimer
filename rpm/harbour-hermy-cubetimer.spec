# Harbour RPM for Cube Timer (user-space rpmbuild on the Sailfish device).
#
# Build model: the binary is compiled in the app repo beforehand (bin/harbour-hermy-cubetimer,
# recipe in the sailfish-device-ops toolchain notes). %build compiles nothing; %install copies
# the binary, the QML/JS/font tree, the desktop entry, the icons and LICENSE from the repo.
# The app repo location can be overridden with --define "app_repo <path>".
#
# The binary is NOT stripped: strip --strip-unneeded removes the exported main() symbol that the
# validator's Symbols check and booster need. Build-time BRP scripts are disabled for the same
# reason (the device has BusyBox xargs, which the BRP scripts cannot use).

%{!?app_repo:%global app_repo %{_topdir}/../../app}
%global debug_package %{nil}
%global __os_install_post %{nil}

Name:       harbour-hermy-cubetimer
Summary:    Rubik's cube speedsolving timer
Version:    1.8
Release:    1
License:    GPLv3
Group:      Qt/Qt

Requires:   sailfishsilica-qt5

%description
Rubik's cube speedsolving timer with scramble generation and session stats.

%prep
# nothing to unpack; the app repo is the input

%build
# intentionally empty: the binary is prebuilt in the app repo (see header)

%install
rm -rf %{buildroot}
APP=%{app_repo}

# binary (unstripped on purpose)
install -D -m 0755 $APP/bin/harbour-hermy-cubetimer %{buildroot}%{_bindir}/harbour-hermy-cubetimer

# QML, JS, fonts; backup files excluded
mkdir -p %{buildroot}%{_datadir}/harbour-hermy-cubetimer
cp -a $APP/share/harbour-hermy-cubetimer/. %{buildroot}%{_datadir}/harbour-hermy-cubetimer/
find %{buildroot}%{_datadir}/harbour-hermy-cubetimer -name '*.bak*' -exec rm -f {} \;

# desktop entry
install -D -m 0644 $APP/harbour-hermy-cubetimer.desktop %{buildroot}%{_datadir}/applications/harbour-hermy-cubetimer.desktop

# icons: 86/108/128/172 for the launcher; 256 is loaded by the cover page via
# Qt.resolvedUrl('../../icons/hicolor/256x256/apps/harbour-hermy-cubetimer.png') relative to
# /usr/share/harbour-hermy-cubetimer/qml, i.e. /usr/share/icons/hicolor/256x256/apps
for s in 86x86 108x108 128x128 172x172 256x256; do
    install -D -m 0644 $APP/share/icons/hicolor/$s/apps/harbour-hermy-cubetimer.png \
        %{buildroot}%{_datadir}/icons/hicolor/$s/apps/harbour-hermy-cubetimer.png
done

# licence: inside the app's share dir, the only location besides bin/, desktop and icons that the
# Harbour validator allows (/usr/share/licenses and %doc paths are rejected as 'not allowed')
install -D -m 0644 $APP/LICENSE %{buildroot}%{_datadir}/harbour-hermy-cubetimer/LICENSE

%files
%defattr(-,root,root,-)
%{_bindir}/harbour-hermy-cubetimer
%{_datadir}/harbour-hermy-cubetimer
%{_datadir}/applications/harbour-hermy-cubetimer.desktop
%{_datadir}/icons/hicolor/*/apps/harbour-hermy-cubetimer.png
