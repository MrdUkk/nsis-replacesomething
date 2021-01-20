; this is a demo for NSIS plug-in NSIS ReplaceSomething written by Wholesome Software
!include "MUI2.nsh"

ShowInstDetails show

Name "test"
OutFile "replacesomething.exe"
ManifestSupportedOS Win7

InstallDir "$TEMP\NSISReplacesomethingTest"

!define MUI_FINISHPAGE_NOAUTOCLOSE
!insertmacro MUI_PAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"


Section ""
  SetOutPath $INSTDIR
  File "in.txt"
  File "in.bin"

  ;for text (ANSI encoding only):
  ;params are [FULL_PATH_TO_FILE] [Case_Sensetive_Flag] [Search_String] [Replacement_String]
  ;FULL_PATH_TO_FILE    contains path where changes are maded.
  ;Case_Sensetive_Flag  is bool variable (0 and 1) indicating that we should ignore (or not) character case
  ;Search_String        what should we look for?
  ;Replacement_String   what to replace the string with
  NSISReplaceSomething::TextInFile "$INSTDIR\in.txt" 0 "FROMstr1" "tostr1"
  ;after call you should pop two statuses (integers)
  ;first one is internal error status (0=fine ,  less 0 encodes stage where error is occurs)
  ;second (and last) is normal Win32 GetLastError() result
  Pop $0 
  Pop $1 
  DetailPrint "returned '$0'"
  DetailPrint "GetLastError() '$1'"

  ;for binary:
  ;params are [FULL_PATH_TO_FILE] [Search_Hex_String] [Replacement_Hex_String]
  ;Hex strings are in format: AABBCCDD or AA BB CC 
  NSISReplaceSomething::BytesInFile "$INSTDIR\in.bin" "DEADBEEF" "AABBCC"
  ;after call you should pop two statuses (integers)
  ;first one is internal error status (0=fine , less 0 encodes stage where error is occurs)
  ;second (and last) is normal Win32 GetLastError() result
  Pop $0 
  Pop $1
  DetailPrint "returned '$0'"
  DetailPrint "GetLastError() '$1'"

SectionEnd

