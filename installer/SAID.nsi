Unicode true
RequestExecutionLevel user
SetCompressor /SOLID lzma

!ifndef PROJECT_ROOT
  !define PROJECT_ROOT ".."
!endif
!ifndef DIST_DIR
  !define DIST_DIR "${PROJECT_ROOT}/dist/SAID"
!endif
!ifndef PRODUCT_VERSION
  !define PRODUCT_VERSION "0.4.0"
!endif
!ifndef OUTPUT_FILE
  !define OUTPUT_FILE "${PROJECT_ROOT}/dist/SAID-Setup-${PRODUCT_VERSION}.exe"
!endif

!define PRODUCT_NAME "SAID"
!define PRODUCT_PUBLISHER "SAID"
!define PRODUCT_EXE "said.exe"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\SAID"
!define LEGACY_UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\VoiceKey"
!define RUN_KEY "Software\Microsoft\Windows\CurrentVersion\Run"

Name "${PRODUCT_NAME}"
OutFile "${OUTPUT_FILE}"
InstallDir "$LOCALAPPDATA\Programs\SAID"
InstallDirRegKey HKCU "Software\SAID" "InstallDirectory"
Icon "${PROJECT_ROOT}/assets/said.ico"
UninstallIcon "${PROJECT_ROOT}/assets/said.ico"
BrandingText "Local voice. Your text."
ShowInstDetails show
ShowUninstDetails show

VIProductVersion "${PRODUCT_VERSION}.0"
VIAddVersionKey /LANG=1033 "ProductName" "SAID"
VIAddVersionKey /LANG=1033 "CompanyName" "SAID"
VIAddVersionKey /LANG=1033 "FileDescription" "SAID Setup"
VIAddVersionKey /LANG=1033 "FileVersion" "${PRODUCT_VERSION}"
VIAddVersionKey /LANG=1033 "ProductVersion" "${PRODUCT_VERSION}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "Copyright (c) 2026 SAID contributors"

!include "MUI2.nsh"
!include "Sections.nsh"

!define MUI_ABORTWARNING
!define MUI_ICON "${PROJECT_ROOT}/assets/said.ico"
!define MUI_UNICON "${PROJECT_ROOT}/assets/said.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "${PROJECT_ROOT}/assets/installer-welcome.bmp"
!define MUI_WELCOMEPAGE_TITLE "SAID, ready when you are"
!define MUI_WELCOMEPAGE_TEXT "Say it once.$\r$\n$\r$\nYour words land wherever the caret already is. Recognition stays on this computer.$\r$\n$\r$\nSetup takes about a minute."
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP "${PROJECT_ROOT}/assets/installer-header.bmp"
!define MUI_FINISHPAGE_RUN "$INSTDIR\${PRODUCT_EXE}"
!define MUI_FINISHPAGE_RUN_PARAMETERS "--onboarding"
!define MUI_FINISHPAGE_RUN_TEXT "Open SAID and finish setup"
!define MUI_FINISHPAGE_TITLE "SAID is ready."
!define MUI_FINISHPAGE_TEXT "SAID and its local speech model are installed. Clean output is included. Adapt is an optional 639 MB local-model download you can choose during setup."
!define MUI_FINISHPAGE_LINK "Read the SAID quick start"
!define MUI_FINISHPAGE_LINK_LOCATION "$INSTDIR\README.md"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${PROJECT_ROOT}/LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

Var ExistingInstallDir
Var LegacyInstallDir

Section "SAID (required)" SEC_CORE
  SectionIn RO
  SetOutPath "$INSTDIR"
  File "${DIST_DIR}/said.exe"
  File /nonfatal "${DIST_DIR}/onnxruntime.dll"
  File /nonfatal "${DIST_DIR}/onnxruntime_providers_shared.dll"
  File "${DIST_DIR}/README.md"
  File "${DIST_DIR}/LICENSE"
  File "${DIST_DIR}/THIRD_PARTY_NOTICES.md"

  SetOutPath "$INSTDIR\third_party"
  File "${DIST_DIR}/third_party/Apache-2.0.txt"
  File "${DIST_DIR}/third_party/FunASR-MODEL-LICENSE-1.1.txt"

  IfFileExists "$LOCALAPPDATA\SAID\models\sense-voice-small.int8.onnx" 0 install_model_bundle
  IfFileExists "$LOCALAPPDATA\SAID\models\sense-voice-small.tokens.txt" 0 install_model_bundle
  IfFileExists "$LOCALAPPDATA\SAID\models\ct-transformer-punctuation.int8.onnx" 0 install_model_bundle
  IfFileExists "$LOCALAPPDATA\SAID\models\silero-vad.onnx" model_bundle_ready install_model_bundle
  install_model_bundle:
  SetOutPath "$LOCALAPPDATA\SAID\models"
  File /oname=sense-voice-small.int8.onnx "${DIST_DIR}/models/sense-voice-small.int8.onnx"
  File /oname=sense-voice-small.tokens.txt "${DIST_DIR}/models/sense-voice-small.tokens.txt"
  File /oname=ct-transformer-punctuation.int8.onnx "${DIST_DIR}/models/ct-transformer-punctuation.int8.onnx"
  File /oname=silero-vad.onnx "${DIST_DIR}/models/silero-vad.onnx"
  model_bundle_ready:

  SetOutPath "$INSTDIR"
  WriteUninstaller "$INSTDIR\Uninstall SAID.exe"
  WriteRegStr HKCU "Software\SAID" "InstallDirectory" "$INSTDIR"

  CreateDirectory "$SMPROGRAMS\SAID"
  CreateShortcut "$SMPROGRAMS\SAID\SAID.lnk" "$INSTDIR\${PRODUCT_EXE}" "" "$INSTDIR\${PRODUCT_EXE}" 0
  CreateShortcut "$SMPROGRAMS\SAID\Uninstall SAID.lnk" "$INSTDIR\Uninstall SAID.exe" "" "$INSTDIR\Uninstall SAID.exe" 0

  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayName" "SAID"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\${PRODUCT_EXE},0"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "UninstallString" '$\"$INSTDIR\Uninstall SAID.exe$\"'
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoRepair" 1
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "EstimatedSize" 360000

  StrCmp $LegacyInstallDir "" legacy_cleanup_done
    Delete "$DESKTOP\VoiceKey.lnk"
    Delete "$SMPROGRAMS\VoiceKey\VoiceKey.lnk"
    Delete "$SMPROGRAMS\VoiceKey\Uninstall VoiceKey.lnk"
    RMDir "$SMPROGRAMS\VoiceKey"
    Delete "$LegacyInstallDir\voicekey.exe"
    Delete "$LegacyInstallDir\README.md"
    Delete "$LegacyInstallDir\LICENSE"
    Delete "$LegacyInstallDir\THIRD_PARTY_NOTICES.md"
    Delete "$LegacyInstallDir\Uninstall VoiceKey.exe"
    RMDir "$LegacyInstallDir"
    DeleteRegKey HKCU "${LEGACY_UNINSTALL_KEY}"
  legacy_cleanup_done:
SectionEnd

Section "Start SAID at sign-in" SEC_STARTUP
  SectionIn 1
  WriteRegStr HKCU "${RUN_KEY}" "SAID" '$\"$INSTDIR\${PRODUCT_EXE}$\" --background'
SectionEnd

Section /o "Desktop shortcut" SEC_DESKTOP
  CreateShortcut "$DESKTOP\SAID.lnk" "$INSTDIR\${PRODUCT_EXE}" "" "$INSTDIR\${PRODUCT_EXE}" 0
SectionEnd

Section -IdentityMigration
  DeleteRegValue HKCU "${RUN_KEY}" "VoiceKey"
  SectionGetFlags ${SEC_STARTUP} $0
  IntOp $0 $0 & ${SF_SELECTED}
  StrCmp $0 0 0 identity_migration_done
    DeleteRegValue HKCU "${RUN_KEY}" "SAID"
  identity_migration_done:
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_CORE} "Install SAID, its local speech model, bundled Clean output, and Start Menu shortcuts."
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_STARTUP} "Keep SAID ready in the system tray after you sign in."
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DESKTOP} "Add a SAID shortcut to your desktop."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "Uninstall"
  Delete "$DESKTOP\SAID.lnk"
  Delete "$SMPROGRAMS\SAID\SAID.lnk"
  Delete "$SMPROGRAMS\SAID\Uninstall SAID.lnk"
  RMDir "$SMPROGRAMS\SAID"
  DeleteRegValue HKCU "${RUN_KEY}" "SAID"
  DeleteRegKey HKCU "${UNINSTALL_KEY}"

  Delete "$INSTDIR\said.exe"
  Delete "$INSTDIR\onnxruntime.dll"
  Delete "$INSTDIR\onnxruntime_providers_shared.dll"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\THIRD_PARTY_NOTICES.md"
  Delete "$INSTDIR\third_party\Apache-2.0.txt"
  Delete "$INSTDIR\third_party\FunASR-MODEL-LICENSE-1.1.txt"
  RMDir "$INSTDIR\third_party"
  Delete "$INSTDIR\Uninstall SAID.exe"
  RMDir "$INSTDIR"
SectionEnd

Function .onInit
  install_check_said:
    FindWindow $0 "SAIDControllerWindow"
    StrCmp $0 0 install_check_legacy
    Goto install_running
  install_check_legacy:
    FindWindow $0 "VoiceKeyControllerWindow"
    StrCmp $0 0 install_read_existing
  install_running:
    IfSilent install_silent_running
    MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION \
      "A currently installed version is running.$\r$\n$\r$\nQuit it from its tray menu, then select Retry. Setup will not close an active dictation automatically." \
      IDRETRY install_check_said IDCANCEL install_cancel
  install_cancel:
    Abort
  install_silent_running:
    SetErrorLevel 2
    Quit

  install_read_existing:
    ReadRegStr $ExistingInstallDir HKCU "Software\SAID" "InstallDirectory"
    ReadRegStr $LegacyInstallDir HKCU "Software\VoiceKey" "InstallDirectory"
    StrCmp $ExistingInstallDir "" 0 startup_read_said
    StrCmp $LegacyInstallDir "" install_ready startup_read_legacy

  startup_read_said:
    ReadRegStr $1 HKCU "${RUN_KEY}" "SAID"
    Goto startup_apply
  startup_read_legacy:
    ReadRegStr $1 HKCU "${RUN_KEY}" "VoiceKey"
  startup_apply:
    StrCmp $1 "" startup_disable startup_enable
  startup_disable:
    SectionGetFlags ${SEC_STARTUP} $2
    IntOp $2 $2 & ${SECTION_OFF}
    SectionSetFlags ${SEC_STARTUP} $2
    Goto install_ready
  startup_enable:
    SectionGetFlags ${SEC_STARTUP} $2
    IntOp $2 $2 | ${SF_SELECTED}
    SectionSetFlags ${SEC_STARTUP} $2
  install_ready:
FunctionEnd

Function un.onInit
  uninstall_check_running:
    FindWindow $0 "SAIDControllerWindow"
    StrCmp $0 0 uninstall_ready
    IfSilent uninstall_silent_running
    MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION \
      "SAID is currently running.$\r$\n$\r$\nQuit SAID from its tray menu, then select Retry before uninstalling." \
      IDRETRY uninstall_check_running IDCANCEL uninstall_cancel
  uninstall_cancel:
    Abort
  uninstall_silent_running:
    SetErrorLevel 2
    Quit
  uninstall_ready:
FunctionEnd
