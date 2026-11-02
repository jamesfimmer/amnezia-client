# Brand assets

Replace these files to change the look of the app. Keep the same names and
formats; the build copies them from `client/images/`.

| File | Purpose |
| --- | --- |
| `client/images/app.ico` | Windows executable and installer icon |
| `client/images/icon.png` | Window and tray icon |
| `client/images/AmneziaVPN.png` | Full logo used on the welcome screens |
| `client/images/amneziaLogo.svg` | Small logo |
| `client/images/AmneziaVPN_Full_logo.svg` | Full wordmark |
| `client/images/tray/*.png` | Tray icons (default, active, error) |

Colours live in `client/ui/qml/Modules/Style/AmneziaStyle.qml`. Change the
`accent*` tokens first: they drive buttons, highlights and the connection
state.

Text strings that mention the old name are spread over `client/ui/qml`. The
main window title already follows the CMake application name.
