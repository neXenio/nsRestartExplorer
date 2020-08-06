Name "nsRestartExplorer"
OutFile "nsRestartExplorer.exe"
ShowInstDetails show

Section "Restart"
    nsRestartExplorer::nsRestartExplorer
    Pop $1
    DetailPrint $1
SectionEnd
