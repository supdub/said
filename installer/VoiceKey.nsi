Unicode true
RequestExecutionLevel user
SetCompressor /SOLID lzma

!ifndef PROJECT_ROOT
  !define PROJECT_ROOT ".."
!endif
!ifndef DIST_DIR
  !define DIST_DIR "${PROJECT_ROOT}\dist\VoiceKey"
!endif
!ifndef OUTPUT_FILE
  !define OUTPUT_FILE "${PROJECT_ROOT}\dist\VoiceKey-Setup-0.2.0.exe"
!endif

!define PRODUCT_NAME "VoiceKey"
!define PRODUCT_VERSION "0.2.0"
!define PRODUCT_PUBLISHER "VoiceKey"
!define PRODUCT_EXE "voicekey.exe"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\VoiceKey"

Name "${PRODUCT_NAME}"
OutFile "${OUTPUT_FILE}"
InstallDir "$LOCALAPPDATA\Programs\VoiceKey"
InstallDirRegKey HKCU "Software\VoiceKey" "InstallDirectory"
Icon "${PROJECT_ROOT}\assets\voicekey.ico"
UninstallIcon "${PROJECT_ROOT}\assets\voicekey.ico"
BrandingText "Local. Private. Ready."
ShowInstDetails show
ShowUninstDetails show

VIProductVersion "0.2.0.0"
VIAddVersionKey /LANG=1033 "ProductName" "VoiceKey"
VIAddVersionKey /LANG=1033 "CompanyName" "VoiceKey"
VIAddVersionKey /LANG=1033 "FileDescription" "VoiceKey Setup"
VIAddVersionKey /LANG=1033 "FileVersion" "0.2.0"
VIAddVersionKey /LANG=1033 "ProductVersion" "0.2.0"
VIAddVersionKey /LANG=1033 "LegalCopyright" "Copyright (c) 2026 VoiceKey contributors"

!include "MUI2.nsh"
!include "Sections.nsh"

!define MUI_ABORTWARNING
!define MUI_ICON "${PROJECT_ROOT}\assets\voicekey.ico"
!define MUI_UNICON "${PROJECT_ROOT}\assets\voicekey.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "${PROJECT_ROOT}\assets\installer-welcome.bmp"
!define MUI_WELCOMEPAGE_TITLE "VoiceKey, ready when you speak"
!define MUI_WELCOMEPAGE_TEXT "Talk. It types.$\r$\n$\r$\nVoiceKey turns speech into text wherever your caret already is. Recognition stays local on this computer.$\r$\n$\r$\nSetup takes about a minute."
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP "${PROJECT_ROOT}\assets\installer-header.bmp"
!define MUI_FINISHPAGE_RUN "$INSTDIR\${PRODUCT_EXE}"
!define MUI_FINISHPAGE_RUN_PARAMETERS "--onboarding"
!define MUI_FINISHPAGE_RUN_TEXT "Open VoiceKey and finish setup"
!define MUI_FINISHPAGE_TITLE "VoiceKey is ready."
!define MUI_FINISHPAGE_TEXT "VoiceKey and its private speech model are installed. Finish the quick microphone and shortcut setup, then dictate in any text field."
!define MUI_FINISHPAGE_LINK "Read the VoiceKey quick start"
!define MUI_FINISHPAGE_LINK_LOCATION "$INSTDIR\README.md"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${PROJECT_ROOT}\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

Function .onInit
  install_check_running:
    FindWindow $0 "VoiceKeyControllerWindow"
    StrCmp $0 0 install_ready
    IfSilent install_silent_running
    MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION \
      "VoiceKey is currently running.$\r$\n$\r$\nQuit VoiceKey from its tray menu, then select Retry. Setup will not close an active dictation automatically." \
      IDRETRY install_check_running IDCANCEL install_cancel
  install_cancel:
    Abort
  install_silent_running:
    SetErrorLevel 2
    Quit
  install_ready:
FunctionEnd

Function un.onInit
  uninstall_check_running:
    FindWindow $0 "VoiceKeyControllerWindow"
    StrCmp $0 0 uninstall_ready
    IfSilent uninstall_silent_running
    MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION \
      "VoiceKey is currently running.$\r$\n$\r$\nQuit VoiceKey from its tray menu, then select Retry before uninstalling." \
      IDRETRY uninstall_check_running IDCANCEL uninstall_cancel
  uninstall_cancel:
    Abort
  uninstall_silent_running:
    SetErrorLevel 2
    Quit
  uninstall_ready:
FunctionEnd

Section "VoiceKey (required)" SEC_CORE
  SectionIn RO
  SetOutPath "$INSTDIR"
  File "${DIST_DIR}\voicekey.exe"
  File "${DIST_DIR}\README.md"
  File "${DIST_DIR}\LICENSE"
  File "${DIST_DIR}\THIRD_PARTY_NOTICES.md"

  SetOutPath "$INSTDIR\models"
  File "${DIST_DIR}\models\ggml-base-q8_0.bin"

  SetOutPath "$INSTDIR"
  WriteUninstaller "$INSTDIR\Uninstall VoiceKey.exe"
  WriteRegStr HKCU "Software\VoiceKey" "InstallDirectory" "$INSTDIR"

  CreateDirectory "$SMPROGRAMS\VoiceKey"
  CreateShortcut "$SMPROGRAMS\VoiceKey\VoiceKey.lnk" "$INSTDIR\${PRODUCT_EXE}" "" "$INSTDIR\${PRODUCT_EXE}" 0
  CreateShortcut "$SMPROGRAMS\VoiceKey\Uninstall VoiceKey.lnk" "$INSTDIR\Uninstall VoiceKey.exe" "" "$INSTDIR\Uninstall VoiceKey.exe" 0

  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayName" "VoiceKey"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\${PRODUCT_EXE},0"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "UninstallString" '$\"$INSTDIR\Uninstall VoiceKey.exe$\"'
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoRepair" 1
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "EstimatedSize" 85000
SectionEnd

Section "Launch VoiceKey when I sign in" SEC_STARTUP
  SectionIn 1
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "VoiceKey" '$\"$INSTDIR\${PRODUCT_EXE}$\" --background'
SectionEnd

Section /o "Desktop shortcut" SEC_DESKTOP
  CreateShortcut "$DESKTOP\VoiceKey.lnk" "$INSTDIR\${PRODUCT_EXE}" "" "$INSTDIR\${PRODUCT_EXE}" 0
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_CORE} "Install VoiceKey, its private speech model, and Start Menu shortcuts."
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_STARTUP} "Keep VoiceKey ready in the system tray after you sign in."
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DESKTOP} "Add a VoiceKey shortcut to your desktop."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "Uninstall"
  Delete "$DESKTOP\VoiceKey.lnk"
  Delete "$SMPROGRAMS\VoiceKey\VoiceKey.lnk"
  Delete "$SMPROGRAMS\VoiceKey\Uninstall VoiceKey.lnk"
  RMDir "$SMPROGRAMS\VoiceKey"
  DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "VoiceKey"
  DeleteRegKey HKCU "${UNINSTALL_KEY}"
  DeleteRegKey HKCU "Software\VoiceKey"
  RMDir /r "$INSTDIR"
SectionEnd
