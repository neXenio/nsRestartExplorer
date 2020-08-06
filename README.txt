NSIS Plugin to gracefully Restart Explorer

Usage:

[NSIS]:
- put the plugin (dll) in NSIS Plugin directory ({NSIS dir}/Plugins/x86-unicode)
- in your setup source add:
  nsRestartExplorer::nsRestartExplorer
- for getting the error/success message add in the next line:
  Pop $1
  DetailPrint $1

[notes]
- Please look at the example nsRestart.nsi
- this is inspired by the origin nsRestartExplorer: https://github.com/sherpya/nsRestartExplorer
- I've tested and build only the Unicode version and i guess it will not worked as ANSI Version (but nowadays noboy should use ANSI anymore)
- If somebody needs the ANSI Version, he can adapt the code very easy. For example just replace methods like Process32NextW with Process32Next or Process32NextA.
- It only works for Windows Vista, 7, 8, 10, because it uses the Windows RestartManager.
- Their are some heuristic implemented to avoid that the reopened File Explorer windows overlap the installer GUI.
  - set all reopen windows to not active
  - Wait till all File Explorer windows which was open before the restart are open again or stop waiting after 10s when no File Explorer window opens or stop waiting after 1s after the last File Explorer window was open. Too avoid a theoretically possible infinity loop in case users are reopen and close windows at the same time very fast, we stop waiting after 20s. 
