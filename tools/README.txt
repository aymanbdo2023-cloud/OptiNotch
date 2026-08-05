OptiNotch
=========

A "Dynamic Island" style bar at the top of your screen: a clock that expands
on hover into your calendar and currently-playing media.

Running
-------
Double-click OptiNotch.exe. Nothing else needs installing. The app is
single-instance-ish: it keeps running from the tray icon; use the tray menu to
hide/show it, open settings, toggle Start-with-Windows, or quit.

Hover the notch to expand it. Win+Alt slides the notch away (toggle the hotkey in settings).

Google Calendar (optional)
-------------------------
The app ships with built-in Google credentials, so all you need is a Google
account. Hover the notch, click "Connect Google Calendar" on the calendar
side, and sign in with the account you want to use. That's it.

To switch to a different Google account later: open the gear (top-right of
the expanded notch) -> Settings -> "Sign out", then press "Connect Google
Calendar" in the notch again and pick the other account.

(The first-run setup wizard with "Load client JSON..." only appears if the
app was built without bundled credentials.)

Settings
--------
The gear icon (top-right of the expanded notch) or the tray menu opens
settings: monitor, horizontal offset, hide hotkey, Start with Windows, accent
color, and opacity. Settings are saved to
%APPDATA%\OptiNotch\settings.json.

Troubleshooting
---------------
- If the clock bar never shows, an antivirus may be blocking the exe; add an
  exception.
- Fonts ship in the assets/ folder next to the exe - keep them together.
