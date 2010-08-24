; First is default
LoadLanguageFile "${NSISDIR}\Contrib\Language files\Danish.nlf"

; Language selection dialog
LangString InstallerLanguageTitle  ${LANG_DANISH} "Installationssprog"
LangString SelectInstallerLanguage  ${LANG_DANISH} "Vælg venligst sprog til installation"

; subtitle on license text caption
LangString LicenseSubTitleUpdate ${LANG_DANISH} " Opdater"
LangString LicenseSubTitleSetup ${LANG_DANISH} " Opsætning"

; installation directory text
LangString DirectoryChooseTitle ${LANG_DANISH} "Installationsmappe" 
LangString DirectoryChooseUpdate ${LANG_DANISH} "Vælg Second Life mappe til opdatering til version ${VERSION_LONG}.(XXX):"
LangString DirectoryChooseSetup ${LANG_DANISH} "Vælg mappe hvor Second Life skal installeres:"

; CheckStartupParams message box
LangString CheckStartupParamsMB ${LANG_DANISH} "Kunne ikke finde programmet '$INSTPROG'. Baggrundsopdatering fejlede."

; installation success dialog
LangString InstSuccesssQuestion ${LANG_DANISH} "Start Second Life nu?"

; remove old NSIS version
LangString RemoveOldNSISVersion ${LANG_DANISH} "Checker ældre version..."

; check windows version
LangString CheckWindowsVersionDP ${LANG_DANISH} "Checker Windows version..."
LangString CheckWindowsVersionMB ${LANG_DANISH} 'Second Life supporterer kun Windows XP, Windows 2000, og Mac OS X.$\n$\nForsøg på installation på Windows $R0 kan resultere i nedbrud og datatab.$\n$\nInstallér alligevel?'

; checkifadministrator function (install)
LangString CheckAdministratorInstDP ${LANG_DANISH} "Checker rettigheder til installation..."
LangString CheckAdministratorInstMB ${LANG_DANISH} 'Det ser ud til at du benytter en konto med begrænsninger.$\nDu skal have "administrator" rettigheder for at installere Second Life.'

; checkifadministrator function (uninstall)
LangString CheckAdministratorUnInstDP ${LANG_DANISH} "Checker rettigheder til at afinstallere..."
LangString CheckAdministratorUnInstMB ${LANG_DANISH} 'Det ser ud til at du benytter en konto med begrænsninger.$\nDu skal have "administrator" rettigheder for at afinstallere Second Life.'

; checkifalreadycurrent
LangString CheckIfCurrentMB ${LANG_DANISH} "Det ser ud til at Second Life ${VERSION_LONG} allerede er installeret.$\n$\nØnsker du at installere igen?"

; closesecondlife function (install)
LangString CloseSecondLifeInstDP ${LANG_DANISH} "Venter på at Second Life skal lukke ned..."
LangString CloseSecondLifeInstMB ${LANG_DANISH} "Second Life kan ikke installeres mens programmet kører.$\n$\nAfslut programmet for at fortsætte.$\nVælg ANNULÉR for at afbryde installation."

; closesecondlife function (uninstall)
LangString CloseSecondLifeUnInstDP ${LANG_DANISH} "Venter på at Second Life skal lukke ned..."
LangString CloseSecondLifeUnInstMB ${LANG_DANISH} "Second Life kan ikke afinstalleres mens programmet kører.$\n$\nAfslut programmet for at fortsætte.$\nVælg ANNULÉR for at afbryde installation."

; CheckNetworkConnection
LangString CheckNetworkConnectionDP ${LANG_DANISH} "Checker netværksforbindelse..."

; removecachefiles
LangString RemoveCacheFilesDP ${LANG_DANISH} "Sletter cache filer i dokument mappen"

; delete program files
LangString DeleteProgramFilesMB ${LANG_DANISH} "Der er stadig filer i Second Life program mappen.$\n$\nDette er sandsynligvis filer du har oprettet eller flyttet til :$\n$INSTDIR$\n$\nØnsker du at fjerne disse filer?"

; uninstall text
LangString UninstallTextMsg ${LANG_DANISH} "Dette vil afinstallere Second Life ${VERSION_LONG} fra dit system."
